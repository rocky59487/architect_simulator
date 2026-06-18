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

    // R2.1 audit AC-06 architectural fix: thin-shell linear buckling eigenvalues are an
    // *upper bound* on the real critical load. Imperfections, post-buckling softening, and
    // edge effects pull the practical capacity down — for thin-walled cylinders EN 1993-1-6
    // Table 6.1 gives knockdown alpha ~= 0.6-0.7 for ordinary fabrication (alpha_x ~= 0.62 at
    // r/t = 500 for Class C quality). Set this to the relevant design-code knockdown to get
    // a USABLE design value back from solveBuckling instead of just a paper eigenvalue. 0
    // (default) keeps the un-knocked-down eigenvalue (bit-identical to v2.0). Typical:
    //   0.65 for general thin shells (NASA SP-8007 lower bound for axially compressed cylinders)
    //   0.70 for moderate-quality fabrication
    //   1.00 for stocky shells where imperfection sensitivity is low
    // The result reports BOTH the un-knocked value (`reportedCriticalFactor`) and the
    // knocked-down value (`criticalFactor`), so the user always sees the source data and
    // the design value side by side. Knockdown is applied AFTER the eigensolve, so this
    // does not change the bit-identity contract for default-options callers.
    real shellBucklingKnockdown = 0;
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
