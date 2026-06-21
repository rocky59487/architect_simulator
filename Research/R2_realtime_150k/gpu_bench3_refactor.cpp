// R2 round-4 step 3: cuDSS PHASE_REFACTORIZATION measurement
//
// Question: when K's non-zero pattern stays the same but the numeric values change (e.g.
// P-Delta updates K with K_sigma(P), or collapse-event K_T re-build with one removed
// element flag), how much faster is cuDSS PHASE_REFACTORIZATION than PHASE_FACTORIZATION?
// NVIDIA reports 3-5x speedup; we measure on the same frame-tower fixtures r2_bench uses
// so the numbers transfer directly to the production scaling curve.
//
// Method (per size):
//   1) Build K = original frame-tower stiffness (host side).
//   2) Run PHASE_ANALYSIS once (it's shared by both factor and refactor).
//   3) Run PHASE_FACTORIZATION on K -- time it. Save residual via PHASE_SOLVE.
//   4) Mutate values: K' = K * (1 + eps), eps ~ 1e-6 (numerics change, pattern stays).
//   5) Upload K' values back into d_val (in-place).
//   6) Run PHASE_REFACTORIZATION -- time it.
//   7) PHASE_SOLVE on K' and check residual ~ matches the K' Eigen oracle.
//
// PASS thresholds:
//   * PHASE_REFACTORIZATION strictly faster than PHASE_FACTORIZATION
//   * Residual after refactor + solve <= 1e-8 (well below the FP64 round-off floor we see
//     at 200k = 1.6e-9)
//
// NOT production code. Lives only in Research/R2_realtime_150k/. Built by build_gpu_bench3.bat.

#include "FrameCore/FrameSolver.h"
#include "FrameCore/Section.h"
#include "ElementStiffness.h"
#include "FrameEigen.h"

#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

#include <cuda_runtime.h>
#include <cudss.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace frame;

#define CUDA_CHECK(x) do { cudaError_t e = (x); if (e != cudaSuccess) { \
    std::printf("CUDA error %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(e)); return 2; } } while (0)
#define CUDSS_CHECK(x) do { cudssStatus_t s = (x); if (s != CUDSS_STATUS_SUCCESS) { \
    std::printf("cuDSS error %s:%d: %d\n", __FILE__, __LINE__, (int)s); return 3; } } while (0)

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

Eigen::SparseMatrix<double> assembleKFull(const FrameModel& m) {
    const int N = m.dofCount();
    std::vector<Eigen::Triplet<double>> tris;
    tris.reserve(m.members.size() * 144);
    for (const Member& mem : m.members) {
        const Node& ni = m.nodes[(size_t)mem.i];
        const Node& nj = m.nodes[(size_t)mem.j];
        const real L = norm(nj.pos - ni.pos);
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

double infNormD(const std::vector<double>& v) {
    double s = 0; for (double x : v) s = std::max(s, std::fabs(x)); return s;
}

}  // namespace

int main(int argc, char** argv) {
    int nx = 20, ny = 15, stories = 80;  // default ~150k
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            const char* p = argv[++i];
            if (std::strcmp(p, "90k") == 0)        { nx = 18; ny = 12; stories = 60; }
            else if (std::strcmp(p, "120k") == 0)  { nx = 18; ny = 12; stories = 80; }
            else if (std::strcmp(p, "150k") == 0)  { nx = 20; ny = 15; stories = 80; }
            else if (std::strcmp(p, "200k") == 0)  { nx = 22; ny = 16; stories = 90; }
        }
    }

    std::printf("R2 round-4 step 3: cuDSS PHASE_REFACTORIZATION timing\n");
    std::printf("fixture: tower nx=%d ny=%d stories=%d\n", nx, ny, stories);

    cudaDeviceProp prop;
    int gpuDev = -1;
    CUDA_CHECK(cudaGetDevice(&gpuDev));
    CUDA_CHECK(cudaGetDeviceProperties(&prop, gpuDev));
    std::printf("GPU: %s\n", prop.name);

    FrameModel model = makeTower(nx, ny, stories);
    const int N = model.dofCount();
    std::vector<int> fmap((size_t)N, -1);
    int nf = 0;
    for (size_t i = 0; i < model.nodes.size(); ++i)
        for (int d = 0; d < 6; ++d)
            if (!model.nodes[i].fixed[d]) fmap[(size_t)(6 * (int)i + d)] = nf++;

    Eigen::SparseMatrix<double> Kfull = assembleKFull(model);
    std::vector<Eigen::Triplet<double>> trisF;
    trisF.reserve((size_t)Kfull.nonZeros());
    for (int c = 0; c < Kfull.outerSize(); ++c)
        for (Eigen::SparseMatrix<double>::InnerIterator it(Kfull, c); it; ++it) {
            const int r = (int)it.row();
            if (fmap[(size_t)r] < 0 || fmap[(size_t)c] < 0) continue;
            trisF.emplace_back(fmap[(size_t)r], fmap[(size_t)c], it.value());
        }
    Eigen::SparseMatrix<double, Eigen::RowMajor> Kff(nf, nf);
    Kff.setFromTriplets(trisF.begin(), trisF.end());
    Kff.makeCompressed();
    const int64_t nnz = (int64_t)Kff.nonZeros();
    std::printf("Kff: nf=%d nnz=%lld\n", nf, (long long)nnz);

    // RHS
    std::vector<double> Ffull((size_t)N, 0.0);
    for (const NodalLoad& nl : model.nodalLoads) {
        const int ni = model.nodeIndex(nl.node);
        if (ni < 0) continue;
        for (int d = 0; d < 6; ++d) Ffull[(size_t)(6 * ni + d)] += nl.comp[d];
    }
    std::vector<double> b((size_t)nf, 0.0);
    for (int g = 0; g < N; ++g) if (fmap[(size_t)g] >= 0) b[(size_t)fmap[(size_t)g]] = Ffull[(size_t)g];
    const double bNorm = infNormD(b);

    cudssHandle_t handle = nullptr;
    cudssConfig_t cfg = nullptr;
    cudssData_t   data = nullptr;
    CUDSS_CHECK(cudssCreate(&handle));
    CUDSS_CHECK(cudssConfigCreate(&cfg));
    CUDSS_CHECK(cudssDataCreate(handle, &data));

    int *d_rowOff = nullptr, *d_colIdx = nullptr;
    double *d_val = nullptr, *d_b = nullptr, *d_x = nullptr;
    CUDA_CHECK(cudaMalloc(&d_rowOff, sizeof(int) * (nf + 1)));
    CUDA_CHECK(cudaMalloc(&d_colIdx, sizeof(int) * nnz));
    CUDA_CHECK(cudaMalloc(&d_val,    sizeof(double) * nnz));
    CUDA_CHECK(cudaMalloc(&d_b,      sizeof(double) * nf));
    CUDA_CHECK(cudaMalloc(&d_x,      sizeof(double) * nf));
    CUDA_CHECK(cudaMemcpy(d_rowOff, Kff.outerIndexPtr(), sizeof(int) * (nf + 1), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_colIdx, Kff.innerIndexPtr(), sizeof(int) * nnz,      cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_val,    Kff.valuePtr(),      sizeof(double) * nnz,   cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_b,      b.data(),            sizeof(double) * nf,    cudaMemcpyHostToDevice));

    cudssMatrix_t cuK = nullptr, cuB = nullptr, cuX = nullptr;
    CUDSS_CHECK(cudssMatrixCreateCsr(&cuK, nf, nf, nnz,
        d_rowOff, nullptr, d_colIdx, d_val,
        CUDSS_R_32I, CUDSS_R_32I, CUDSS_R_64F,
        CUDSS_MTYPE_SPD, CUDSS_MVIEW_FULL, CUDSS_BASE_ZERO));
    CUDSS_CHECK(cudssMatrixCreateDn(&cuB, nf, 1, nf, d_b, CUDSS_R_64F, CUDSS_LAYOUT_COL_MAJOR));
    CUDSS_CHECK(cudssMatrixCreateDn(&cuX, nf, 1, nf, d_x, CUDSS_R_64F, CUDSS_LAYOUT_COL_MAJOR));

    // 1) PHASE_ANALYSIS (once)
    auto t0 = std::chrono::steady_clock::now();
    CUDSS_CHECK(cudssExecute(handle, CUDSS_PHASE_ANALYSIS, cfg, data, cuK, cuX, cuB));
    CUDA_CHECK(cudaDeviceSynchronize());
    auto t1 = std::chrono::steady_clock::now();
    const double tA_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 2) PHASE_FACTORIZATION (full)
    t0 = std::chrono::steady_clock::now();
    CUDSS_CHECK(cudssExecute(handle, CUDSS_PHASE_FACTORIZATION, cfg, data, cuK, cuX, cuB));
    CUDA_CHECK(cudaDeviceSynchronize());
    t1 = std::chrono::steady_clock::now();
    const double tF_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 3) PHASE_SOLVE on K
    t0 = std::chrono::steady_clock::now();
    CUDSS_CHECK(cudssExecute(handle, CUDSS_PHASE_SOLVE, cfg, data, cuK, cuX, cuB));
    CUDA_CHECK(cudaDeviceSynchronize());
    t1 = std::chrono::steady_clock::now();
    const double tS_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 4) Mutate K: K' = K * (1 + eps) -- numerics change, pattern stays. Upload new d_val.
    const double eps = 1e-3;
    std::vector<double> Kp((size_t)nnz);
    for (int64_t i = 0; i < nnz; ++i) Kp[(size_t)i] = Kff.valuePtr()[i] * (1.0 + eps);
    CUDA_CHECK(cudaMemcpy(d_val, Kp.data(), sizeof(double) * nnz, cudaMemcpyHostToDevice));

    // Re-create cuK to point at the same buffers but signal a value update. We could also
    // use cudssMatrixSetValues; either way cuDSS knows the values changed.
    cudssMatrixSetValues(cuK, d_val);

    // 5) PHASE_REFACTORIZATION
    t0 = std::chrono::steady_clock::now();
    CUDSS_CHECK(cudssExecute(handle, CUDSS_PHASE_REFACTORIZATION, cfg, data, cuK, cuX, cuB));
    CUDA_CHECK(cudaDeviceSynchronize());
    t1 = std::chrono::steady_clock::now();
    const double tR_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 6) PHASE_SOLVE on K' + residual check
    t0 = std::chrono::steady_clock::now();
    CUDSS_CHECK(cudssExecute(handle, CUDSS_PHASE_SOLVE, cfg, data, cuK, cuX, cuB));
    CUDA_CHECK(cudaDeviceSynchronize());
    t1 = std::chrono::steady_clock::now();
    const double tS2_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Verify against an Eigen FP64 oracle on K'
    std::vector<double> x_gpu((size_t)nf, 0.0);
    CUDA_CHECK(cudaMemcpy(x_gpu.data(), d_x, sizeof(double) * nf, cudaMemcpyDeviceToHost));
    Eigen::SparseMatrix<double> KffCol(Kff);
    for (int c = 0; c < KffCol.outerSize(); ++c)
        for (Eigen::SparseMatrix<double>::InnerIterator it(KffCol, c); it; ++it)
            it.valueRef() *= (1.0 + eps);
    Eigen::VectorXd b64 = Eigen::Map<Eigen::VectorXd>(b.data(), nf);
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt;
    ldlt.compute(KffCol);
    Eigen::VectorXd x64 = ldlt.solve(b64);
    Eigen::Map<Eigen::VectorXd> xgMap(x_gpu.data(), nf);
    Eigen::VectorXd resV = b64 - KffCol * xgMap;
    double resInf = 0; for (int i = 0; i < resV.size(); ++i) resInf = std::max(resInf, std::fabs(resV[i]));
    const double resRel = resInf / std::max(bNorm, 1e-300);

    std::printf("TIMING\n");
    std::printf("  PHASE_ANALYSIS         : %.2f ms (one-shot)\n", tA_ms);
    std::printf("  PHASE_FACTORIZATION    : %.2f ms (full)\n", tF_ms);
    std::printf("  PHASE_SOLVE (on K)     : %.2f ms\n", tS_ms);
    std::printf("  PHASE_REFACTORIZATION  : %.2f ms (numerics-only update)\n", tR_ms);
    std::printf("  PHASE_SOLVE (on K')    : %.2f ms\n", tS2_ms);
    std::printf("  speedup REFACTOR vs FACTOR: %.2fx\n", tF_ms / std::max(tR_ms, 1e-9));
    std::printf("VERIFY\n");
    std::printf("  residual rel on K'     : %.3e (PASS if < 1e-8)\n", resRel);
    if (tR_ms < tF_ms && resRel < 1e-8)
        std::printf("OVERALL: cuDSS REFACTORIZATION VIABLE for P-Delta / numerics-only K updates\n");
    else
        std::printf("OVERALL: cuDSS REFACTORIZATION not viable for this workload\n");

    cudssMatrixDestroy(cuK); cudssMatrixDestroy(cuB); cudssMatrixDestroy(cuX);
    cudssDataDestroy(handle, data); cudssConfigDestroy(cfg); cudssDestroy(handle);
    cudaFree(d_rowOff); cudaFree(d_colIdx); cudaFree(d_val); cudaFree(d_b); cudaFree(d_x);
    return 0;
}
