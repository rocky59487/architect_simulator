#pragma once
//
// Sparse generalized eigensolver via SUBSPACE (inverse) ITERATION — a pure-Eigen,
// zero-new-dependency scale-up path for the dense GeneralizedSelfAdjointEigenSolver used by
// modal/buckling analysis on small models. It computes the few smallest eigenvalues of
//
//        A x = lambda B x        (A symmetric positive-definite, B symmetric)
//
// reusing A's already-computed SimplicialLDLT (no re-factorization). The iteration drives a
// block of `p > nev` vectors through  Xbar = A^{-1} B X  (so it converges to the LARGEST
// eigenvalues of A^{-1}B, i.e. the SMALLEST lambda) and does a Rayleigh-Ritz projection each
// step. Only the n×p block is dense; A and B stay sparse and A^{-1} is the sparse LDLT solve.
//
// Honest scope: this is a standard subspace iteration with full Rayleigh-Ritz and guard
// vectors — validated against the dense path to give identical eigenvalues on the test models.
// It is NOT a hardened production eigensolver (no shift strategy for clustered/zero modes, no
// selective reorthogonalization); for the modest interactive models here it is exact, and it
// falls back cleanly (returns false) so the caller can use the dense path.
//
#include "FrameEigen.h"
#include <random>
#include <algorithm>
#include <cmath>

namespace frame {

// Smallest `nev` eigenpairs of  A x = lambda B x,  A SPD (given as the sparse A plus its LDLT),
// B symmetric sparse. On success fills `lambda` (ascending, size nev) and `vec` (n×nev, columns
// are the eigenvectors, A-normalized) and returns true. Returns false if the projected problem
// goes ill-conditioned or it fails to converge — the caller should fall back to the dense path.
// Optional `X0` seeds the leading columns of the iteration block (warm-start): pass the
// pre-event modes to converge the post-event modes in fewer iterations. Default nullptr keeps
// the deterministic random start -> bit-identical to the un-seeded path (modal/buckling oracles
// unaffected).
inline bool subspaceSmallest(const SpMat& A, const LDLTSolver& Ainv, const SpMat& B,
                             int nev, VecX& lambda, MatX& vec,
                             int maxIter = 300, real tol = 1e-11,
                             const MatX* X0 = nullptr) {
    const int n = static_cast<int>(A.rows());
    if (nev <= 0 || n <= 0 || nev > n) return false;
    const int p = std::min(n, std::max(nev + 4, 2 * nev));   // guard vectors aid convergence

    // Deterministic random start (fixed seed -> reproducible builds).
    std::mt19937 rng(20260607u);
    std::uniform_real_distribution<real> dist(real(-1), real(1));
    MatX X(n, p);
    for (int j = 0; j < p; ++j)
        for (int i = 0; i < n; ++i) X(i, j) = dist(rng);
    if (X0) {   // warm-start: overwrite the leading columns with the supplied block
        const int c = std::min<int>(p, static_cast<int>(X0->cols()));
        X.leftCols(c) = X0->leftCols(c);
    }

    VecX muPrev = VecX::Constant(p, real(1e30));   // mu = 1/lambda (largest -> smallest lambda)
    VecX mu     = VecX::Zero(p);
    MatX Xconv  = X;                                // last accepted Ritz block
    bool converged = false;

    for (int iter = 0; iter < maxIter; ++iter) {
        const MatX Y    = B * X;            // n×p
        const MatX Xbar = Ainv.solve(Y);    // A^{-1} B X   (sparse LDLT solve)
        if (Ainv.info() != Eigen::Success) return false;

        // Rayleigh-Ritz in span(Xbar):  solve  (Xbar^T B Xbar) q = mu (Xbar^T A Xbar) q.
        const MatX Ar = Xbar.transpose() * (A * Xbar);   // p×p SPD
        const MatX Br = Xbar.transpose() * (B * Xbar);   // p×p sym
        Eigen::GeneralizedSelfAdjointEigenSolver<MatX> es(Br, Ar);
        if (es.info() != Eigen::Success) return false;
        mu = es.eigenvalues();              // ascending; the LARGEST mu = smallest lambda
        const MatX V = es.eigenvectors();

        X = Xbar * V;                        // Ritz vectors
        // A-normalize columns so the projected A-block stays well-scaled.
        const MatX AX = A * X;
        for (int j = 0; j < p; ++j) {
            const real q = X.col(j).dot(AX.col(j));
            const real nrm = (q > 0) ? std::sqrt(q) : real(0);
            if (nrm > 0) X.col(j) /= nrm;
        }
        Xconv = X;

        // Convergence on the `nev` largest mu (the smallest lambdas), which sit at the TOP.
        real maxRel = 0;
        for (int k = 0; k < nev; ++k) {
            const int idx = p - 1 - k;
            const real d = std::abs(mu(idx) - muPrev(idx)) / std::max<real>(1e-30, std::abs(mu(idx)));
            maxRel = std::max(maxRel, d);
        }
        if (maxRel < tol) { converged = true; break; }
        muPrev = mu;
    }
    if (!converged) return false;

    // Emit the nev smallest lambda = 1/mu (largest mu), ascending in lambda.
    lambda.resize(nev);
    vec.resize(n, nev);
    for (int k = 0; k < nev; ++k) {
        const int idx = p - 1 - k;                       // largest mu -> smallest lambda
        const real m = mu(idx);
        lambda(k) = (m > 0) ? real(1) / m : real(0);     // lambda = 1/mu
        vec.col(k) = Xconv.col(idx);
    }
    return true;
}

}  // namespace frame
