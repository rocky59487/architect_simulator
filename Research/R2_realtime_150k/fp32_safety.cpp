// R2 round-3 step 1: FP32 SPD safety check on the 90k frame tower
//
// Question:
//   Is the 90k mixed-frame-tower K_ff well-conditioned enough that
//   Eigen::SimplicialLDLT<SparseMatrix<float>> + 1-2 Neumaier IR steps converges
//   to the FP64 baseline solution? Per Higham §15.5: SPD systems converge under
//   IR when kappa(A) < 1/u_FP32 ~= 8e6.
//
// What this program does:
//   1) Build the 90k braced frame tower (same makeTower as r2_bench).
//   2) Assemble K_ff in Eigen::SparseMatrix<double> using public ElementStiffness
//      APIs (localStiffness12 + transform12). Frame tower has no releases / hinges
//      / shells, so this is bit-identical to what BeamColumnElement::addToK does.
//   3) Cast to SparseMatrix<float>; factor with SimplicialLDLT<float>.
//   4) Build a representative RHS (the tower's design loads, reduced to free DOFs).
//   5) Solve in FP32; FP64-residual; one IR step; FP64-residual again.
//   6) Print:
//      - rel solution diff vs FP64 baseline
//      - residual inf-norm relative to ||b|| before and after IR
//      - FP32 solve time vs FP64 solve time
//
// PASS thresholds:
//   - rel residual <= 1e-3 after 0 IR steps (FP32 alone "good enough" for visual)
//   - rel residual <= 1e-9 after 1 IR step (close to FP64 round-off)
//   - FP32 backsub time strictly less than FP64 backsub time (else FP32 is pointless)
//
// FAIL means: the supernodal kernel FP32 conversion path in PLAN_round3 will not
// converge on real building fixtures and we should pivot to the GPU lane.
//
// NOT production code. Lives only in Research/R2_realtime_150k/. Built by
// build_fp32_safety.bat, not by run_gate.ps1 / build.bat.

#include "FrameCore/FrameSolver.h"
#include "FrameCore/Section.h"
#include "ElementStiffness.h"
#include "FrameEigen.h"

#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace frame;

namespace {

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
    m.materials.emplace_back(200000.0, 76923.07692307692, 7850.0);
    m.sections.push_back(squareSection(520.0));
    m.sections.push_back(squareSection(360.0));
    m.sections.push_back(squareSection(220.0));
    for (int k = 0; k <= stories; ++k) {
        const real driftX = 35.0 * k, driftY = -18.0 * k;
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i) {
                Node n(nodeId(i, j, k, nx, ny), i * sx + driftX, j * sy + driftY, k * h);
                if (k == 0) n.fixAll();
                m.nodes.push_back(n);
            }
    }
    auto nodePos = [&](int id) -> Vec3 { return m.nodes[(size_t)id].pos; };
    auto addMember = [&](int i, int j, int sec) {
        Member mem((int)m.members.size(), i, j, 0, sec);
        mem.refVec = refVecFor(nodePos(i), nodePos(j));
        m.members.push_back(mem);
    };
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
                const int a = ((i + k) % 2 == 0) ? nodeId(i, jf, k, nx, ny) : nodeId(i + 1, jf, k, nx, ny);
                const int b = ((i + k) % 2 == 0) ? nodeId(i + 1, jf, k + 1, nx, ny) : nodeId(i, jf, k + 1, nx, ny);
                addMember(a, b, 2);
            }
        }
        for (int j = 0; j < ny; ++j) {
            for (int face = 0; face < 2; ++face) {
                const int ifc = face ? nx : 0;
                const int a = ((j + k) % 2 == 0) ? nodeId(ifc, j, k, nx, ny) : nodeId(ifc, j + 1, k, nx, ny);
                const int b = ((j + k) % 2 == 0) ? nodeId(ifc, j + 1, k + 1, nx, ny) : nodeId(ifc, j, k + 1, nx, ny);
                addMember(a, b, 2);
            }
        }
    }
    for (int k = 1; k <= stories; ++k) {
        const real sf = (real)k / (real)stories;
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i) {
                const int edge = (i == 0 || i == nx ? 1 : 0) + (j == 0 || j == ny ? 1 : 0);
                const real trib = edge == 0 ? 1.0 : (edge == 1 ? 0.65 : 0.42);
                NodalLoad l; l.node = nodeId(i, j, k, nx, ny);
                l.comp[Ux] = 900.0 * sf * (1.0 + 0.08 * (j - ny / 2.0));
                l.comp[Uy] = -350.0 * sf * (1.0 + 0.05 * (i - nx / 2.0));
                l.comp[Uz] = -38000.0 * trib;
                m.nodalLoads.push_back(l);
            }
    }
    return m;
}

// Manually assemble K_full (N x N FP64) from the tower model using public element APIs.
// frame tower has no releases / hinges / shells, so this matches BeamColumnElement::addToK.
Eigen::SparseMatrix<double> assembleKFull(const FrameModel& m) {
    const int N = m.dofCount();
    std::vector<Eigen::Triplet<double>> tris;
    tris.reserve(m.members.size() * 144);
    for (const Member& mem : m.members) {
        const Node& ni = m.nodes[(size_t)mem.i];
        const Node& nj = m.nodes[(size_t)mem.j];
        const Vec3 d = nj.pos - ni.pos;
        const real L = norm(d);
        const Mat3 R = localAxes(ni.pos, nj.pos, mem.refVec);
        const Mat12 T = transform12(R);
        const Section& sec = m.sections[(size_t)mem.secIdx];
        const Material& mat = m.materials[(size_t)mem.matIdx];
        const Mat12 Kl = localStiffness12(mat.E, mat.G, sec.A, sec.Iy, sec.Iz, sec.J, L);
        const Mat12 Kg = T.transpose() * Kl * T;
        const int dofs[12] = {
            6 * mem.i + 0, 6 * mem.i + 1, 6 * mem.i + 2,
            6 * mem.i + 3, 6 * mem.i + 4, 6 * mem.i + 5,
            6 * mem.j + 0, 6 * mem.j + 1, 6 * mem.j + 2,
            6 * mem.j + 3, 6 * mem.j + 4, 6 * mem.j + 5
        };
        for (int a = 0; a < 12; ++a)
            for (int b = 0; b < 12; ++b)
                tris.emplace_back(dofs[a], dofs[b], (double)Kg(a, b));
    }
    Eigen::SparseMatrix<double> K(N, N);
    K.setFromTriplets(tris.begin(), tris.end());
    K.makeCompressed();
    return K;
}

// Reduce N x N to nf x nf by mapping free DOFs only.
std::vector<int> buildFmap(const FrameModel& m, int& nf) {
    const int N = m.dofCount();
    std::vector<int> fmap((size_t)N, -1);
    int j = 0;
    for (size_t i = 0; i < m.nodes.size(); ++i)
        for (int d = 0; d < 6; ++d)
            if (!m.nodes[i].fixed[d]) fmap[(size_t)(6 * (int)i + d)] = j++;
    nf = j;
    return fmap;
}

Eigen::SparseMatrix<double> reduceToFree(const Eigen::SparseMatrix<double>& K,
                                          const std::vector<int>& fmap, int nf) {
    std::vector<Eigen::Triplet<double>> tris;
    tris.reserve((size_t)K.nonZeros());
    for (int c = 0; c < K.outerSize(); ++c)
        for (Eigen::SparseMatrix<double>::InnerIterator it(K, c); it; ++it) {
            const int r = (int)it.row();
            if (fmap[(size_t)r] < 0 || fmap[(size_t)c] < 0) continue;
            tris.emplace_back(fmap[(size_t)r], fmap[(size_t)c], it.value());
        }
    Eigen::SparseMatrix<double> Kff(nf, nf);
    Kff.setFromTriplets(tris.begin(), tris.end());
    Kff.makeCompressed();
    return Kff;
}

double infNorm(const Eigen::VectorXd& v) {
    double s = 0; for (int i = 0; i < v.size(); ++i) s = std::max(s, std::fabs(v[i])); return s;
}

}  // namespace

int main() {
    const int nx = 18, ny = 12, stories = 60;
    std::printf("R2 round-3 step 1: FP32 SPD safety check\n");
    std::printf("fixture: tower nx=%d ny=%d stories=%d (~90k DOF)\n", nx, ny, stories);

    FrameModel model = makeTower(nx, ny, stories);
    int nf = 0;
    std::vector<int> fmap = buildFmap(model, nf);
    const int N = model.dofCount();
    std::printf("N=%d nf=%d members=%zu\n", N, nf, model.members.size());

    // Build K (FP64), reduce to K_ff
    const auto tA0 = std::chrono::steady_clock::now();
    Eigen::SparseMatrix<double> Kfull = assembleKFull(model);
    Eigen::SparseMatrix<double> K64 = reduceToFree(Kfull, fmap, nf);
    const auto tA1 = std::chrono::steady_clock::now();
    std::printf("assemble Kff: nnz=%lld build=%.2fms\n",
                (long long)K64.nonZeros(),
                std::chrono::duration<double, std::milli>(tA1 - tA0).count());

    // Build F_full from nodal loads, reduce to Ff
    Eigen::VectorXd Ffull = Eigen::VectorXd::Zero(N);
    for (const NodalLoad& nl : model.nodalLoads) {
        const int ni = model.nodeIndex(nl.node);
        if (ni < 0) continue;
        for (int d = 0; d < 6; ++d) Ffull[6 * ni + d] += nl.comp[d];
    }
    Eigen::VectorXd Ff64(nf);
    for (int g = 0; g < N; ++g) if (fmap[(size_t)g] >= 0) Ff64[fmap[(size_t)g]] = Ffull[g];
    const double bNorm = infNorm(Ff64);
    std::printf("RHS Ff: ||F||_inf = %.6e\n", bNorm);

    // ----- FP64 baseline -----
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt64;
    const auto tF640 = std::chrono::steady_clock::now();
    ldlt64.compute(K64);
    const auto tF641 = std::chrono::steady_clock::now();
    if (ldlt64.info() != Eigen::Success) {
        std::printf("FAIL: FP64 LDLT factor info != Success\n");
        return 2;
    }
    const auto tS640 = std::chrono::steady_clock::now();
    Eigen::VectorXd x64 = ldlt64.solve(Ff64);
    const auto tS641 = std::chrono::steady_clock::now();
    const double factor64Ms = std::chrono::duration<double, std::milli>(tF641 - tF640).count();
    const double solve64Ms  = std::chrono::duration<double, std::milli>(tS641 - tS640).count();
    Eigen::VectorXd r64 = Ff64 - K64 * x64;
    const double res64Rel = infNorm(r64) / std::max(bNorm, 1e-300);
    std::printf("FP64 LDLT (Eigen):  factor=%.2fms  solve=%.2fms  resInf/||F||=%.3e\n",
                factor64Ms, solve64Ms, res64Rel);

    // ----- FP32 path -----
    Eigen::SparseMatrix<float> K32 = K64.cast<float>();
    Eigen::VectorXf Ff32 = Ff64.cast<float>();
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<float>> ldlt32;
    const auto tF320 = std::chrono::steady_clock::now();
    ldlt32.compute(K32);
    const auto tF321 = std::chrono::steady_clock::now();
    if (ldlt32.info() != Eigen::Success) {
        std::printf("FAIL: FP32 LDLT factor info != Success (kappa probably > 1e7)\n");
        return 3;
    }
    const auto tS320 = std::chrono::steady_clock::now();
    Eigen::VectorXf x32 = ldlt32.solve(Ff32);
    const auto tS321 = std::chrono::steady_clock::now();
    const double factor32Ms = std::chrono::duration<double, std::milli>(tF321 - tF320).count();
    const double solve32Ms  = std::chrono::duration<double, std::milli>(tS321 - tS320).count();

    // Residual in FP64 of the FP32 solution
    Eigen::VectorXd x32_as64 = x32.cast<double>();
    Eigen::VectorXd r0 = Ff64 - K64 * x32_as64;
    const double res0Rel = infNorm(r0) / std::max(bNorm, 1e-300);

    // 1 IR step: d = K \ r in FP32, x += d
    const auto tIR0 = std::chrono::steady_clock::now();
    Eigen::VectorXf r0_32 = r0.cast<float>();
    Eigen::VectorXf d32 = ldlt32.solve(r0_32);
    Eigen::VectorXd d_as64 = d32.cast<double>();
    Eigen::VectorXd x32_ir1 = x32_as64 + d_as64;
    Eigen::VectorXd r1 = Ff64 - K64 * x32_ir1;
    const auto tIR1 = std::chrono::steady_clock::now();
    const double res1Rel = infNorm(r1) / std::max(bNorm, 1e-300);
    const double irMs = std::chrono::duration<double, std::milli>(tIR1 - tIR0).count();

    // 2nd IR step
    Eigen::VectorXf r1_32 = r1.cast<float>();
    Eigen::VectorXf d2_32 = ldlt32.solve(r1_32);
    Eigen::VectorXd d2_as64 = d2_32.cast<double>();
    Eigen::VectorXd x32_ir2 = x32_ir1 + d2_as64;
    Eigen::VectorXd r2 = Ff64 - K64 * x32_ir2;
    const double res2Rel = infNorm(r2) / std::max(bNorm, 1e-300);

    // Solution difference vs FP64
    Eigen::VectorXd du0 = x32_as64 - x64;
    Eigen::VectorXd du1 = x32_ir1 - x64;
    Eigen::VectorXd du2 = x32_ir2 - x64;
    const double xNorm = std::max(infNorm(x64), 1e-300);
    std::printf("FP32 LDLT (Eigen):  factor=%.2fms  solve=%.2fms  IR_step=%.2fms\n",
                factor32Ms, solve32Ms, irMs);
    std::printf("residual rel:       IR=0 %.3e   IR=1 %.3e   IR=2 %.3e\n", res0Rel, res1Rel, res2Rel);
    std::printf("|x32 - x64|_inf/|x64|_inf:  IR=0 %.3e   IR=1 %.3e   IR=2 %.3e\n",
                infNorm(du0) / xNorm, infNorm(du1) / xNorm, infNorm(du2) / xNorm);
    std::printf("speedup FP32 backsub vs FP64 backsub: %.2fx\n", solve64Ms / std::max(solve32Ms, 1e-12));

    // Verdict
    std::printf("VERDICT\n");
    const bool resVisualOk = res0Rel < 1e-3;
    const bool resIROk     = res1Rel < 1e-9 || res2Rel < 1e-9;
    const bool fasterOk    = solve32Ms < solve64Ms;
    std::printf("  res0 < 1e-3 (visual-good FP32 alone):  %s\n", resVisualOk ? "PASS" : "FAIL");
    std::printf("  res<=1e-9 after 1 or 2 IR steps:       %s\n", resIROk ? "PASS" : "FAIL");
    std::printf("  FP32 backsub faster than FP64 backsub: %s\n", fasterOk ? "PASS" : "FAIL");
    if (resIROk && fasterOk)
        std::printf("OVERALL: mixed-precision path is VIABLE -- ok to invest sn_chol.h FP32 kernel work\n");
    else
        std::printf("OVERALL: mixed-precision path is NOT VIABLE -- pivot to GPU offload (RTX 5070 Ti)\n");
    return 0;
}
