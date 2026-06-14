#pragma once
#include "FrameCore/FrameSolver.h"   // FRAMECORE_API, PreparedSystem (opaque PIMPL), real
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveResult.h"
#include <memory>
#include <string>
#include <vector>

namespace frame {

// Options for the seeded HP-FEM session (A2). POD boundary; no Eigen. The defaults make the
// session an active HP lane (enabled=true) — unlike the one-shot solveLoadHP, an HpSession is
// constructed precisely to accelerate a fixed structure under repeated low-dimensional loads
// (the game niche: gravity + a few contacts). The PreparedSystem's LDLT factorization always
// remains the oracle and the safety net, so a disabled / un-seeded / non-converging frame
// returns exactly the direct-solve result.
struct HpSessionOptions {
    int  basisMax       = 64;      // seeded-basis cap; setLoadBasis auto-expands to fit all seeds
    real projGateTol    = 1e-6;    // initialRel below this => in-subspace (research: in 9.7e-11 vs out 4.1e-2)
    real pcgTol         = 1e-10;   // PCG convergence target on the relative residual ||r||/||b||
    int  pcgMaxIter     = 500;     // out-of-subspace full-PCG iteration cap
    int  pcgWarmIter    = 3;       // in-subspace warm-start PCG cap (residual safety net atop x0)
    bool fallbackOnFail = true;    // non-convergence / indefinite -> fall back to LDLT
    bool enabled        = true;    // false => solveFrame is a drop-in equal to solveLoad (LDLT)
    int  threads        = 1;       // >1 spawns a persistent pool that parallelizes the element apply
                                   // + block6 Jacobi (the A2a large-problem win); 1 = serial (A2c)
};

// Per-frame diagnostics (which path ran, how hard it worked). POD.
struct HpSessionStats {
    bool usedProjection = false;   // seeded Galerkin projection (in-subspace; ~0 PCG iters)
    bool usedPcg        = false;   // out-of-subspace / un-seeded full PCG from the projected x0
    bool usedLdlt       = false;   // LDLT oracle (disabled / empty basis / shell / non-converge)
    int  pcgIters       = 0;       // PCG iterations taken on this frame
    real initialRel     = 0;       // ||Ff - K x0|| / ||Ff|| of the seeded initial guess
    int  basisSize      = 0;       // current seeded-basis dimension
    bool usedCoarse     = false;   // the A2b coarse-grid correction was active in this frame's PCG
};

// Result of prepareCoarse (A2b). POD. A genuine multi-storey tower yields banded==true with
// floors>1; a 1D / arbitrary-topology model degrades gracefully (floors==1 or built==false -> the
// block6 / scalar preconditioner carries the solve unaided).
struct HpCoarseInfo {
    bool built  = false;   // a usable coarse correction was built and is now active
    bool banded = false;   // block-tridiagonal block-Thomas factor (else dense fallback)
    int  floors = 0;       // number of aggregated z-levels (>1 = genuine multi-storey structure)
    int  dim    = 0;       // coarse DOF count
};

// Stateful seeded HP-FEM solve session (A2c). Models ReSolveSession: PIMPL, all Eigen confined
// to the .cpp Impl, Eigen-free public header, move-only, POD options/stats. It holds a NON-owning
// pointer into the PreparedSystem's Impl plus its own A-orthonormal seeded load-response basis and
// matrix-free apply / Jacobi components.
//
//   - setLoadBasis(loads): one-time seeding. Each 6N global load vector is reduced and solved via
//     the (already-factored) LDLT, then A-orthonormalized into the basis. The seeds span the load
//     family the structure will see each frame (gravity + candidate contacts).
//   - solveFrame(model): the per-frame solve. Reuses the SAME RHS assembly / reactions / element
//     recovery as solveLoad, replacing only the linear solve: a Galerkin projection onto the seeded
//     basis gives a near-exact initial guess (in-subspace => ~0 PCG iters); a load that leaves the
//     subspace runs a full PCG; either way the LDLT factor is the always-correct fallback.
//
// The element apply + preconditioner are SERIAL by default (threads=1, the A2c lane) and frame-only
// — a shell element routes every frame to LDLT. Set opts.threads > 1 to spawn a persistent thread
// pool that parallelizes the element apply (and the block6 Jacobi): this is A2a, where the per-frame
// ~14ms serial apply at nf~18720 drops to ~0.9ms across 16 threads, making the seeded ~19x real.
// Serial threads=1 has no large-problem win over reused LDLT (~14ms apply > ~10.9ms LDLT backsub);
// it delivers the API, correctness, and the small-problem (nf<~2000) speedup. The seeded basis and
// the LDLT stay single-threaded (Eigen is not thread-safe); the pool only parallelizes the
// element-local apply (per-thread accumulation + touched-DOF reduction) and the block6 map.
//
// LIFETIME CONTRACT: the PreparedSystem MUST outlive the session (the session stores a non-owning
// pointer into it, like std::span). Moving or destroying it while the session is alive is undefined
// behaviour. solveFrame re-checks the model fingerprint each call: a structural change since
// assembleAndFactor yields a singular SolveResult (re-run assembleAndFactor).
class FRAMECORE_API HpSession {
public:
    explicit HpSession(const PreparedSystem& prepared, const HpSessionOptions& opts = {});
    ~HpSession();
    HpSession(HpSession&&) noexcept;
    HpSession& operator=(HpSession&&) noexcept;
    HpSession(const HpSession&) = delete;
    HpSession& operator=(const HpSession&) = delete;

    // HP acceleration is ready: the PreparedSystem is non-null, non-singular and frame-only. When
    // false (shell present / singular), solveFrame still returns the correct LDLT result — it just
    // never takes the accelerated path. diagnostic() explains a false.
    bool valid() const;
    const std::string& diagnostic() const;

    // Seed the load-response basis. Each entry is a 6N global load vector (the nodal-load pattern a
    // frame may apply; prescribed displacements are assumed zero in the seeds — fold a settlement
    // response into the per-frame model). Returns false if the session is not valid() or a vector is
    // not 6N. Rebuilds the basis, auto-expanding the cap to hold every seed (FIFO eviction would
    // otherwise silently drop early seeds and break the in-subspace guarantee).
    bool setLoadBasis(const std::vector<std::vector<real>>& loadVectors);

    // Optionally build a coarse-grid correction for the preconditioner (A2b): floor-aggregation by
    // node z (one 6-DOF coarse group per z-level) + a block-Thomas (banded) factor when the
    // aggregated coarse matrix is floor-block-tridiagonal (a tower), else a dense factor. This only
    // accelerates OUT-of-subspace PCG; in-subspace frames are 0-iter and never touch the
    // preconditioner. A 1D / arbitrary-topology model degrades gracefully (the block6/scalar Jacobi
    // carries the solve). Requires valid() and nf%6==0. The model must be structurally identical to
    // assembleAndFactor (its node coordinates define the aggregation). Idempotent; opt-in — a session
    // that never calls this just uses the plain block6/scalar preconditioner.
    HpCoarseInfo prepareCoarse(const FrameModel& model);

    // Solve the CURRENT model's NODAL loads + PRESCRIBED values, reusing the factorization. The
    // SolveResult (u / reactions / member+shell forces) matches solveLoad to tolerance (PCG tol /
    // factorization round-off); on a mechanism or fingerprint mismatch, SolveResult.singular.
    SolveResult solveFrame(const FrameModel& model, HpSessionStats* stats = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace frame
