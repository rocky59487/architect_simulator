#pragma once
#include "FrameCore/FrameTypes.h"

namespace frame {

// Options for the opt-in HP-FEM iterative solve lane (solveLoadHP). All defaults keep the
// lane inert: enabled must be set true to request the matrix-free PCG path. The LDLT
// factorization held by the PreparedSystem always remains the oracle and fallback, so a
// disabled or failing lane returns exactly the direct-solve result. POD only — no Eigen.
struct HpSolveOptions {
    bool enabled        = false;   // opt in: false => solveLoadHP behaves like solveLoad (LDLT)
    real pcgTol         = 1e-10;   // PCG convergence target on the relative residual ||r||/||b||
    int  pcgMaxIter     = 500;     // iteration cap before declaring non-convergence
    bool fallbackOnFail = true;    // non-convergence / shell / singular -> fall back to LDLT
};

}  // namespace frame
