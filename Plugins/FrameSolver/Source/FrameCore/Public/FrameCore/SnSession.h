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

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace frame
