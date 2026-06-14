// exp_sn_chol.cpp - M1 driver for the self-built supernodal Cholesky (research-only, NOT engine).
//
// This step verifies the SYMBOLIC phase of sn_chol.h:
//   * natural ordering -> my L fill must match Eigen SimplicialLDLT/NaturalOrdering EXACTLY
//     (same ordering + correct symbolic factorization => identical structure). This is the
//     ground-truth correctness gate.
//   * METIS ordering   -> my L fill vs Eigen SimplicialLDLT/MetisOrdering (fill should drop and
//     land in the same ballpark; confirms the METIS perm/iperm convention is right).
// Eigen's matrixL() omits the unit diagonal, so compare STRICT-lower nnz (mine - n).

#define NOMINMAX
#include <iostream>               // Eigen/MetisSupport.h references std::cerr without including it
#include "sn_chol.h"              // METIS + self-built symbolic
#include "research_common.h"      // FrameCore + Eigen + makeTower + reduceFF
#include <Eigen/SparseCholesky>
#include <Eigen/MetisSupport>
#include <cstdio>
#include <cstring>

using namespace research;

// 2-D 5-point Poisson on m x m grid -> SPD, known sparsity (structural sanity).
static SpMat poisson2D(int m) {
    const int n = m * m;
    std::vector<Triplet> t;
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

static long long eigenStrictLnnz_natural(const SpMat& K) {
    Eigen::SimplicialLDLT<SpMat, Eigen::Lower, Eigen::NaturalOrdering<int>> s;
    s.compute(K);
    return static_cast<long long>(s.matrixL().nestedExpression().nonZeros());   // already strict-lower
}
static long long eigenStrictLnnz_metis(const SpMat& K) {
    Eigen::SimplicialLDLT<SpMat, Eigen::Lower, Eigen::MetisOrdering<int>> s;
    s.compute(K);
    return static_cast<long long>(s.matrixL().nestedExpression().nonZeros());   // already strict-lower
}

static void testCase(const char* name, const SpMat& K) {
    const int n = static_cast<int>(K.rows());
    const int* op = K.outerIndexPtr();
    const int* oi = K.innerIndexPtr();

    // natural: must match Eigen natural EXACTLY (strict-lower)
    sn::SnSymbolic sNat = sn::analyze(n, op, oi, /*useMetis=*/false);
    const long long mineNat = sNat.Lnnz - n;
    const long long eigNat  = eigenStrictLnnz_natural(K);

    // metis: compare ballpark
    sn::SnSymbolic sMet = sn::analyze(n, op, oi, /*useMetis=*/true);
    const long long mineMet = sMet.Lnnz - n;
    const long long eigMet  = eigenStrictLnnz_metis(K);

    std::printf("[sn-sym] %-18s n=%6d nnzA=%lld\n", name, n, static_cast<long long>(K.nonZeros()));
    std::printf("         natural: mine=%lld eigen=%lld  %s\n",
                mineNat, eigNat, (mineNat == eigNat ? "EXACT MATCH" : "*** MISMATCH ***"));
    std::printf("         metis  : mine=%lld eigen=%lld  usedMetis=%d  (ratio mine/eigen=%.3f)\n",
                mineMet, eigMet, (int)sMet.usedMetis,
                eigMet > 0 ? (double)mineMet / (double)eigMet : 0.0);
    std::printf("         metis vs natural fill drop: %.1f%%\n",
                mineNat > 0 ? 100.0 * (1.0 - (double)mineMet / (double)mineNat) : 0.0);
}

int main(int argc, char** argv) {
    int nx = 8, ny = 6, st = 10;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : "0"; };
        if      (!std::strcmp(argv[i], "--nx"))      nx = std::atoi(next());
        else if (!std::strcmp(argv[i], "--ny"))      ny = std::atoi(next());
        else if (!std::strcmp(argv[i], "--stories")) st = std::atoi(next());
    }

    // 1) Poisson 2D (small, known structure)
    testCase("poisson2D-20", poisson2D(20));
    testCase("poisson2D-40", poisson2D(40));

    // 2) Real frame K_ff (makeTower) -- same K_ff path as exp_supernodal_compare
    {
        FrameModel m = makeTower(nx, ny, st);
        assertNodalOnly(m, "exp_sn_chol");
        PreparedSystem ps = assembleAndFactor(m);
        const PreparedSystem::Impl& S = *ps.impl;
        if (S.singular) { std::printf("[sn-sym] tower singular\n"); return 2; }
        const SpMat Kff = research::reduceFF(S.K, S.fmap, S.nf);
        char nm[64]; std::snprintf(nm, sizeof(nm), "tower(%d,%d,%d)", nx, ny, st);
        testCase(nm, Kff);
    }

    std::printf("[sn-sym] done\n");
    return 0;
}
