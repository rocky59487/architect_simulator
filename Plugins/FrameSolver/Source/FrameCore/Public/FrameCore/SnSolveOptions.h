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
    int  amalgRelax      = 0;      // supernode amalgamation relax rows (M3a: 0 = off; no net speedup yet)
    int  amalgMaxCol     = 64;     // max columns per amalgamated supernode
    int  numThreads      = 0;      // 0 = sqrt(nf)/20 memory-bandwidth heuristic (recommendedThreads)
    int  blasThreadsRoot = 0;      // 0 = nt (mixed parallelism); 1 = single-thread BLAS everywhere
    bool fallbackOnFail  = true;   // non-SPD / non-finite / not compiled -> fall back to LDLT
};

}  // namespace frame
