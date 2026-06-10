#pragma once
#include "FrameCore/FrameSolver.h"
#include "FrameCore/BucklingResult.h"

namespace frame {

// POD options selecting the eigensolve path for solveBuckling (S1). The default keeps the
// historical DENSE GeneralizedSelfAdjointEigenSolver for small models; large models take the
// zero-dependency SPARSE subspace iteration (Private/SparseEigsolver.h) which reuses K_ff's
// already-computed LDLT instead of re-factoring. The sparse path is tolerance-level (NOT bit-
// identical to dense) and falls back to dense on any non-convergence, so correctness is never
// at risk. No Eigen on this public boundary — plain int/real only.
struct BucklingOptions {
    int  denseThreshold = 500;    // nf > this -> sparse path; nf <= this -> dense; <= 0 forces sparse
    int  nev            = 1;      // smallest eigenpairs to converge (criticalFactor = lambda[0])
    int  maxIter        = 300;    // sparse subspace-iteration cap before dense fallback
    real tol            = 1e-11;  // sparse relative-eigenvalue convergence tolerance
};

// Linear (eigenvalue) BUCKLING. Applies the model's reference load (via solveLoad), builds the
// geometric stiffness Kg from the resulting member axial forces, and finds the smallest factor
// lambda such that (K + lambda*Kg) is singular. P_cr = lambda * (applied reference load). For
// a slender column this reproduces the Euler load pi^2 EI/(KL)^2. Beam-column geometric
// stiffness only (shell geometric stiffness is a future addition). Tension-only members are
// ignored in Kg because they are stabilizing, not buckling sources. Linear buckling = the onset
// eigenvalue, NOT a nonlinear post-buckling path.
FRAMECORE_API BucklingResult solveBuckling(const PreparedSystem& prepared, const FrameModel& model);

// Same analysis with explicit options (sparse/dense dispatch per BucklingOptions). The no-options
// overload above delegates here with BucklingOptions{}. Identical result on small models; on large
// models the sparse path returns the same criticalFactor to tolerance (see BucklingOptions).
FRAMECORE_API BucklingResult solveBuckling(const PreparedSystem& prepared, const FrameModel& model,
                                           const BucklingOptions& opts);

}  // namespace frame
