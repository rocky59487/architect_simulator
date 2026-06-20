// R2 round-3 step 2b: GPU sparse Cholesky via cuDSS (NVIDIA GPU-native direct solver)
//
// gpu_bench.cpp (step 2a) used cusolverSpDcsrlsvchol -- the cuSOLVER high-level sparse
// Cholesky -- and got 9501 ms at 90k. Per NVIDIA docs that API is HOST-based: the
// "csrchol*" family in cusolverSp runs symbolic + numeric factorization on CPU and only
// uploads the solve. So step 2a was effectively a single-threaded CPU Cholesky with GPU
// trip overhead. NEGATIVE result, but expected.
//
// Step 2b switches to cuDSS (cuSparse Direct Solver) -- NVIDIA's 2024+ GPU-native
// sparse direct solver. cuDSS PHASE_FACTORIZATION runs on device with proper GPU
// parallelism, and PHASE_SOLVE is a true GPU triangular solve. This is the API NVIDIA
// recommends for "factor once, solve many" workflows that need GPU acceleration.
//
// Same fixture as gpu_bench.cpp / fp32_safety.cpp: the 90k braced frame tower with
// nx=18, ny=12, stories=60 (88,920 free DOFs, nnz ~ 3.8 M).
//
// What we measure:
//   * PHASE_ANALYSIS (one-shot, ordering + symbolic): expected slow.
//   * PHASE_FACTORIZATION (one-shot, GPU numeric):   expected fast (vs CPU 3-4 s).
//   * PHASE_SOLVE (per-frame):                        if < 16 ms, we win 60 fps.
//   * upload(b) + solve + download(x): the realistic interactive load drag.
//
// Result interpretation -- same thresholds as gpu_bench.cpp:
//   * < 16.67 ms per frame → 60 fps possible → GPU lane GO
//   * < 33.33 ms           → 30 fps → GO
//   * < 100 ms             → interactive only → maybe
//   * else                 → NOT viable, document and stop

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
    int nx = 18, ny = 12, stories = 60;  // default ~90k
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            const char* p = argv[++i];
            if (std::strcmp(p, "90k") == 0)        { nx = 18; ny = 12; stories = 60; }
            else if (std::strcmp(p, "120k") == 0)  { nx = 18; ny = 12; stories = 80; }
            else if (std::strcmp(p, "150k") == 0)  { nx = 20; ny = 15; stories = 80; }
            else if (std::strcmp(p, "200k") == 0)  { nx = 22; ny = 16; stories = 90; }
        }
    }
    std::printf("R2 round-3 step 2b: GPU sparse Cholesky via cuDSS\n");
    std::printf("fixture: tower nx=%d ny=%d stories=%d (~90k DOF)\n", nx, ny, stories);

    cudaDeviceProp prop;
    int gpuDev = -1;
    CUDA_CHECK(cudaGetDevice(&gpuDev));
    CUDA_CHECK(cudaGetDeviceProperties(&prop, gpuDev));
    std::printf("GPU: %s | SM %d.%d | %.1f GB | %d MHz\n",
                prop.name, prop.major, prop.minor,
                prop.totalGlobalMem / 1.0e9, (int)(prop.clockRate / 1000));

    FrameModel model = makeTower(nx, ny, stories);
    const int N = model.dofCount();
    std::vector<int> fmap((size_t)N, -1);
    int nf = 0;
    for (size_t i = 0; i < model.nodes.size(); ++i)
        for (int d = 0; d < 6; ++d)
            if (!model.nodes[i].fixed[d]) fmap[(size_t)(6 * (int)i + d)] = nf++;
    std::printf("N=%d nf=%d members=%zu\n", N, nf, model.members.size());

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
    std::printf("RHS: ||b||_inf = %.6e\n", bNorm);

    // FP64 baseline for residual check
    Eigen::SparseMatrix<double> KffCol = Eigen::SparseMatrix<double>(Kff);
    Eigen::VectorXd b64 = Eigen::Map<Eigen::VectorXd>(b.data(), nf);
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt;
    ldlt.compute(KffCol);
    if (ldlt.info() != Eigen::Success) { std::printf("FAIL: Eigen LDLT factor\n"); return 5; }
    Eigen::VectorXd x64 = ldlt.solve(b64);

    // cuDSS setup
    cudssHandle_t handle = nullptr;
    cudssConfig_t solverConfig = nullptr;
    cudssData_t solverData = nullptr;
    CUDSS_CHECK(cudssCreate(&handle));
    CUDSS_CHECK(cudssConfigCreate(&solverConfig));
    CUDSS_CHECK(cudssDataCreate(handle, &solverData));

    // Device CSR for K_ff
    int *d_rowOff = nullptr, *d_colIdx = nullptr;
    double *d_val = nullptr, *d_b = nullptr, *d_x = nullptr;
    CUDA_CHECK(cudaMalloc(&d_rowOff, sizeof(int) * (nf + 1)));
    CUDA_CHECK(cudaMalloc(&d_colIdx, sizeof(int) * nnz));
    CUDA_CHECK(cudaMalloc(&d_val,    sizeof(double) * nnz));
    CUDA_CHECK(cudaMalloc(&d_b,      sizeof(double) * nf));
    CUDA_CHECK(cudaMalloc(&d_x,      sizeof(double) * nf));

    const auto tU0 = std::chrono::steady_clock::now();
    CUDA_CHECK(cudaMemcpy(d_rowOff, Kff.outerIndexPtr(), sizeof(int) * (nf + 1), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_colIdx, Kff.innerIndexPtr(), sizeof(int) * nnz, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_val,    Kff.valuePtr(),      sizeof(double) * nnz, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaDeviceSynchronize());
    const auto tU1 = std::chrono::steady_clock::now();
    const double uploadK_ms = std::chrono::duration<double, std::milli>(tU1 - tU0).count();

    cudssMatrix_t cuK = nullptr, cuB = nullptr, cuX = nullptr;
    // K is SPD; CUDSS_MVIEW_UPPER is fine but we built a full-symmetric CSR -- use FULL view.
    CUDSS_CHECK(cudssMatrixCreateCsr(&cuK, nf, nf, nnz,
                                      d_rowOff, /*rowEnd*/nullptr, d_colIdx, d_val,
                                      CUDSS_R_32I, CUDSS_R_32I, CUDSS_R_64F,
                                      CUDSS_MTYPE_SPD, CUDSS_MVIEW_FULL, CUDSS_BASE_ZERO));
    CUDSS_CHECK(cudssMatrixCreateDn(&cuB, nf, 1, nf, d_b, CUDSS_R_64F, CUDSS_LAYOUT_COL_MAJOR));
    CUDSS_CHECK(cudssMatrixCreateDn(&cuX, nf, 1, nf, d_x, CUDSS_R_64F, CUDSS_LAYOUT_COL_MAJOR));

    // PHASE_ANALYSIS (ordering + symbolic) — one-shot, may run on host then upload
    const auto tA0 = std::chrono::steady_clock::now();
    CUDSS_CHECK(cudssExecute(handle, CUDSS_PHASE_ANALYSIS, solverConfig, solverData, cuK, cuX, cuB));
    CUDA_CHECK(cudaDeviceSynchronize());
    const auto tA1 = std::chrono::steady_clock::now();
    const double analysis_ms = std::chrono::duration<double, std::milli>(tA1 - tA0).count();

    // PHASE_FACTORIZATION (numeric) — runs on GPU
    const auto tF0 = std::chrono::steady_clock::now();
    CUDSS_CHECK(cudssExecute(handle, CUDSS_PHASE_FACTORIZATION, solverConfig, solverData, cuK, cuX, cuB));
    CUDA_CHECK(cudaDeviceSynchronize());
    const auto tF1 = std::chrono::steady_clock::now();
    const double factor_ms = std::chrono::duration<double, std::milli>(tF1 - tF0).count();

    // PHASE_SOLVE — per-frame backsub. Time multiple repeats.
    const int repeat = 50;
    std::vector<double> solveMs;
    solveMs.reserve((size_t)repeat);
    for (int r = 0; r < repeat; ++r) {
        const auto tB0 = std::chrono::steady_clock::now();
        CUDA_CHECK(cudaMemcpy(d_b, b.data(), sizeof(double) * nf, cudaMemcpyHostToDevice));
        const auto tS0 = std::chrono::steady_clock::now();
        CUDSS_CHECK(cudssExecute(handle, CUDSS_PHASE_SOLVE, solverConfig, solverData, cuK, cuX, cuB));
        CUDA_CHECK(cudaDeviceSynchronize());
        const auto tS1 = std::chrono::steady_clock::now();
        std::vector<double> x_gpu((size_t)nf, 0.0);
        CUDA_CHECK(cudaMemcpy(x_gpu.data(), d_x, sizeof(double) * nf, cudaMemcpyDeviceToHost));
        const auto tS2 = std::chrono::steady_clock::now();
        const double total_ms = std::chrono::duration<double, std::milli>(tS2 - tB0).count();
        solveMs.push_back(total_ms);
        if (r == 0) {
            // Verify residual on the first iter
            Eigen::Map<Eigen::VectorXd> xgMap(x_gpu.data(), nf);
            Eigen::VectorXd resV = b64 - KffCol * xgMap;
            double resInf = 0; for (int i = 0; i < resV.size(); ++i) resInf = std::max(resInf, std::fabs(resV[i]));
            const double resRel = resInf / std::max(bNorm, 1e-300);
            Eigen::VectorXd dx = xgMap - x64;
            double dxInf = 0; for (int i = 0; i < dx.size(); ++i) dxInf = std::max(dxInf, std::fabs(dx[i]));
            double xInf = 0; for (int i = 0; i < x64.size(); ++i) xInf = std::max(xInf, std::fabs(x64[i]));
            std::printf("residual rel: %.3e   |x_gpu-x64|/|x64|: %.3e\n", resRel, dxInf / std::max(xInf, 1e-300));
        }
    }
    std::sort(solveMs.begin(), solveMs.end());
    const double med = solveMs[solveMs.size() / 2];
    const double mn  = solveMs.front();
    const double p95 = solveMs[(size_t)(0.95 * solveMs.size())];

    std::printf("STAGE (cuDSS, GPU-native)\n");
    std::printf("  uploadK (one-shot)   : %.2f ms\n", uploadK_ms);
    std::printf("  analysis (one-shot)  : %.2f ms\n", analysis_ms);
    std::printf("  factorisation (one)  : %.2f ms\n", factor_ms);
    std::printf("  per-frame (B+solve+D): median=%.3f min=%.3f p95=%.3f ms (n=%d)\n",
                med, mn, p95, repeat);

    std::printf("VERDICT (per-frame end-to-end)\n");
    std::printf("  60 fps   (16.67 ms) : %s  margin=%+.2fms\n", med < 16.67 ? "PASS" : "FAIL", 16.67 - med);
    std::printf("  30 fps   (33.33 ms) : %s  margin=%+.2fms\n", med < 33.33 ? "PASS" : "FAIL", 33.33 - med);
    std::printf("  interactive (100ms) : %s  margin=%+.2fms\n", med < 100.0 ? "PASS" : "FAIL", 100.0 - med);

    if (med < 16.67)
        std::printf("OVERALL: cuDSS GPU lane is VIABLE for 60 fps -- proceed to SnSession integration plan\n");
    else if (med < 33.33)
        std::printf("OVERALL: cuDSS GPU lane is VIABLE for 30 fps -- proceed to integration plan\n");
    else if (med < 100.0)
        std::printf("OVERALL: cuDSS GPU lane is VIABLE for interactive (100 ms) -- but no 30 fps gain over CPU\n");
    else
        std::printf("OVERALL: cuDSS GPU lane is NOT viable on this fixture\n");

    // Cleanup
    cudssMatrixDestroy(cuK); cudssMatrixDestroy(cuB); cudssMatrixDestroy(cuX);
    cudssDataDestroy(handle, solverData);
    cudssConfigDestroy(solverConfig);
    cudssDestroy(handle);
    cudaFree(d_rowOff); cudaFree(d_colIdx); cudaFree(d_val); cudaFree(d_b); cudaFree(d_x);
    return 0;
}
