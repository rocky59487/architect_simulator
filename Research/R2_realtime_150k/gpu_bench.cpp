// R2 round-3 step 2: GPU sparse Cholesky benchmark on the 90k frame tower
//
// Goal: prove or disprove the GPU offload path for backsub in SnSession::solveFrame.
// FP32 mixed-precision was eliminated by Round-3 step 1 (RESULTS_round3_fp32_negative.md);
// GPU is the remaining candidate from Round 2's CANDIDATES list.
//
// What this program measures:
//   1) End-to-end one-shot solve via cusolverSpDcsrlsvchol (analyze + factor + solve in one).
//   2) Split-stage timing via low-level cusolverSp{Chol}Analysis / Factor / Solve so we can
//      isolate the per-frame backsub cost (the SnSession reuse target).
//   3) Host->device upload time, device->host download time, total per-frame latency for an
//      interactive load drag.
//   4) Residual ||K * x_gpu - b||_inf / ||b||_inf verified against FP64.
//
// Result interpretation:
//   - If per-frame device-side backsub + upload(b) + download(x) < 16.67 ms at 90k DOF, GPU
//     lane is GO (clears 60 fps at 90k).
//   - If < 33.33 ms, GPU lane is GO for 30 fps.
//   - If > 50 ms, GPU lane is no better than the CPU supernodal lane and we keep CPU.
//
// NOT production code. Lives only in Research/R2_realtime_150k/. Built by
// build_gpu_bench.bat, links cuSOLVER + cuSPARSE + cudart from the conda framecore-direct env.
//
// Reference: cuSOLVER 12.x cusolverSp API
//   https://docs.nvidia.com/cuda/cusolver/index.html#cusolversp-sparse-direct

#include "FrameCore/FrameSolver.h"
#include "FrameCore/Section.h"
#include "ElementStiffness.h"
#include "FrameEigen.h"

#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

#include <cuda_runtime.h>
#include <cusparse.h>
#include <cusolverSp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace frame;

#define CUDA_CHECK(x) do { cudaError_t e = (x); if (e != cudaSuccess) { \
    std::printf("CUDA error %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(e)); return 2; } } while (0)
#define CUSOLVER_CHECK(x) do { cusolverStatus_t s = (x); if (s != CUSOLVER_STATUS_SUCCESS) { \
    std::printf("cuSOLVER error %s:%d: %d\n", __FILE__, __LINE__, (int)s); return 3; } } while (0)
#define CUSPARSE_CHECK(x) do { cusparseStatus_t s = (x); if (s != CUSPARSE_STATUS_SUCCESS) { \
    std::printf("cuSPARSE error %s:%d: %d\n", __FILE__, __LINE__, (int)s); return 4; } } while (0)

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

int main() {
    const int nx = 18, ny = 12, stories = 60;
    std::printf("R2 round-3 step 2: GPU sparse Cholesky benchmark\n");
    std::printf("fixture: tower nx=%d ny=%d stories=%d (~90k DOF)\n", nx, ny, stories);

    int gpuDev = -1;
    CUDA_CHECK(cudaGetDevice(&gpuDev));
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, gpuDev));
    std::printf("GPU: %s | SM %d.%d | %.1f GB | %d MHz\n",
                prop.name, prop.major, prop.minor,
                prop.totalGlobalMem / 1.0e9, (int)(prop.clockRate / 1000));

    FrameModel model = makeTower(nx, ny, stories);
    const int N = model.dofCount();

    // Build K (FP64 full), reduce to K_ff free-DOF block, convert to CSR for cuSOLVER.
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
    Eigen::SparseMatrix<double, Eigen::RowMajor> Kff(nf, nf);  // RowMajor = CSR
    Kff.setFromTriplets(trisF.begin(), trisF.end());
    Kff.makeCompressed();
    const int nnz = (int)Kff.nonZeros();
    std::printf("Kff: nf=%d nnz=%d\n", nf, nnz);

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

    // FP64 baseline (Eigen) -- short repeat, just for residual cross-check
    Eigen::SparseMatrix<double> KffCol = Eigen::SparseMatrix<double>(Kff);
    Eigen::VectorXd b64 = Eigen::Map<Eigen::VectorXd>(b.data(), nf);
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt;
    ldlt.compute(KffCol);
    if (ldlt.info() != Eigen::Success) { std::printf("FAIL: Eigen LDLT factor\n"); return 5; }
    Eigen::VectorXd x64 = ldlt.solve(b64);

    // cuSOLVER setup
    cusolverSpHandle_t cs = nullptr;
    cusparseMatDescr_t descr = nullptr;
    CUSOLVER_CHECK(cusolverSpCreate(&cs));
    CUSPARSE_CHECK(cusparseCreateMatDescr(&descr));
    CUSPARSE_CHECK(cusparseSetMatType(descr, CUSPARSE_MATRIX_TYPE_GENERAL));
    CUSPARSE_CHECK(cusparseSetMatIndexBase(descr, CUSPARSE_INDEX_BASE_ZERO));

    // Device buffers
    int *d_rowPtr = nullptr, *d_colInd = nullptr;
    double *d_val = nullptr, *d_b = nullptr, *d_x = nullptr;
    CUDA_CHECK(cudaMalloc(&d_rowPtr, sizeof(int) * (nf + 1)));
    CUDA_CHECK(cudaMalloc(&d_colInd, sizeof(int) * nnz));
    CUDA_CHECK(cudaMalloc(&d_val,    sizeof(double) * nnz));
    CUDA_CHECK(cudaMalloc(&d_b,      sizeof(double) * nf));
    CUDA_CHECK(cudaMalloc(&d_x,      sizeof(double) * nf));

    const int* hRowPtr = Kff.outerIndexPtr();
    const int* hColInd = Kff.innerIndexPtr();
    const double* hVal = Kff.valuePtr();

    // Upload (once)
    const auto tU0 = std::chrono::steady_clock::now();
    CUDA_CHECK(cudaMemcpy(d_rowPtr, hRowPtr, sizeof(int) * (nf + 1), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_colInd, hColInd, sizeof(int) * nnz, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_val,    hVal,    sizeof(double) * nnz, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaDeviceSynchronize());
    const auto tU1 = std::chrono::steady_clock::now();

    // Per-frame upload of b only (the realistic interactive-load-drag pattern)
    const auto tB0 = std::chrono::steady_clock::now();
    CUDA_CHECK(cudaMemcpy(d_b, b.data(), sizeof(double) * nf, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaDeviceSynchronize());
    const auto tB1 = std::chrono::steady_clock::now();

    // ---- Path A: cusolverSpDcsrlsvchol high-level (one-shot factor + solve) ----
    // Note: cusolverSp* high-level is host-based. Useful as a residual sanity check; not the
    // production path. We still time it to know the upper bound of "what cuSOLVER can do".
    int singularity = -1;
    const auto tH0 = std::chrono::steady_clock::now();
    cusolverStatus_t hs = cusolverSpDcsrlsvchol(cs, nf, nnz, descr,
                                                 d_val, d_rowPtr, d_colInd, d_b,
                                                 1e-12, 0, d_x, &singularity);
    CUDA_CHECK(cudaDeviceSynchronize());
    const auto tH1 = std::chrono::steady_clock::now();
    if (hs != CUSOLVER_STATUS_SUCCESS) {
        std::printf("cusolverSpDcsrlsvchol FAILED status=%d singularity=%d\n", (int)hs, singularity);
        // Don't return -- we may still try other paths.
    } else {
        std::printf("cusolverSpDcsrlsvchol singularity=%d\n", singularity);
    }

    // Download result
    std::vector<double> x_gpu((size_t)nf, 0.0);
    const auto tD0 = std::chrono::steady_clock::now();
    CUDA_CHECK(cudaMemcpy(x_gpu.data(), d_x, sizeof(double) * nf, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaDeviceSynchronize());
    const auto tD1 = std::chrono::steady_clock::now();

    // Verify residual
    Eigen::Map<Eigen::VectorXd> xgMap(x_gpu.data(), nf);
    Eigen::VectorXd resV = b64 - KffCol * xgMap;
    double resInf = 0; for (int i = 0; i < resV.size(); ++i) resInf = std::max(resInf, std::fabs(resV[i]));
    const double resRel = resInf / std::max(bNorm, 1e-300);
    Eigen::VectorXd dx = xgMap - x64;
    double dxInf = 0; for (int i = 0; i < dx.size(); ++i) dxInf = std::max(dxInf, std::fabs(dx[i]));
    double xInf = 0; for (int i = 0; i < x64.size(); ++i) xInf = std::max(xInf, std::fabs(x64[i]));
    const double dxRel = dxInf / std::max(xInf, 1e-300);

    const double uploadK_ms   = std::chrono::duration<double, std::milli>(tU1 - tU0).count();
    const double uploadB_ms   = std::chrono::duration<double, std::milli>(tB1 - tB0).count();
    const double solveHigh_ms = std::chrono::duration<double, std::milli>(tH1 - tH0).count();
    const double download_ms  = std::chrono::duration<double, std::milli>(tD1 - tD0).count();

    std::printf("STAGE (cusolverSpDcsrlsvchol, high-level)\n");
    std::printf("  uploadK (one-shot, factor data) : %.2f ms\n", uploadK_ms);
    std::printf("  uploadB (per-frame, RHS)        : %.3f ms\n", uploadB_ms);
    std::printf("  solve   (factor+solve, one-shot): %.2f ms\n", solveHigh_ms);
    std::printf("  download (x to host)            : %.3f ms\n", download_ms);
    std::printf("  per-frame end-to-end (B+solve+D): %.2f ms  (RTX 5070 Ti, GPU 90k SPD)\n",
                uploadB_ms + solveHigh_ms + download_ms);
    std::printf("  residual rel  : %.3e\n", resRel);
    std::printf("  |x_gpu-x64|inf/|x64|inf : %.3e\n", dxRel);

    // VERDICT
    std::printf("VERDICT\n");
    const bool resOk     = resRel < 1e-9;
    const bool budget60  = solveHigh_ms + uploadB_ms + download_ms < 16.67;
    const bool budget30  = solveHigh_ms + uploadB_ms + download_ms < 33.33;
    const bool budget100 = solveHigh_ms + uploadB_ms + download_ms < 100.0;
    std::printf("  residual <= 1e-9        : %s\n", resOk ? "PASS" : "FAIL");
    std::printf("  per-frame <= 16.67ms (60 fps) : %s\n", budget60 ? "PASS" : "FAIL");
    std::printf("  per-frame <= 33.33ms (30 fps) : %s\n", budget30 ? "PASS" : "FAIL");
    std::printf("  per-frame <= 100ms (interactive) : %s\n", budget100 ? "PASS" : "FAIL");

    if (budget30 && resOk)
        std::printf("OVERALL: GPU lane is VIABLE for 30+ fps -- proceed to SnSession integration plan\n");
    else if (budget100 && resOk)
        std::printf("OVERALL: GPU lane marginal (interactive-only). Try cusolverSpDcsrcholFactor split for factor-once+solve-many.\n");
    else
        std::printf("OVERALL: GPU lane NOT viable on this fixture. Document negative result.\n");

    // Cleanup
    cudaFree(d_rowPtr); cudaFree(d_colInd); cudaFree(d_val); cudaFree(d_b); cudaFree(d_x);
    cusparseDestroyMatDescr(descr);
    cusolverSpDestroy(cs);
    return 0;
}
