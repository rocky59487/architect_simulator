// R2 realtime-150k micro-bench: split-stage timing on the production SnSession lane.
//
// Why this exists
// ---------------
// docs/PROGRESS_R_supernodal.md:77 quotes "150k DOF realtime via backsub <=100 ms" but
// the underlying data points are 17k / 32k / 64k mixed buildings extrapolated log-log.
// docs/specs/v3_memory_recon.md:62 shows a 113k point (CHOLMOD: factor 2625 ms, backsub
// 67 ms) and a 191k point. The two points bracket 150k but DO NOT pin down the
// realtime ceiling on the SELF-BUILT supernodal lane (the production code path), and the
// 60 fps target (16.67 ms) is well below any reported backsub number.
//
// So we run the production path (assembleAndFactor + SnSession + solveFrame) on three
// frame-tower scales (~90k / ~120k / ~160k free DOF) and split factor vs backsub. The
// SnSession contract is "factor ONCE in ctor, reuse in solveFrame", so:
//   * factor cost is paid once per topology change. We time the ctor.
//   * backsub cost is paid every frame. We time solveFrame() over repeat runs.
//
// Output
// ------
// One line per (case, repeat):
//   case={name} nf={n} factorMs={ms} backsub_med={ms} backsub_min={ms} backsub_p95={ms}
//     residRel={r} cs={checksum}
// followed by a verdict block:
//   60fps_budget=16.67ms  ok=Y/N  margin={ms}
//   30fps_budget=33.33ms  ok=Y/N  margin={ms}
// Exit code 0 unconditionally — this is a measurement program, not a gate. The verdict
// block is the human-facing answer to "did we hit 150k realtime?".
//
// Not included in main lane
// -------------------------
// This is research-only: not in any build.bat, not in run_gate.ps1, not in the standalone
// gate's $g_fail counter. Run by hand:
//   Research/R2_realtime_150k/build_r2.bat
//   Research/R2_realtime_150k/r2_bench.exe --preset 150k --repeat 50

#include "FrameCore/FrameSolver.h"
#include "FrameCore/Section.h"
#include "FrameCore/SnSession.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

using namespace frame;

namespace {

// Frame-tower fixture — identical geometry to Standalone/frame_perf.cpp so numbers can be
// cross-referenced. Columns + 2-way beams + braced 4 outer faces, fixed base. Pure frame
// (no shells), so the comparison is apples-to-apples with the v3_memory_recon mixed
// building entries (which carry shells; expect this bench to factor faster than mixed
// numbers at equal nf because shell sparsity adds more nz).

struct BenchSpec {
    int nx = 18;
    int ny = 12;
    int stories = 60;
    int repeat = 30;
    int warmup = 3;
    bool skipRecover = false;
    bool compareModes = false;   // run both modes back-to-back and print speedup
    bool useGpu = false;
    std::string name = "~90k";
};

int nodeId(int i, int j, int k, int nx, int ny) {
    return k * (nx + 1) * (ny + 1) + j * (nx + 1) + i;
}

Section squareSection(real side) {
    Section s = Section::Rectangular(side, side);
    s.J = 0.1406 * side * side * side * side;
    return s;
}

Vec3 refVecFor(const Vec3& pi, const Vec3& pj) {
    Vec3 x = pj - pi;
    const real n = norm(x);
    if (n > 0) x = x * (1.0 / n);
    return (std::abs(x.z) < 0.92) ? Vec3(0, 0, 1) : Vec3(1, 0, 0);
}

FrameModel makeTower(int nx, int ny, int stories) {
    constexpr real sx = 6000.0, sy = 5000.0, h = 3300.0;
    FrameModel m;
    m.materials.reserve(1);
    m.sections.reserve(3);
    m.nodes.reserve(static_cast<size_t>(nx + 1) * (ny + 1) * (stories + 1));

    m.materials.emplace_back(200000.0, 76923.07692307692, 7850.0);
    m.sections.push_back(squareSection(520.0));
    m.sections.push_back(squareSection(360.0));
    m.sections.push_back(squareSection(220.0));

    for (int k = 0; k <= stories; ++k) {
        const real driftX = 35.0 * k;
        const real driftY = -18.0 * k;
        for (int j = 0; j <= ny; ++j) {
            for (int i = 0; i <= nx; ++i) {
                Node n(nodeId(i, j, k, nx, ny), i * sx + driftX, j * sy + driftY, k * h);
                if (k == 0) n.fixAll();
                m.nodes.push_back(n);
            }
        }
    }

    auto nodePos = [&](int id) -> Vec3 { return m.nodes[static_cast<size_t>(id)].pos; };
    auto addMember = [&](int i, int j, int sec) {
        Member mem(static_cast<int>(m.members.size()), i, j, 0, sec);
        mem.refVec = refVecFor(nodePos(i), nodePos(j));
        m.members.push_back(mem);
    };

    const size_t columnCount = static_cast<size_t>(nx + 1) * (ny + 1) * stories;
    const size_t beamPerFloor = static_cast<size_t>(nx) * (ny + 1) + static_cast<size_t>(ny) * (nx + 1);
    const size_t bracePerStory = static_cast<size_t>(2 * nx + 2 * ny);
    m.members.reserve(columnCount + beamPerFloor * stories + bracePerStory * stories);

    for (int k = 0; k < stories; ++k)
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i)
                addMember(nodeId(i, j, k, nx, ny), nodeId(i, j, k + 1, nx, ny), 0);

    for (int k = 1; k <= stories; ++k) {
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i < nx; ++i)
                addMember(nodeId(i, j, k, nx, ny), nodeId(i + 1, j, k, nx, ny), 1);
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i <= nx; ++i)
                addMember(nodeId(i, j, k, nx, ny), nodeId(i, j + 1, k, nx, ny), 1);
    }

    for (int k = 0; k < stories; ++k) {
        for (int i = 0; i < nx; ++i) {
            for (int face = 0; face < 2; ++face) {
                const int jf = face ? ny : 0;
                const int a = ((i + k) % 2 == 0) ? nodeId(i, jf, k, nx, ny)     : nodeId(i + 1, jf, k, nx, ny);
                const int b = ((i + k) % 2 == 0) ? nodeId(i + 1, jf, k + 1, nx, ny) : nodeId(i, jf, k + 1, nx, ny);
                addMember(a, b, 2);
            }
        }
        for (int j = 0; j < ny; ++j) {
            for (int face = 0; face < 2; ++face) {
                const int ifc = face ? nx : 0;
                const int a = ((j + k) % 2 == 0) ? nodeId(ifc, j, k, nx, ny)     : nodeId(ifc, j + 1, k, nx, ny);
                const int b = ((j + k) % 2 == 0) ? nodeId(ifc, j + 1, k + 1, nx, ny) : nodeId(ifc, j, k + 1, nx, ny);
                addMember(a, b, 2);
            }
        }
    }

    m.nodalLoads.reserve(static_cast<size_t>(nx + 1) * (ny + 1) * stories + 2);
    for (int k = 1; k <= stories; ++k) {
        const real sf = static_cast<real>(k) / static_cast<real>(stories);
        for (int j = 0; j <= ny; ++j) {
            for (int i = 0; i <= nx; ++i) {
                const int edge = (i == 0 || i == nx ? 1 : 0) + (j == 0 || j == ny ? 1 : 0);
                const real trib = edge == 0 ? 1.0 : (edge == 1 ? 0.65 : 0.42);
                NodalLoad l;
                l.node = nodeId(i, j, k, nx, ny);
                l.comp[Ux] = 900.0 * sf * (1.0 + 0.08 * (j - ny / 2.0));
                l.comp[Uy] = -350.0 * sf * (1.0 + 0.05 * (i - nx / 2.0));
                l.comp[Uz] = -38000.0 * trib;
                m.nodalLoads.push_back(l);
            }
        }
    }
    NodalLoad t1; t1.node = nodeId(nx, ny, stories, nx, ny); t1.comp[Rz] = 3.0e6; m.nodalLoads.push_back(t1);
    NodalLoad t2; t2.node = nodeId(0, 0, stories, nx, ny);  t2.comp[Ry] = -2.0e6; m.nodalLoads.push_back(t2);
    return m;
}

double checksum(const SolveResult& r) {
    double s = 0.0;
    for (size_t i = 0; i < r.u.size(); i += std::max<size_t>(1, r.u.size() / 4096)) s += std::abs(r.u[i]);
    return s;
}

void applyPreset(const std::string& p, BenchSpec& s) {
    // Targets are free-DOF (k=0 fixed). Approx free = (nx+1)(ny+1)(stories+1)*6 - (nx+1)(ny+1)*6
    const bool sr = s.skipRecover, cm = s.compareModes, gp = s.useGpu;
    if (p == "90k")  { s = BenchSpec{18, 12, 60, s.repeat, s.warmup, sr, cm, gp, "~90k-frame-tower"}; }
    else if (p == "120k") { s = BenchSpec{18, 12, 80, s.repeat, s.warmup, sr, cm, gp, "~120k-frame-tower"}; }
    else if (p == "150k") { s = BenchSpec{20, 15, 80, s.repeat, s.warmup, sr, cm, gp, "~160k-frame-tower"}; }
    else if (p == "200k") { s = BenchSpec{22, 16, 90, s.repeat, s.warmup, sr, cm, gp, "~200k-frame-tower"}; }
}

BenchSpec parseArgs(int argc, char** argv) {
    BenchSpec s;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : ""; };
        if (std::strcmp(argv[i], "--preset") == 0) applyPreset(next(), s);
        else if (std::strcmp(argv[i], "--nx") == 0) s.nx = std::atoi(next());
        else if (std::strcmp(argv[i], "--ny") == 0) s.ny = std::atoi(next());
        else if (std::strcmp(argv[i], "--stories") == 0) s.stories = std::atoi(next());
        else if (std::strcmp(argv[i], "--repeat") == 0) s.repeat = std::max(1, std::atoi(next()));
        else if (std::strcmp(argv[i], "--warmup") == 0) s.warmup = std::max(0, std::atoi(next()));
        else if (std::strcmp(argv[i], "--skip-recover") == 0) s.skipRecover = true;
        else if (std::strcmp(argv[i], "--compare") == 0) s.compareModes = true;
        else if (std::strcmp(argv[i], "--gpu") == 0) s.useGpu = true;
    }
    return s;
}

double median(std::vector<double> v);
double percentile(std::vector<double> v, double q);
double minv(const std::vector<double>& v);

struct RunStats {
    double med, mn, p95;
    double factorMs;
    double cs;
};

RunStats runOnce(const FrameModel& model, const SolveOptions& opt, bool skipRecover,
                 int repeat, int warmup, const char* label, bool useGpu = false) {
    const auto tF0 = std::chrono::steady_clock::now();
    PreparedSystem prepared = assembleAndFactor(model, opt);
    const auto tF1 = std::chrono::steady_clock::now();
    const double factorMs = std::chrono::duration<double, std::milli>(tF1 - tF0).count();
    if (prepared.isSingular()) {
        std::printf("FAIL: prepared singular: %s\n", prepared.diagnostic());
        std::exit(2);
    }
    SnSessionOptions sOpts;
    sOpts.skipForceRecovery = skipRecover;
    sOpts.useGpuBacksub     = useGpu;
    SnSession session(prepared, sOpts);
    if (!session.valid()) {
        std::printf("FAIL: session invalid: %s\n", session.diagnostic().c_str());
        std::exit(3);
    }
    double cs = 0.0;
    for (int i = 0; i < warmup; ++i) {
        SolveResult r = session.solveFrame(model);
        if (r.singular) { std::printf("FAIL: warmup singular: %s\n", r.diagnostic.c_str()); std::exit(4); }
        cs += checksum(r);
    }
    std::vector<double> backsubMs;
    backsubMs.reserve(static_cast<size_t>(repeat));
    size_t lastMembers = 0;
    // R2.2: accumulate sub-stage timings across the repeats. The SnSession exposes lastTimings()
    // for the most recent solveFrame; we sum them up and divide to get a per-repeat average
    // breakdown of RHS / backsub / IR / scatter / SPMV / recover.
    double rhsSum = 0, rhsEqSum = 0, rhsKSum = 0, bsubSum = 0, irSum = 0, scatSum = 0, spmvSum = 0, recSum = 0, totSum = 0;
    for (int i = 0; i < repeat; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        SolveResult r = session.solveFrame(model);
        const auto t1 = std::chrono::steady_clock::now();
        if (r.singular) { std::printf("FAIL: solve singular: %s\n", r.diagnostic.c_str()); std::exit(5); }
        cs += checksum(r);
        if (i == 0) lastMembers = r.memberForces.size();
        backsubMs.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        const SnSessionTimings st = session.lastTimings();
        rhsSum += st.rhsMs;   rhsEqSum += st.rhsEqMs;   rhsKSum += st.rhsKMs;
        bsubSum += st.backsubMs;   irSum   += st.irMs;
        scatSum += st.scatterMs;   spmvSum += st.spmvMs;
        recSum += st.recoverMs;   totSum  += st.totalMs;
    }
    const double invR = 1.0 / static_cast<double>(std::max(1, repeat));
    RunStats rs{ median(backsubMs), minv(backsubMs), percentile(backsubMs, 0.95), factorMs, cs };
    std::printf("RUN %-10s skipRecover=%d factorMs=%.3f solveFrame_med=%.3f min=%.3f p95=%.3f members=%zu cs=%.6g\n",
                label, skipRecover ? 1 : 0, rs.factorMs, rs.med, rs.mn, rs.p95, lastMembers, rs.cs);
    std::printf("STAGE      rhs=%.3f (eq=%.3f Kloop=%.3f rest=%.3f) bsub=%.3f ir=%.3f scat=%.3f spmv=%.3f rec=%.3f tot=%.3f\n",
                rhsSum * invR, rhsEqSum * invR, rhsKSum * invR, (rhsSum - rhsEqSum - rhsKSum) * invR,
                bsubSum * invR, irSum * invR, scatSum * invR, spmvSum * invR, recSum * invR, totSum * invR);
    return rs;
}

double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}
double percentile(std::vector<double> v, double q) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t idx = std::min(v.size() - 1, static_cast<size_t>(q * v.size()));
    return v[idx];
}
double minv(const std::vector<double>& v) { return v.empty() ? 0.0 : *std::min_element(v.begin(), v.end()); }

} // namespace

int main(int argc, char** argv) {
    BenchSpec spec = parseArgs(argc, argv);
    std::printf("R2 realtime-150k micro-bench (production SnSession lane)\n");
    std::printf("case=%s nx=%d ny=%d stories=%d repeat=%d warmup=%d\n",
                spec.name.c_str(), spec.nx, spec.ny, spec.stories, spec.repeat, spec.warmup);

    const auto tBuild0 = std::chrono::steady_clock::now();
    FrameModel model = makeTower(spec.nx, spec.ny, spec.stories);
    const auto tBuild1 = std::chrono::steady_clock::now();
    const double buildMs = std::chrono::duration<double, std::milli>(tBuild1 - tBuild0).count();

    int fixed = 0;
    for (const Node& n : model.nodes) for (bool f : n.fixed) if (f) ++fixed;
    const int dof = model.dofCount();
    const int freeDof = dof - fixed;
    std::printf("nodes=%zu members=%zu dof=%d freeDof=%d loads=%zu buildMs=%.3f\n",
                model.nodes.size(), model.members.size(), dof, freeDof, model.nodalLoads.size(), buildMs);

    SolveOptions opt;
    opt.useSupernodalPrimary = true;
    auto verdict = [](double budget, double v) -> const char* { return v <= budget ? "PASS" : "FAIL"; };

    if (spec.compareModes) {
        RunStats off = runOnce(model, opt, /*skipRecover=*/false, spec.repeat, spec.warmup, "RECOVER", spec.useGpu);
        RunStats on  = runOnce(model, opt, /*skipRecover=*/true,  spec.repeat, spec.warmup, "LAZY   ", spec.useGpu);
        std::printf("SPEEDUP solveFrame_med RECOVER=%.3f LAZY=%.3f ratio=%.2fx saved=%.3fms (gpu=%d)\n",
                    off.med, on.med, off.med / std::max(on.med, 1e-9), off.med - on.med, spec.useGpu ? 1 : 0);
        std::printf("VERDICT (LAZY mode%s)\n", spec.useGpu ? " + GPU" : "");
        std::printf("  60fps_budget=16.67ms  %s  margin=%+.3fms\n", verdict(16.67, on.med), 16.67 - on.med);
        std::printf("  30fps_budget=33.33ms  %s  margin=%+.3fms\n", verdict(33.33, on.med), 33.33 - on.med);
        std::printf("  interactive_100ms     %s  margin=%+.3fms\n", verdict(100.0, on.med), 100.0 - on.med);
    } else {
        RunStats rs = runOnce(model, opt, spec.skipRecover, spec.repeat, spec.warmup,
                              spec.skipRecover ? "LAZY   " : "RECOVER", spec.useGpu);
        std::printf("VERDICT%s\n", spec.useGpu ? " (GPU lane)" : "");
        std::printf("  60fps_budget=16.67ms  %s  margin=%+.3fms  (median %.3f)\n", verdict(16.67, rs.med), 16.67 - rs.med, rs.med);
        std::printf("  30fps_budget=33.33ms  %s  margin=%+.3fms\n", verdict(33.33, rs.med), 33.33 - rs.med);
        std::printf("  interactive_100ms     %s  margin=%+.3fms\n", verdict(100.0, rs.med), 100.0 - rs.med);
    }
    return 0;
}
