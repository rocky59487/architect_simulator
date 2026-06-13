#pragma once
#include "FrameCore/FrameSolver.h"   // FRAMECORE_API, FrameModel, PreparedSystem, SolveOptions, SolveResult, MemberId, real
#include <memory>
#include <string>

namespace frame {

// Options for ReSolveSession (S1 ReSolve ladder). POD boundary; SolveOptions threads through to
// the baseline assembleAndFactor / rebaseline (e.g. enableReleases, useTimoshenko, pivotTol).
struct ReanalysisOptions {
    int  maxRank      = 96;      // Tier-1 cumulative rank cap (~16 beams); above may use Tier-2
    real pcgTol       = 1e-10;   // Tier-2 relative residual target
    int  pcgMaxIter   = 500;     // Tier-2 PCG iteration cap
    bool allowTier2   = true;    // false: above maxRank goes straight to Tier-3
    real mechPivotTol = 1e-10;   // Tier-1 capacitance |piv|min/|piv|max below this -> mechanism
    SolveOptions solve;          // threaded into the baseline assembleAndFactor / rebaseline
};

// Per-solve diagnostics.
struct ReanalysisStats {
    int  tier        = 0;        // tier taken: 0 = no increment, 1 = Woodbury, 2 = PCG, 3 = rebaseline
    int  rank        = 0;        // current accumulated low-rank update rank
    int  pcgIters    = 0;        // Tier-2 iterations, 0 outside Tier-2
    real relResidual = 0;        // Tier-2 relative residual, 0 outside Tier-2
    bool refactored  = false;    // this solve triggered a rebaseline (Tier-3)
    bool mechanism   = false;    // Tier-1 capacitance singular (the removed set forms a mechanism)
};

// Incremental re-analysis after element (member/shell) deactivate/restore, REUSING the baseline
// LDLᵀ factorization instead of a fresh assembleAndFactor — the interactive / collapse-driver path.
//
//   Tier-0  no change        -> baseline back-substitution.
//   Tier-1  rank <= maxRank   -> EXACT Woodbury low-rank update on the baseline factor; the
//                                capacitance matrix is singular  <=>  K' is singular  <=>  the
//                                removed set made a mechanism (detected from the factor, not topology).
//   Tier-2  rank <= 2*maxRank -> stale-LDLT-preconditioned PCG on assembled K'_ff (tolerance-grade).
//   Tier-3  larger/failed PCG  -> rebaseline (fresh assembleAndFactor on the current active set),
//                                which is always correct.
//
// The caller's model is never mutated (an internal working copy carries the active flags). Honest
// scope: SAME-TOPOLOGY increments only — the node set, support flags, and material/section VALUES
// must be unchanged (those need a fresh assembleAndFactor). Tier-1 is exact; Tier-2 is deterministic
// and tolerance-grade; Tier-3 is the always-correct fallback.
class FRAMECORE_API ReSolveSession {
public:
    explicit ReSolveSession(const FrameModel& base, const ReanalysisOptions& opts = {});
    ~ReSolveSession();
    ReSolveSession(ReSolveSession&&) noexcept;
    ReSolveSession& operator=(ReSolveSession&&) noexcept;
    ReSolveSession(const ReSolveSession&) = delete;
    ReSolveSession& operator=(const ReSolveSession&) = delete;

    bool valid() const;                          // baseline validate() passed AND non-singular
    const std::string& diagnostic() const;       // reason when !valid()

    // Toggle an element's activity. Returns false for an unknown id; a no-op (same as current
    // state) returns true. Each state change appends one signed low-rank update to the ladder.
    bool setMemberActive(MemberId id, bool active);
    bool setShellActive(int shellId, bool active);

    // Solve the CURRENT active set. The result (u / reactions / member+shell forces) matches a fresh
    // assembleAndFactor+solveLoad to factorization round-off; on a mechanism, SolveResult.singular.
    SolveResult solve(ReanalysisStats* stats = nullptr);

    // Force Tier-3: rebuild the baseline factorization on the current active set and clear the ladder.
    void rebaseline();

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

} // namespace frame
