// SizeOpt.h — fully-stressed design (FSD) member sizing via stress-ratio resizing.
//
// Iteratively rescales each member's cross-section so its worst-case (over both ends and all load
// cases) elastic Demand/Capacity approaches 1:  A <- max(Amin, A * maxDC). Sections are rescaled by
// SIMILARITY (shape preserved: I ~ A^2, W ~ A^1.5, ...), so any starting section works and the
// classic square-bar truss benchmark reproduces the literature optimum.
//
// Honest scope: a heuristic, not a true optimizer. For a statically DETERMINATE structure the
// fully-stressed point IS the minimum-weight optimum (member forces are area-independent); for a
// statically INDETERMINATE structure forces redistribute and FSD converges to a fully-stressed
// FIXED POINT that is NOT guaranteed globally optimal (WS_H). No LTB / local buckling / displacement
// constraints. The inner re-solves are FRESH factorizations -- resizing changes K's VALUES, which is
// outside the S1 ReSolve same-topology assumption, so FSD does not reuse a factorization.
//
// Pure POD/std public API (no Eigen, no UE). Mirrors the engine's existing analysis modules.

#pragma once
#include "FrameCore/FrameModel.h"
#include "FrameCore/Section.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/SolveOptions.h"
#include "FrameCore/Load.h"
#include <vector>

namespace frame {

// One alternative load set for multi-case FSD (the member size is driven by the worst D/C across
// cases -- the standard envelope rule). An empty SizeOptOptions::cases list = a single case taken
// from the model's own nodalLoads / memberUDLs.
struct SizeOptLoadCase {
    std::vector<NodalLoad> nodalLoads;
    std::vector<MemberUDL> memberUDLs;
};

struct SizeOptOptions {
    int  maxIter = 40;          // iteration cap before reporting non-convergence
    real dcTol   = 1e-8;        // fully-stressed tolerance: max over RESIZED members of |D/C - 1|
    real Amin    = 0;           // lower bound on area (clamp). 0 lets slender members shrink toward
                                // zero -> a mechanism; set > 0 for indeterminate structures.
    SolveOptions solve;         // threaded into every assembleAndFactor / solve

    // Optional ascending discrete section-AREA table. Empty = continuous A. When set, each member's
    // area is snapped UP (round-up: the smallest table area >= the continuous target -- conservative)
    // after each resize; a transition-hash cycle guard breaks any two-section oscillation (finite
    // termination), exactly as the S4 tension-only driver does for its active set.
    std::vector<real> sectionTable;

    // Multi-case load sets. Empty = a single case = the model's own loads.
    std::vector<SizeOptLoadCase> cases;
};

struct SizeOptResult {
    std::vector<real>    finalAreas;     // per member (model.members order); unchanged for non-sizable
    std::vector<Section> finalSections;  // per member: the resized section (copy of original otherwise)
    std::vector<real>    finalDC;        // per member: D/C at convergence (fully-stressed members ~ 1)
    std::vector<real>    dcHistory;      // worst D/C across sizable members, per iteration
    std::vector<real>    weightHistory;  // material VOLUME sum_e A_e * L_e (mm^3) -- a density-free
                                          // weight proxy, monotone-ish decreasing toward the optimum
    bool converged  = false;
    bool cycled     = false;             // discrete-table oscillation guard tripped -> finite-terminated
    bool singular   = false;             // a solve hit a mechanism (e.g. Amin too small)
    bool invalidDemand = false;          // non-finite D/C (e.g. zero allowable capacity under demand)
    int  iterations = 0;
};

// Fully-stressed design. `sizableMembers` lists the member INDICES to resize (empty = every active
// member with a valid material/section). The caller's model is never mutated -- the resized sections
// come back in the result. See the header note for the honest scope / ReSolve caveat.
FRAMECORE_API SizeOptResult runSizeOptimization(const FrameModel& model,
                                                const SizeOptOptions& opts,
                                                const std::vector<int>& sizableMembers = {});

}  // namespace frame
