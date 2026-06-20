#pragma once
#include "FrameCore/FrameSolver.h"   // FRAMECORE_API, PreparedSystem (opaque PIMPL), real
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveResult.h"
#include <memory>
#include <string>

namespace frame {

// Options for a supernodal session. POD; no Eigen. enabled defaults TRUE: a session is built
// precisely to reuse ONE supernodal factor across many solveFrame calls (factor-once + solve-many,
// the production payoff of the self-built supernodal direct solver). The PreparedSystem's LDLT stays
// the oracle/fallback, so a disabled / non-SPD / not-compiled session returns the direct-solve result.
struct SnSessionOptions {
    bool enabled         = true;   // false => solveFrame is a drop-in equal to solveLoad (LDLT)
    bool useMetis        = true;   // METIS nested-dissection fill-reducing ordering
    int  amalgRelax      = 0;      // supernode amalgamation relax rows (0 = off; M3a had no net gain)
    int  amalgMaxCol     = 64;     // max columns per amalgamated supernode
    int  numThreads      = 0;      // 0 = sqrt(nf)/20 memory-bandwidth heuristic (recommendedThreads)
    int  blasThreadsRoot = 0;      // 0 = nt (mixed parallelism); 1 = single-thread BLAS everywhere
    bool fallbackOnFail  = true;   // supernodal non-finite -> fall back to LDLT
    // R2: Neumaier-compensated iterative refinement (see SnSolveOptions.h for rationale).
    // The session caches the K_ff CSC structure (~nnz*16B; tens of MB at 100k DOF) when irSteps>0
    // so compensated SpMV reuses the same matrix the factor was built from.
    int    irSteps = 0;            // 0 = off (bit-identical to no-IR); 1-2 typical
    double irTol   = 0.0;          // 0 = no early stop; >0 = break when ||r||_inf <= irTol * ||b||_inf

    // R2.2 lazy-recover (v2.8.2+): when true, solveFrame() returns u + reactions only;
    // R.memberForces / R.shellForces are left empty (size()==0). Callers reading those
    // post-solve MUST check .empty() and re-run solveFrame() with skipForceRecovery=false
    // to populate. Use when a per-frame interactive load drag only needs to render the
    // deformed mesh — Research/R2_realtime_150k/RESULTS_round1.md measured the
    // per-element recover pass at ~50-80 ms on a 90k frame tower, dominating the user-
    // facing solveFrame() time. Default false: bit-identical to v2.8.x SnSession behaviour.
    bool skipForceRecovery = false;
};

// R2.2 sub-stage timings of the LAST solveFrame() call. ALWAYS-ZERO unless the engine
// was built with -DSN_SESSION_TIMING=1 (a research-only diagnostic; main lane / production
// builds are zero-cost — the chrono pairs are #ifdef'd out and this struct stays all-zero).
// The Research/R2_realtime_150k bench reads these to break down the 90k frame-tower
// solveFrame budget (RESULTS_round1.md round 1 showed the user-facing solveFrame is
// dominated by RHS assembly + supernodal backsub + K*u SPMV + per-element recover, not by
// any single one — quantifying each is round 2's job and this is the hook).
struct SnSessionTimings {
    double rhsMs      = 0;   // total RHS: nodalLoads + equivalent loads + prescribed reduction
    double rhsEqMs    = 0;   // sub: el->addEquivalentNodalLoads loop over all elements
    double rhsKMs     = 0;   // sub: sparse-K column-iterator prescribed reduction (0 when fastpath skips)
    double backsubMs  = 0;   // sn::solveSuper itself (or LDLT fallback)
    double irMs       = 0;   // Neumaier IR loop (0 unless irSteps>0)
    double scatterMs  = 0;   // uf -> u global scatter
    double spmvMs     = 0;   // K*u - F for reactions
    double recoverMs  = 0;   // per-element recover() loop (0 in lazy mode)
    double totalMs    = 0;   // end-to-end solveFrame()
};

// Stateful supernodal solve session: factor ONCE in the ctor, reuse the factor across solveFrame
// calls. This is the production "factor-once + solve-many" mode of the self-built supernodal direct
// solver -- for a fixed structure re-solved each frame (the game niche), the supernodal factor is far
// cheaper than Eigen SimplicialLDLT (M2: ~27x at scale) and reuse amortizes it over the frames, while
// each solveFrame is a forward/back substitution. PIMPL; all Eigen / sn confined to the .cpp; the
// public header is POD; move-only.
//
// LIFETIME CONTRACT: the PreparedSystem MUST outlive the session (the session stores a non-owning
// pointer into its Impl, like std::span). Moving or destroying it while the session is alive is
// undefined behaviour. solveFrame re-checks the model fingerprint each call: a structural change since
// assembleAndFactor yields a singular SolveResult (re-run assembleAndFactor). When the supernodal
// factor is not ready (disabled / non-SPD / FRAMECORE_SUPERNODAL=0), solveFrame transparently uses
// the LDLT oracle, so the result never deviates from the direct solve.
//
// THREAD SAFETY (R2 audit SLV-01): the supernodal factor builds on OpenBLAS, which uses
// `openblas_set_num_threads(...)` as **process-global state**. A SnSession ctor mutates it; so
// does each solveFrame call when the supernodal path is taken. **Do not construct or call
// solveFrame() concurrently on multiple SnSession instances on the same process** — even if
// each instance has its own PreparedSystem, the OpenBLAS thread-count is shared. Serialise
// supernodal work with an external lock if your host is multi-threaded. The LDLT fallback
// inside solveFrame() has no such constraint, but you cannot rely on it being chosen.
class FRAMECORE_API SnSession {
public:
    explicit SnSession(const PreparedSystem& prepared, const SnSessionOptions& opts = {});
    ~SnSession();
    SnSession(SnSession&&) noexcept;
    SnSession& operator=(SnSession&&) noexcept;
    SnSession(const SnSession&) = delete;
    SnSession& operator=(const SnSession&) = delete;

    // The supernodal factor is built and SPD: solveFrame reuses it. When false (disabled / singular /
    // non-SPD / not compiled), solveFrame still returns the correct LDLT result. diagnostic() explains.
    bool valid() const;
    const std::string& diagnostic() const;

    // Per-frame solve of the model's NODAL loads + PRESCRIBED values, reusing the factor. The
    // SolveResult (u / reactions / member+shell forces) matches solveLoad to factorization round-off;
    // on a mechanism or fingerprint mismatch, SolveResult.singular.
    SolveResult solveFrame(const FrameModel& model);

    // R2.2 timings of the LAST solveFrame() call. All-zero on a default build (zero-cost);
    // populated when the engine was compiled with -DSN_SESSION_TIMING=1.
    SnSessionTimings lastTimings() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace frame
