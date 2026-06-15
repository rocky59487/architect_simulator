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
#include <windows.h>              // Phase E: peak working-set via K32GetProcessMemoryInfo (kernel32, no psapi.lib)
#include <psapi.h>

using namespace research;

// ---- Phase E (limit sweep) helpers ----------------------------------------------------------
// Peak resident set in MB. K32GetProcessMemoryInfo lives in kernel32 -> no psapi.lib link needed.
static double peakWorkingSetMB() {
    PROCESS_MEMORY_COUNTERS pmc{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
    return -1.0;
}

// Neumaier-compensated residual r = F - K*x in DOUBLE -- genuine extra precision on double hardware
// (MSVC long double is only 64-bit, so it gives no extra precision; compensated summation does).
// Used to test whether a high-precision residual breaks the fixed-precision cond floor. K is
// column-major (Eigen default): outer index = column j, inner = row.
static void compResidual(const SpMat& K, const VecX& x, const VecX& F, VecX& r) {
    const int n = (int)K.rows();
    std::vector<double> s((size_t)n, 0.0), c((size_t)n, 0.0);   // running sum + compensation
    for (int j = 0; j < K.outerSize(); ++j)
        for (SpMat::InnerIterator it(K, j); it; ++it) {
            const int i = it.row();
            const double term = it.value() * x(j);
            const double t = s[(size_t)i] + term;
            c[(size_t)i] += (std::fabs(s[(size_t)i]) >= std::fabs(term)) ? ((s[(size_t)i] - t) + term)
                                                                         : ((term - t) + s[(size_t)i]);
            s[(size_t)i] = t;
        }
    r.resize(n);
    for (int i = 0; i < n; ++i) r(i) = F(i) - (s[(size_t)i] + c[(size_t)i]);
}

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

// v3 baseline: a PURE-shell 2D-manifold model (n x n clamped square plate, all MITC4 quads, no frame)
// -- the sparse structure of a free-form shell building is a 2D manifold embedded in 3D, expected to
// fill in like O(N^1.5) (lower exponent than the mixed frame+shell building). All edge nodes are
// clamped (6 DOF fixed) so K_ff is SPD; interior nodes keep the full 6 DOF -> nf ~ 6*(n-1)^2.
static FrameModel makeShellGrid(int n) {
    FrameModel m;
    Material mat(30000.0, 11538.0); mat.nu = 0.3;
    m.materials.push_back(mat);
    const double a = 1000.0, h = a / n;
    auto gid = [n](int i, int j) { return j * (n + 1) + i; };
    for (int j = 0; j <= n; ++j)
        for (int i = 0; i <= n; ++i) {
            Node nd(gid(i, j), i * h, j * h, 0.0);
            if (i == 0 || i == n || j == 0 || j == n)
                for (int d = 0; d < 6; ++d) nd.fixed[d] = true;   // clamped edges -> SPD K_ff
            m.nodes.push_back(nd);
        }
    int sid = 0;
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
            m.shells.push_back(ShellQuad(sid++, gid(i, j), gid(i + 1, j), gid(i + 1, j + 1), gid(i, j + 1), 0, 10.0));
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

static double medianMs(std::vector<double> v) {        // caller has already dropped the warm-up run
    std::sort(v.begin(), v.end());
    return v.empty() ? 0.0 : v[v.size() / 2];
}

// M3b: serial vs parallel supernodal factor -- two gates in one report.
//  * stage-1 correctness: BOTH forced to single-thread BLAS -> result must be BIT-EXACT (memcmp
//    every panel). Any diff = a real race/scheduling bug, not FP reassociation from BLAS threads.
//  * stage-2 perf: parallel runs MIXED (narrow root levels use multi-thread BLAS), so it is NOT
//    bit-exact -- reported as a tolerance ||par-ser||/||ser|| + factor-time speedup vs serial/CHOLMOD.
static void testSuperParallel(const char* name, const SpMat& K, int relax, int nt) {
    const int n = static_cast<int>(K.rows());
    const int* op = K.outerIndexPtr(); const int* oi = K.innerIndexPtr(); const double* va = K.valuePtr();
    VecX F = VecX::Ones(n);
    sn::SnSymbolic sym = sn::analyze(n, op, oi, /*useMetis=*/true);
    const int ntUsed = sn::recommendedThreads(n, nt);   // actual thread count (heuristic when nt<=0)
    const int prevBlas = openblas_get_num_threads();

    // ---- stage-1: single-thread BLAS everywhere -> bit-exact serial vs parallel ----
    openblas_set_num_threads(1);
    sn::SnSuper Sser = sn::factorizeSuper(n, op, oi, va, sym, relax);
    sn::SnSuper Spar = sn::factorizeSuperParallel(n, op, oi, va, sym, relax, /*amalgMaxCol=*/64, /*numThreads=*/nt, /*blasThreadsRoot=*/1);
    long long bitDiffs = 0, valDiffs = 0;          // bit-level vs value-level (race) differences
    if (Sser.nsn != Spar.nsn) valDiffs = -1;       // structural mismatch (must not happen: same setup)
    else for (int s = 0; s < Sser.nsn; ++s) {
        if (Sser.data[s].size() != Spar.data[s].size()) { valDiffs++; continue; }
        for (size_t i = 0; i < Sser.data[s].size(); ++i)
            if (std::memcmp(&Sser.data[s][i], &Spar.data[s][i], sizeof(double)) != 0) {
                ++bitDiffs;
                if (Sser.data[s][i] != Spar.data[s][i]) ++valDiffs;   // != is false for +0.0 vs -0.0
            }
    }
    // parallel-factor correctness: solve + one step of iterative refinement, residual + vs CHOLMOD.
    // The mixed-building K is ill-conditioned (shell drilling / large spans): refinement reaches
    // ~5e-10 by 32k, but at 64k it floors near ~1.4e-9 -- the fixed-precision refinement limit
    // (~cond*eps), NOT a factor bug. CHOLMOD on the same K floors identically; the vsCHOLMOD figure
    // below (parallel solve vs the independent oracle) is the real numeric gate, not the residual.
    VecX xs(n); sn::solveSuper(Spar, sym, F.data(), xs.data());
    VecX rr = F - K * xs; VecX dx(n); sn::solveSuper(Spar, sym, rr.data(), dx.data()); xs += dx;
    const double res1 = static_cast<double>((K * xs - F).norm() / std::max<real>(1e-300, F.norm()));
    openblas_set_num_threads(prevBlas);
    Eigen::CholmodSupernodalLLT<SpMat> ch; ch.compute(K);
    VecX xc = ch.solve(F);
    const double diff = static_cast<double>((xs - xc).norm() / std::max<real>(1e-300, xc.norm()));

    // ---- stage-2: mixed parallelism -- tolerance vs serial oracle + factor timing ----
    sn::SnSuper SserT = sn::factorizeSuper(n, op, oi, va, sym, relax);                 // default BLAS
    sn::SnSuper SparT = sn::factorizeSuperParallel(n, op, oi, va, sym, relax, 64, nt, 0);  // mixed (root multi-BLAS)
    double num = 0, den = 0;
    if (SserT.nsn == SparT.nsn)
        for (int s = 0; s < SserT.nsn; ++s)
            for (size_t i = 0; i < SserT.data[s].size(); ++i) {
                const double d = SparT.data[s][i] - SserT.data[s][i];
                num += d * d; den += SserT.data[s][i] * SserT.data[s][i];
            }
    const double rel = den > 0 ? std::sqrt(num / den) : 0.0;

    std::vector<double> ts, tp, tc;
    for (int it = 0; it < 6; ++it) {   // 5 timed + 1 warm-up dropped
        { Timer t; sn::SnSuper z = sn::factorizeSuper(n, op, oi, va, sym, relax);                if (it) ts.push_back(t.ms()); (void)z; }
        { Timer t; sn::SnSuper z = sn::factorizeSuperParallel(n, op, oi, va, sym, relax, 64, nt, 0); if (it) tp.push_back(t.ms()); (void)z; }
        { Timer t; Eigen::CholmodSupernodalLLT<SpMat> c; c.compute(K);                           if (it) tc.push_back(t.ms()); }
    }
    const double serMs = medianMs(ts), parMs = medianMs(tp), chMs = medianMs(tc);

    // Gate = no race (bit-exact serial-vs-parallel) + parallel solve matches CHOLMOD (vsCHOLMOD).
    // res is reported for transparency and flagged "(cond)" when the conditioning floor keeps it
    // above 1e-9 -- that is a property of K, identical for CHOLMOD, not a parallel-factor failure.
    std::printf("[sn-par] %-15s relax=%-2d nt=%d nsn=%d | ser=%.1f par=%.1f sp=%.2fx | cholmod=%.1f vsCH=%.2fx | bitdiff=%lld valdiff=%lld rel=%.1e res=%.2e%s vsCHOLMOD=%.2e  %s\n",
                name, relax, ntUsed, Spar.nsn, serMs, parMs, parMs > 0 ? serMs / parMs : 0.0,
                chMs, chMs > 0 ? parMs / chMs : 0.0, bitDiffs, valDiffs, rel, res1,
                res1 <= 1e-9 ? "" : "(cond)", diff,
                (valDiffs == 0 && diff <= 1e-8) ? "PASS" : "*** FAIL ***");
    std::fflush(stdout);
}

// Phase E limit sweep: one scale, one process. factor time / per-frame backsub time / peak resident
// memory / residual at double vs long-double (mixed-precision) refinement. Validates the reachable
// edge (factor extrapolation, per-frame backsub <= ~100ms real-time ceiling, peak-memory wall) and
// tests whether 80-bit residual refinement breaks the fixed-precision cond floor (~1.4e-9 at 64k).
static void testSuperLimit(const char* name, const SpMat& K, int relax, int nt) {
    const int n = (int)K.rows();
    const int* op = K.outerIndexPtr(); const int* oi = K.innerIndexPtr(); const double* va = K.valuePtr();
    VecX F = VecX::Ones(n);
    sn::SnSymbolic sym = sn::analyze(n, op, oi, /*useMetis=*/true);

    // factor (parallel) timing: median of 2 timed (1 warm-up dropped); keep the last factor.
    std::vector<double> tf;
    sn::SnSuper S = sn::factorizeSuperParallel(n, op, oi, va, sym, relax, 64, nt, 0);   // warm-up (dropped)
    for (int it = 0; it < 2; ++it) { Timer t; S = sn::factorizeSuperParallel(n, op, oi, va, sym, relax, 64, nt, 0); tf.push_back(t.ms()); }
    const double facMs = medianMs(tf);

    // per-frame backsub timing: median of 7 reused-factor solves (the game-engine per-frame cost).
    std::vector<double> tb;
    VecX x(n);
    for (int it = 0; it < 8; ++it) { Timer t; sn::solveSuper(S, sym, F.data(), x.data()); if (it) tb.push_back(t.ms()); }
    const double bsMs = medianMs(tb);

    // residual: raw, 1-step double refinement, 3-step compensated-residual refinement (high-precision
    // residual in double, double solve) -- tests whether residual precision is the cond-floor cause.
    sn::solveSuper(S, sym, F.data(), x.data());
    const double res0 = static_cast<double>((K * x - F).norm() / std::max<real>(1e-300, F.norm()));
    VecX xd = x; { VecX rr = F - K * xd; VecX dx(n); sn::solveSuper(S, sym, rr.data(), dx.data()); xd += dx; }
    const double resD = static_cast<double>((K * xd - F).norm() / std::max<real>(1e-300, F.norm()));
    VecX xc = x, rr;
    for (int s = 0; s < 3; ++s) { compResidual(K, xc, F, rr); VecX dx(n); sn::solveSuper(S, sym, rr.data(), dx.data()); xc += dx; }
    VecX rfin; compResidual(K, xc, F, rfin);
    const double resComp = static_cast<double>(rfin.norm() / std::max<real>(1e-300, F.norm()));
    const double peakMB = peakWorkingSetMB();

    std::printf("[sn-limit] %-13s relax=%-2d nt=%d n=%6d nsn=%d | facMs=%.1f backsubMs=%.3f peakMB=%.0f | res0=%.2e resD(1x)=%.2e resComp(3x)=%.2e\n",
                name, relax, sn::recommendedThreads(n, nt), n, S.nsn, facMs, bsMs, peakMB, res0, resD, resComp);
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    int nx = 8, ny = 6, st = 10;
    int nt = 0;                 // 0 = hardware_concurrency (M3b parallel factor)
    bool bigSweep = false, limitMode = false, shellMode = false;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : "0"; };
        if      (!std::strcmp(argv[i], "--nx"))       nx = std::atoi(next());
        else if (!std::strcmp(argv[i], "--ny"))       ny = std::atoi(next());
        else if (!std::strcmp(argv[i], "--stories"))  st = std::atoi(next());
        else if (!std::strcmp(argv[i], "--threads"))  nt = std::atoi(next());
        else if (!std::strcmp(argv[i], "--bigSweep")) bigSweep = true;
        else if (!std::strcmp(argv[i], "--limit"))    limitMode = true;
        else if (!std::strcmp(argv[i], "--shell"))    shellMode = true;   // v3: pure-shell grid (nx = grid n)
    }

    if (limitMode) {   // Phase E / v3: reachable-edge limit sweep on ONE scale (one process/scale)
        FrameModel m;
        SolveOptions so;
        char nm[64];
        if (shellMode) {                                 // v3: PURE-shell 2D-manifold model
            m = makeShellGrid(nx);
            so.skipLdltFactor = true;                    // supernodal factors K_ff itself -> past the SimplicialLDLT wall
            std::snprintf(nm, sizeof(nm), "shell(%dx%d)", nx, nx);
        } else {
            m = makeMixedBuilding(nx, ny, st);
            std::snprintf(nm, sizeof(nm), "mixed(%d,%d,%d)", nx, ny, st);
        }
        assertNodalOnly(m, "exp_sn_chol");
        PreparedSystem ps = assembleAndFactor(m, so);
        const PreparedSystem::Impl& S = *ps.impl;
        if (!S.singular && S.nf > 0) {
            const SpMat Kff = research::reduceFF(S.K, S.fmap, S.nf);
            testSuperLimit(nm, Kff, 16, nt);
        } else {
            std::printf("[sn-limit] %s singular/empty -- skipped\n", nm);
        }
        return 0;
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
            std::printf("[sn-par] --- M3b parallel factor (mixed building) ---\n");
            testSuperParallel(nm, Kff, 16, nt);   // amalgamated panels feed the workers best
            testSuperParallel(nm, Kff, 48, nt);
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
            testSuperParallel("mixed-small", Kff, 16, nt);   // M3b parallel correctness (shell bugs)
        }
    }
    // M3b: parallel factor correctness + perf on the real frame K_ff
    {
        FrameModel m = makeTower(nx, ny, st);
        assertNodalOnly(m, "exp_sn_chol");
        PreparedSystem ps = assembleAndFactor(m);
        const PreparedSystem::Impl& S = *ps.impl;
        if (!S.singular) {
            const SpMat Kff = research::reduceFF(S.K, S.fmap, S.nf);
            std::printf("[sn-par] --- M3b parallel factor ---\n");
            testSuperParallel("tower-Kff", Kff, 0, nt);
            testSuperParallel("tower-Kff", Kff, 16, nt);
        }
    }
    std::printf("[sn-super] done\n");
    return 0;
}
