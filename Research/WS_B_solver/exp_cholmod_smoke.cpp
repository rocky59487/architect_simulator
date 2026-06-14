// exp_cholmod_smoke.cpp — minimal CHOLMOD + METIS link smoke test (scratch, NOT engine).
//
// Purpose: de-risk the single biggest unknown of research-line R — can conda-forge's
// SuiteSparse(CHOLMOD) + METIS be compiled & linked against Eigen with MSVC at all?
// Deliberately depends ONLY on Eigen + CHOLMOD + METIS (no FrameCore, no obj_core), so a
// failure here is unambiguously a dependency/link problem, not an engine integration one.
//
// Builds a small SPD sparse matrix (2-D 5-point Poisson) and solves the same RHS three ways:
//   1. SimplicialLDLT / AMD          (Eigen built-in, zero new dep — sanity baseline)
//   2. SimplicialLDLT / MetisOrdering (isolates METIS link)
//   3. CholmodSupernodalLLT          (isolates CHOLMOD supernodal link)
// PASS = all three compile, link, run, and give relResidual <= 1e-9.

#include <iostream>               // Eigen/MetisSupport.h references std::cerr without including it
#include <Eigen/SparseCore>
#include <Eigen/SparseCholesky>   // SimplicialLDLT
#include <Eigen/CholmodSupport>   // CholmodSupernodalLLT
#include <Eigen/MetisSupport>     // MetisOrdering
#include <cstdio>
#include <vector>

using SpMat = Eigen::SparseMatrix<double>;   // col-major, int index — matches FrameCore SpMat
using Trip  = Eigen::Triplet<double>;
using Vec   = Eigen::VectorXd;

// 2-D 5-point Poisson on an m x m grid -> SPD, n = m*m. Stands in for K_ff structurally.
static SpMat poisson2D(int m) {
    const int n = m * m;
    std::vector<Trip> t;
    t.reserve(static_cast<size_t>(5) * n);
    auto id = [&](int i, int j) { return i * m + j; };
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < m; ++j) {
            const int k = id(i, j);
            t.emplace_back(k, k, 4.0);
            if (i > 0)     t.emplace_back(k, id(i - 1, j), -1.0);
            if (i < m - 1) t.emplace_back(k, id(i + 1, j), -1.0);
            if (j > 0)     t.emplace_back(k, id(i, j - 1), -1.0);
            if (j < m - 1) t.emplace_back(k, id(i, j + 1), -1.0);
        }
    SpMat A(n, n);
    A.setFromTriplets(t.begin(), t.end());
    A.makeCompressed();
    return A;
}

static double relRes(const SpMat& A, const Vec& x, const Vec& b) {
    return (A * x - b).norm() / std::max(1e-300, b.norm());
}

int main() {
    const SpMat A = poisson2D(40);   // n = 1600
    const Vec   b = Vec::Ones(A.rows());
    std::printf("[smoke] n=%lld nnz=%lld\n",
                static_cast<long long>(A.rows()), static_cast<long long>(A.nonZeros()));

    int bad = 0;
    {
        Eigen::SimplicialLDLT<SpMat> s; s.compute(A);
        const Vec x = s.solve(b);
        const double r = relRes(A, x, b);
        std::printf("[smoke] SimplicialLDLT/AMD     info=%d res=%.3e\n", (int)s.info(), r);
        if (s.info() != Eigen::Success || r > 1e-9) ++bad;
    }
    {
        Eigen::SimplicialLDLT<SpMat, Eigen::Lower, Eigen::MetisOrdering<int>> s; s.compute(A);
        const Vec x = s.solve(b);
        const double r = relRes(A, x, b);
        std::printf("[smoke] SimplicialLDLT/METIS   info=%d res=%.3e\n", (int)s.info(), r);
        if (s.info() != Eigen::Success || r > 1e-9) ++bad;
    }
    {
        Eigen::CholmodSupernodalLLT<SpMat> s; s.compute(A);
        const Vec x = s.solve(b);
        const double r = relRes(A, x, b);
        std::printf("[smoke] CholmodSupernodalLLT   info=%d res=%.3e\n", (int)s.info(), r);
        if (s.info() != Eigen::Success || r > 1e-9) ++bad;
    }

    std::printf(bad == 0 ? "[smoke] PASS (all 3 solvers link & solve)\n"
                         : "[smoke] FAIL (%d solver(s) bad)\n", bad);
    return bad == 0 ? 0 : 1;
}
