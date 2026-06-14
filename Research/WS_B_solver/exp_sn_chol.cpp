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
#include <Eigen/CholmodSupport>   // numeric correctness oracle
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

// Mixed building (frame + one MITC4 shell slab per bay per floor); same construction as
// exp_supernodal_compare so the M2 benchmark uses the go/no-go matrix family.
static FrameModel makeMixedBuilding(int nx, int ny, int stories) {
    FrameModel m = makeTower(nx, ny, stories);
    m.materials[0].nu = 0.3;
    int sid = 0;
    for (int k = 1; k <= stories; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const int a = towerNodeId(i, j, k, nx, ny), b = towerNodeId(i + 1, j, k, nx, ny),
                          c = towerNodeId(i + 1, j + 1, k, nx, ny), d = towerNodeId(i, j + 1, k, nx, ny);
                m.shells.push_back(ShellQuad(sid++, a, b, c, d, 0, 200.0));
            }
    return m;
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

// Numeric correctness: self-built factor+solve vs CHOLMOD on the same K, RHS = ones.
static void testNumeric(const char* name, const SpMat& K) {
    const int n = static_cast<int>(K.rows());
    VecX F = VecX::Ones(n);

    sn::SnSymbolic sym = sn::analyze(n, K.outerIndexPtr(), K.innerIndexPtr(), /*useMetis=*/true);
    sn::SnFactor   fac = sn::factorize(n, K.outerIndexPtr(), K.innerIndexPtr(), K.valuePtr(), sym);
    VecX xmy(n);
    sn::solve(fac, sym, F.data(), xmy.data());
    const double res = static_cast<double>((K * xmy - F).norm() / std::max<real>(1e-300, F.norm()));

    Eigen::CholmodSupernodalLLT<SpMat> ch;
    ch.compute(K);
    VecX xch = ch.solve(F);
    const double diff = static_cast<double>((xmy - xch).norm() / std::max<real>(1e-300, xch.norm()));

    std::printf("[sn-num] %-18s n=%6d spd=%d res=%.2e vsCHOLMOD=%.2e  %s\n",
                name, n, (int)fac.spd, res, diff,
                (res <= 1e-9 && diff <= 1e-8) ? "PASS" : "*** FAIL ***");
}

// Supernodal BLAS3 path: correctness vs CHOLMOD + factor-time comparison (mine/cholmod).
static void testSuper(const char* name, const SpMat& K, int relax = 0) {
    const int n = static_cast<int>(K.rows());
    VecX F = VecX::Ones(n);

    sn::SnSymbolic sym = sn::analyze(n, K.outerIndexPtr(), K.innerIndexPtr(), /*useMetis=*/true);
    Timer tf;
    sn::SnSuper S = sn::factorizeSuper(n, K.outerIndexPtr(), K.innerIndexPtr(), K.valuePtr(), sym, relax);
    const double facMs = tf.ms();
    VecX xs(n);
    sn::solveSuper(S, sym, F.data(), xs.data());
    const double res0 = static_cast<double>((K * xs - F).norm() / std::max<real>(1e-300, F.norm()));
    // one step of iterative refinement (cheap: reuses the factor). Mixed-building K is
    // ill-conditioned (shell drilling / large spans) so the raw direct residual sits at ~1e-9
    // -- CHOLMOD has the same limit; refinement recovers 1e-9+.
    VecX rr = F - K * xs;
    VecX dx(n);
    sn::solveSuper(S, sym, rr.data(), dx.data());
    xs += dx;
    const double res1 = static_cast<double>((K * xs - F).norm() / std::max<real>(1e-300, F.norm()));

    Timer tc;
    Eigen::CholmodSupernodalLLT<SpMat> ch;
    ch.compute(K);
    const double chMs = tc.ms();
    VecX xc = ch.solve(F);
    const double diff = static_cast<double>((xs - xc).norm() / std::max<real>(1e-300, xc.norm()));

    std::printf("[sn-super] %-16s relax=%-2d n=%6d nsn=%d facMs=%.1f cholmod=%.1f (%.2fx) res1=%.2e vsCHOLMOD=%.2e  %s\n",
                name, relax, n, S.nsn, facMs, chMs, chMs > 0 ? facMs / chMs : 0.0, res1, diff,
                (res1 <= 1e-9 && diff <= 1e-8) ? "PASS" : "*** FAIL ***");
    std::fflush(stdout);   // survive an abort() from a downstream assert
}

int main(int argc, char** argv) {
    int nx = 8, ny = 6, st = 10;
    bool bigSweep = false;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : "0"; };
        if      (!std::strcmp(argv[i], "--nx"))       nx = std::atoi(next());
        else if (!std::strcmp(argv[i], "--ny"))       ny = std::atoi(next());
        else if (!std::strcmp(argv[i], "--stories"))  st = std::atoi(next());
        else if (!std::strcmp(argv[i], "--bigSweep")) bigSweep = true;
    }

    if (bigSweep) {   // large-scale mixed building: self-built supernodal vs CHOLMOD only
        FrameModel m = makeMixedBuilding(nx, ny, st);
        assertNodalOnly(m, "exp_sn_chol");
        PreparedSystem ps = assembleAndFactor(m);
        const PreparedSystem::Impl& S = *ps.impl;
        if (!S.singular) {
            const SpMat Kff = research::reduceFF(S.K, S.fmap, S.nf);
            char nm[64]; std::snprintf(nm, sizeof(nm), "mixed(%d,%d,%d)", nx, ny, st);
            testSuper(nm, Kff, 0);     // fundamental supernodes
            testSuper(nm, Kff, 16);    // amalgamated (relax=16)
            testSuper(nm, Kff, 48);    // amalgamated (relax=48)
        }
        return 0;
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

    // ---- numeric correctness vs CHOLMOD oracle ----
    testNumeric("poisson2D-20", poisson2D(20));
    testNumeric("poisson2D-40", poisson2D(40));
    {
        FrameModel m = makeTower(nx, ny, st);
        assertNodalOnly(m, "exp_sn_chol");
        PreparedSystem ps = assembleAndFactor(m);
        const PreparedSystem::Impl& S = *ps.impl;
        if (!S.singular) {
            const SpMat Kff = research::reduceFF(S.K, S.fmap, S.nf);
            testNumeric("tower-Kff", Kff);
        }
    }
    std::printf("[sn-num] done\n");

    // ---- supernodal BLAS3 vs CHOLMOD (correctness + factor timing) ----
    testSuper("poisson2D-40", poisson2D(40));
    {
        FrameModel m = makeTower(nx, ny, st);
        assertNodalOnly(m, "exp_sn_chol");
        PreparedSystem ps = assembleAndFactor(m);
        const PreparedSystem::Impl& S = *ps.impl;
        if (!S.singular) {
            const SpMat Kff = research::reduceFF(S.K, S.fmap, S.nf);
            testSuper("tower-Kff", Kff);
        }
    }
    // mixed-building small-scale: catches shell-specific symbolic/numeric/amalgamation bugs
    {
        FrameModel m = makeMixedBuilding(4, 3, 5);
        assertNodalOnly(m, "exp_sn_chol");
        PreparedSystem ps = assembleAndFactor(m);
        const PreparedSystem::Impl& S = *ps.impl;
        if (!S.singular) {
            const SpMat Kff = research::reduceFF(S.K, S.fmap, S.nf);
            testCase("mixed-small", Kff);       // symbolic vs Eigen
            testNumeric("mixed-small", Kff);    // column-based vs CHOLMOD
            testSuper("mixed-small", Kff, 0);   // supernodal fundamental
            testSuper("mixed-small", Kff, 16);  // supernodal amalgamated
        }
    }
    std::printf("[sn-super] done\n");
    return 0;
}
