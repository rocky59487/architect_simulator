// exp_supernodal_compare.cpp — research-line R (real-time million-DOF): can a SINGLE
// direct solver scale, i.e. is the Eigen-SimplicialLDLT factor wall a "fake wall"?
// Compares, on the SAME mixed-building (frame + MITC4 shell floors) K_ff / F_ff:
//   1. SimplicialLDLT / AMD    (engine default = v1 baseline, the origin to beat)
//   2. SimplicialLDLT / METIS  (isolates ORDERING power; nested dissection)
//   3. CholmodSupernodalLLT    (isolates SUPERNODAL power; BLAS3 + CHOLMOD's own ND)
// Per solver: factor time, median backsub time, relResidual (must be <=1e-9), and for the
// two Eigen orderings the L-factor nnz (clean fill metric). One scale per process so the
// peak-memory reading stays clean; a PowerShell driver loops sizes. NOT engine, NOT a gate.
//
// Build: Research\WS_B_solver\build_supernodal.bat compare   (needs Research\obj_core)

#define NOMINMAX
#include <iostream>               // Eigen/MetisSupport.h references std::cerr without including it
#include "research_common.h"      // makeTower, Timer, reduceFF, reduceVec, nodalLoadVector, SpMat, VecX
#include <Eigen/CholmodSupport>   // CholmodSupernodalLLT
#include <Eigen/MetisSupport>     // MetisOrdering
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#include <algorithm>
#include <cstring>
#include <vector>

using namespace research;

namespace {

double privMiB() {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        return static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    return 0;
}
double peakMiB() {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        return static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    return 0;
}

double relResidual(const SpMat& K, const VecX& x, const VecX& F) {
    return static_cast<double>((K * x - F).norm() / std::max<real>(1e-300, F.norm()));
}

// Median backsub over `reps` reuses of an already-factored solver; reports final residual.
template <class Solver>
double medianBacksubMs(Solver& s, const VecX& F, int reps, const SpMat& K, double& resOut) {
    std::vector<double> t;
    t.reserve(static_cast<size_t>(reps));
    VecX x;
    for (int i = 0; i < reps; ++i) {
        Timer tt;
        x = s.solve(F);
        t.push_back(tt.ms());
    }
    std::sort(t.begin(), t.end());
    resOut = relResidual(K, x, F);
    return t[t.size() / 2];
}

// Mixed building: makeTower's frame + one MITC4 shell facet per bay per floor (slabs).
// Loads stay nodal (makeTower's wind+gravity) — no shell pressure — so the research
// nodal-only helpers (reduceVec/nodalLoadVector/assertNodalOnly) remain valid. The shells
// densify K_ff toward the conservative (shell-heavy) fill regime of a real building.
FrameModel makeMixedBuilding(int nx, int ny, int stories) {
    FrameModel m = makeTower(nx, ny, stories);
    m.materials[0].nu = 0.3;     // shell bending/membrane constitutive needs Poisson (G already implies 0.3)
    int sid = 0;
    for (int k = 1; k <= stories; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const int a = towerNodeId(i,     j,     k, nx, ny);
                const int b = towerNodeId(i + 1, j,     k, nx, ny);
                const int c = towerNodeId(i + 1, j + 1, k, nx, ny);
                const int d = towerNodeId(i,     j + 1, k, nx, ny);
                m.shells.push_back(ShellQuad(sid++, a, b, c, d, 0, 200.0));   // CCW from +Z, 200 mm slab
            }
    return m;
}

} // namespace

int main(int argc, char** argv) {
    int nx = 8, ny = 6, st = 10, reps = 51;
    bool frameOnly = false;
    bool skipSimplicial = false;   // skip the two Eigen simplicial paths (too slow at large nf)
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : "0"; };
        if      (!std::strcmp(argv[i], "--nx"))        nx = std::atoi(next());
        else if (!std::strcmp(argv[i], "--ny"))        ny = std::atoi(next());
        else if (!std::strcmp(argv[i], "--stories"))   st = std::atoi(next());
        else if (!std::strcmp(argv[i], "--reps"))      reps = std::atoi(next());
        else if (!std::strcmp(argv[i], "--frameOnly")) frameOnly = true;
        else if (!std::strcmp(argv[i], "--skipSimplicial")) skipSimplicial = true;
    }
    if (reps < 1) reps = 1;

    FrameModel model = frameOnly ? makeTower(nx, ny, st) : makeMixedBuilding(nx, ny, st);
    assertNodalOnly(model, "exp_supernodal_compare");

    try {
        PreparedSystem ps = assembleAndFactor(model);          // engine assembly path (used only to get K, fmap)
        const PreparedSystem::Impl& S = *ps.impl;
        if (S.singular) { std::printf("[scale-fail] singular: %s\n", S.diagnostic.c_str()); return 2; }

        const SpMat Kff = research::reduceFF(S.K, S.fmap, S.nf);
        const VecX  Fff = reduceVec(nodalLoadVector(model), S.fmap, S.nf);

        std::printf("[scale] mixed=%d nx=%d ny=%d st=%d nodes=%zu members=%zu shells=%zu nf=%d nnzKff=%lld peakMiB=%.0f\n",
                    frameOnly ? 0 : 1, nx, ny, st, model.nodes.size(), model.members.size(),
                    model.shells.size(), S.nf, static_cast<long long>(Kff.nonZeros()), peakMiB());
        std::fflush(stdout);

      if (!skipSimplicial) {
        // 1. SimplicialLDLT / AMD  (v1 baseline origin)
        {
            const double m0 = privMiB();
            Eigen::SimplicialLDLT<SpMat> s;
            Timer tf; s.compute(Kff); const double fMs = tf.ms();
            if (s.info() != Eigen::Success) { std::printf("[solver] AMD    FACTOR-FAIL info=%d\n", (int)s.info()); }
            else {
                const double dM = privMiB() - m0;
                double res; const double bMs = medianBacksubMs(s, Fff, reps, Kff, res);
                std::printf("[solver] AMD    factorMs=%.1f backsubMs=%.3f res=%.2e Lnnz=%lld dPrivMiB=%.0f\n",
                            fMs, bMs, res, static_cast<long long>(s.matrixL().nestedExpression().nonZeros()), dM);
            }
            std::fflush(stdout);
        }
        // 2. SimplicialLDLT / METIS  (ordering power)
        {
            const double m0 = privMiB();
            Eigen::SimplicialLDLT<SpMat, Eigen::Lower, Eigen::MetisOrdering<int>> s;
            Timer tf; s.compute(Kff); const double fMs = tf.ms();
            if (s.info() != Eigen::Success) { std::printf("[solver] METIS  FACTOR-FAIL info=%d\n", (int)s.info()); }
            else {
                const double dM = privMiB() - m0;
                double res; const double bMs = medianBacksubMs(s, Fff, reps, Kff, res);
                std::printf("[solver] METIS  factorMs=%.1f backsubMs=%.3f res=%.2e Lnnz=%lld dPrivMiB=%.0f\n",
                            fMs, bMs, res, static_cast<long long>(s.matrixL().nestedExpression().nonZeros()), dM);
            }
            std::fflush(stdout);
        }
      } // !skipSimplicial
        // 3. CholmodSupernodalLLT  (supernodal power; CHOLMOD's own fill-reducing ND)
        {
            const double m0 = privMiB();
            Eigen::CholmodSupernodalLLT<SpMat> s;
            Timer tf; s.compute(Kff); const double fMs = tf.ms();
            if (s.info() != Eigen::Success) { std::printf("[solver] CHOLMOD FACTOR-FAIL info=%d\n", (int)s.info()); }
            else {
                const double dM = privMiB() - m0;
                double res; const double bMs = medianBacksubMs(s, Fff, reps, Kff, res);
                std::printf("[solver] CHOLMOD factorMs=%.1f backsubMs=%.3f res=%.2e Lnnz=NA dPrivMiB=%.0f\n",
                            fMs, bMs, res, dM);
            }
            std::fflush(stdout);
        }

        std::printf("[done] nf=%d peakMiB=%.0f\n", S.nf, peakMiB());
        return 0;
    } catch (const std::bad_alloc&) {
        std::printf("[oom] nx=%d ny=%d st=%d\n", nx, ny, st);
        return 3;
    }
}
