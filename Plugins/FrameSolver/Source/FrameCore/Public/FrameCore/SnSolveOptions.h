#pragma once
#include "FrameCore/FrameTypes.h"

namespace frame {

// Options for the opt-in self-built supernodal Cholesky solve lane (solveLoadSupernodal / SnSession).
// All defaults keep the lane inert: enabled must be set true to request the supernodal factor. The
// LDLT factorization held by the PreparedSystem always remains the oracle and fallback, so a disabled
// or failing lane returns exactly the direct-solve result. POD only -- no Eigen.
struct SnSolveOptions {
    bool enabled         = false;  // opt in: false => solveLoadSupernodal behaves like solveLoad (LDLT)
    bool useMetis        = true;   // METIS nested-dissection fill-reducing ordering (off => natural)
    int  amalgRelax      = 0;      // supernode amalgamation relax rows (0 = off; measured no net gain)
    int  amalgMaxCol     = 64;     // max columns per amalgamated supernode
    int  numThreads      = 0;      // 0 = sqrt(nf)/20 memory-bandwidth heuristic (recommendedThreads)
    int  blasThreadsRoot = 0;      // 0 = nt (mixed parallelism); 1 = single-thread BLAS everywhere
    bool fallbackOnFail  = true;   // non-SPD / non-finite / not compiled -> fall back to LDLT
    // R2: Neumaier-compensated iterative refinement layered on top of the fixed-precision factor.
    // For mixed frame+shell topology the assembled K*u residual crosses 1e-9 around ~40k DOF
    // (sn_sweep.txt confirms 64k FAIL @ res=1.40e-9 with vsCHOLMOD=2.57e-12 -- solution itself
    // is essentially exact, residual is K*u rounding accumulation). Compensated SpMV breaks the
    // double-precision residual floor on MSVC (where long double == double, so naive long-double
    // residual is a no-op). irSteps=0 is bit-identical to no-IR.
    int    irSteps = 0;            // 0 = off; 1-2 typical; >0 enables compensated residual IR after backsub
    double irTol   = 0.0;          // 0 = no early stop; >0 = break when ||r||_inf <= irTol * ||b||_inf
};

}  // namespace frame
