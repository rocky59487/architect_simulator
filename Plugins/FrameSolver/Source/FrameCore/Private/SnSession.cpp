//
// Stateful supernodal session: factor ONCE (ctor), reuse the factor across solveFrame calls.
// See Public/FrameCore/SnSession.h.
//
// Design contract (as SnSolver.cpp): the per-frame RHS assembly, prescribed
// reduction, scatter, reactions (K*u - F) and element recovery are copied verbatim from
// FrameSolver.cpp::solveLoad -- the ONLY difference from solveLoad is reusing the PREBUILT supernodal
// factor (sn::solveSuper) instead of S.ldlt.solve(Ff). The LDLT factor remains the oracle and the
// fallback. Eigen + sn are confined to this Private .cpp; the public header is POD. FRAMECORE_SUPERNODAL
// (from FrameSnChol.h) gates the supernodal members/body: when 0 the session routes every frame to LDLT.
//
#include "FrameCore/SnSession.h"
#include "PreparedSystemImpl.h"   // PreparedSystem::Impl (K/fmap/nf/ldlt/elems/...) + reduceFF + FrameEigen.h
#include "IElement.h"
#include "ModelHash.h"            // modelFingerprint
#include "FrameSnChol.h"          // FRAMECORE_SUPERNODAL + sn:: (guarded)

#include <vector>
#include <unordered_map>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdint>
#ifdef SN_SESSION_TIMING
  #include <chrono>
#endif

// R2.3 GPU lane: cuDSS state lives in Impl when FRAMECORE_CUDA=1; main lane compiles
// with FRAMECORE_CUDA=0 and these declarations are not visible -- ABI / build stays clean.
#if defined(FRAMECORE_CUDA) && FRAMECORE_CUDA
  #include <cuda_runtime.h>
  #include <cudss.h>
  #include <cusparse.h>      // Phase 1 (v2.11): GPU SPMV for reactions = K * u - F
#endif

// R2.2 sub-stage timing helpers: zero-cost when SN_SESSION_TIMING is not defined.
// Compile main lane without -DSN_SESSION_TIMING to keep solveFrame on the bare hot path
// (no chrono::steady_clock::now calls). Research/R2_realtime_150k/build_r2.bat opts in.
#ifdef SN_SESSION_TIMING
  #define SN_TIME_BEGIN(name) const auto name##_t0 = std::chrono::steady_clock::now()
  #define SN_TIME_END(name, dst)                                                              \
      const auto name##_t1 = std::chrono::steady_clock::now();                                \
      (dst) = std::chrono::duration<double, std::milli>(name##_t1 - name##_t0).count()
#else
  #define SN_TIME_BEGIN(name)        ((void)0)
  #define SN_TIME_END(name, dst)     ((void)(dst))
#endif

namespace frame {

struct SnSession::Impl {
    const PreparedSystem::Impl* S = nullptr;   // non-owning -- PreparedSystem must outlive the session
    SnSessionOptions opts;
    uint64_t fingerprint = 0;
    bool snReady = false;                       // supernodal factor built + SPD -> reuse per frame
    std::string diag;
#if FRAMECORE_SUPERNODAL
    sn::SnSymbolic sym;                          // symbolic analysis (ordering + etree), built once
    sn::SnSuper    fac;                          // numeric supernodal factor, built once, reused
    // R2.1 PERF-01: when the PreparedSystem was built with useSupernodalPrimary=true, the
    // session reuses the factor THAT ALREADY EXISTS on Impl rather than building a duplicate
    // (avoids paying the ~1.7s supernodal-factor cost twice at 62k DOF). When false (the
    // normal session path), the session owns its own fac/sym in the two members above.
    bool useExternalFac = false;                 // route solveFrame to S->snFac/snSym
    // R2: K_ff CSC structure cached when opts.irSteps>0 so compensated residual SpMV reuses the
    // SAME matrix the factor was built from. Empty when irSteps==0 (~zero memory cost in the
    // default lane). Memory budget when populated: ~nnz(K_ff) * (4+8) bytes (Ai+Ax) +
    // (n+1)*4 bytes (Ap); tens of MB for mid-100k-DOF models, negligible vs the factor itself.
    std::vector<int>    Ap, Ai;
    std::vector<double> Ax;
#endif
    SnSessionTimings lastT;          // R2.2 always-present; zero unless SN_SESSION_TIMING is on
    // R2.2 PERF: NodeId -> node-index cache, populated lazily on first solveFrame() and reused.
    // model.nodeIndex() is O(N) linear search; on a 14k nodal-load / 15k-node tower the per-frame
    // cost is ~74ms (Research/R2_realtime_150k profile). The fingerprint guard keeps nodes stable
    // for the session lifetime, so a one-shot rebuild covers all subsequent solveFrame() calls.
    std::unordered_map<NodeId, int> nodeIdToIdx;

#if defined(FRAMECORE_CUDA) && FRAMECORE_CUDA
    // R2.3: cuDSS GPU lane state. Built at ctor when opts.useGpuBacksub == true. All-null when
    // GPU lane is off; the dtor's individual null checks make cleanup safe in either case.
    bool                gpuReady       = false;   // analysis + factorisation succeeded on GPU
    int                 gpuNf          = 0;
    cudssHandle_t       cudssHandle    = nullptr;
    cudssConfig_t       cudssCfg       = nullptr;
    cudssData_t         cudssDataS     = nullptr;
    cudssMatrix_t       cuK            = nullptr;
    cudssMatrix_t       cuB            = nullptr;
    cudssMatrix_t       cuX            = nullptr;
    int*                d_rowOff       = nullptr;
    int*                d_colIdx       = nullptr;
    double*             d_val          = nullptr;
    double*             d_b            = nullptr;
    double*             d_x            = nullptr;
    int                 gpuNnz         = 0;
    // Permutation from full-DOF (S.K column order) to free-DOF (nf) order for fast Ff scatter
    std::vector<int>    csrRowOff;     // host-side CSR rowOffsets, cached for re-upload checks

    // Phase 1 (v2.11) -- GPU SPMV for reactions = K * u - F. Reuses the cuDSS handle's CUDA
    // context but maintains its own cuSPARSE handle + descriptors. The full N x N stiffness
    // S.K is uploaded once at ctor (same fingerprint guard as the cuDSS factor). per-frame
    // path: upload u (full N), upload F (full N, in-place reused as y for SpMV), call
    // cusparseSpMV with beta=-1.0 so d_F = K*u - F = reactions, download. Falls back to CPU
    // SPMV if any of the setup steps fail.
    bool                gpuReactionsReady = false;
    cusparseHandle_t    cusparseHdl    = nullptr;
    cusparseSpMatDescr_t cuKFull       = nullptr;
    cusparseDnVecDescr_t cuVecU        = nullptr;   // d_u_full
    cusparseDnVecDescr_t cuVecR        = nullptr;   // d_F_full, reused as r = K*u - F
    int                 gpuFullN       = 0;
    int                 gpuFullNnz     = 0;
    int*                d_Krow_full    = nullptr;
    int*                d_Kcol_full    = nullptr;
    double*             d_Kval_full    = nullptr;
    double*             d_u_full       = nullptr;
    double*             d_F_full       = nullptr;   // reused as the SpMV output (= reactions)
    void*               d_spmvWork     = nullptr;
    size_t              spmvWorkSize   = 0;
#endif
};

SnSession::SnSession(const PreparedSystem& prepared, const SnSessionOptions& opts)
    : p_(std::make_unique<Impl>()) {
    p_->S = prepared.impl.get();
    p_->opts = opts;
    if (!p_->S || p_->S->singular) {
        p_->diag = "[SnSession] PreparedSystem null/singular; frames use LDLT";
        return;
    }
    const PreparedSystem::Impl& S = *p_->S;
    p_->fingerprint = S.fingerprint;
#if FRAMECORE_SUPERNODAL
    // R2.1 PERF-01: if the PreparedSystem already holds a supernodal-primary factor, REUSE it.
    // No second factorisation, no extra ~nnz*16B copy of the panels. Cache K_ff CSC for IR if
    // the caller still requested IR steps.
    if (opts.enabled && S.useSnPrimary && S.snFac.spd && S.nf > 0) {
        p_->useExternalFac = true;
        p_->snReady = true;
        p_->diag = "[SnSession] reusing PreparedSystem supernodal-primary factor";
        if (opts.irSteps > 0) {
            SpMat Kff = reduceFF(S.K, S.fmap, S.nf);
            Kff.makeCompressed();
            const int nnzKff = static_cast<int>(Kff.nonZeros());
            p_->Ap.assign(Kff.outerIndexPtr(), Kff.outerIndexPtr() + S.nf + 1);
            p_->Ai.assign(Kff.innerIndexPtr(), Kff.innerIndexPtr() + nnzKff);
            p_->Ax.assign(Kff.valuePtr(),      Kff.valuePtr()      + nnzKff);
            p_->diag += " + IR cache (nnz=" + std::to_string(nnzKff) + ")";
        }
        // R2.3: do NOT early-return -- fall through to the GPU setup block at the end of ctor
        // so useGpuBacksub still attaches a cuDSS factor even on a SnPrimary-reuse session.
    }
    else if (opts.enabled && S.nf > 0) {
        try {
            const int n = S.nf;
            SpMat Kff = reduceFF(S.K, S.fmap, n);     // full-symmetric nf x nf CSC -- sn input
            Kff.makeCompressed();
            p_->sym = sn::analyze(n, Kff.outerIndexPtr(), Kff.innerIndexPtr(), opts.useMetis);
            p_->fac = sn::factorizeSuperParallel(n, Kff.outerIndexPtr(), Kff.innerIndexPtr(),
                                                 Kff.valuePtr(), p_->sym, opts.amalgRelax,
                                                 opts.amalgMaxCol, opts.numThreads, opts.blasThreadsRoot);
            if (p_->fac.spd) {
                p_->snReady = true;
                p_->diag = "[SnSession] supernodal factor ready (reused per frame)";
                // R2: cache K_ff CSC for IR matvec when iterative refinement is requested.
                if (opts.irSteps > 0) {
                    const int  nnzKff = static_cast<int>(Kff.nonZeros());
                    p_->Ap.assign(Kff.outerIndexPtr(), Kff.outerIndexPtr() + n + 1);
                    p_->Ai.assign(Kff.innerIndexPtr(), Kff.innerIndexPtr() + nnzKff);
                    p_->Ax.assign(Kff.valuePtr(),      Kff.valuePtr()      + nnzKff);
                    p_->diag += " + IR cache (nnz=" + std::to_string(nnzKff) + ")";
                }
            } else {
                p_->diag = "[SnSession] supernodal factor not SPD; frames use LDLT";
            }
        } catch (...) {
            p_->snReady = false;
            p_->diag = "[SnSession] supernodal factor threw; frames use LDLT";
        }
    } else {
        p_->diag = opts.enabled ? "[SnSession] no free DOF; frames use LDLT"
                                : "[SnSession] disabled; frames use LDLT";
    }
#else
    p_->diag = "[SnSession] supernodal not compiled (FRAMECORE_SUPERNODAL=0); frames use LDLT";
#endif

#if defined(FRAMECORE_CUDA) && FRAMECORE_CUDA
    // R2.3: opt-in GPU lane. Only attempt when:
    //   * caller explicitly set useGpuBacksub=true
    //   * the supernodal CPU path was set up successfully (snReady) -- we need the same K_ff
    //     pattern; the LDLT fallback path is FP64 and stays valid if GPU setup fails.
    //   * S.nf > 0 (no free DOF -> no system to solve).
    if (opts.useGpuBacksub && p_->snReady && S.nf > 0) {
        try {
            const int n = S.nf;
            // We need K_ff in CSR. Build once; the supernodal CPU path used CSC -- swap by going
            // through Eigen RowMajor (no extra factorisation).
            SpMat Kff_csc = reduceFF(S.K, S.fmap, n);
            Kff_csc.makeCompressed();
            Eigen::SparseMatrix<double, Eigen::RowMajor> Kff_csr(Kff_csc);
            Kff_csr.makeCompressed();
            const int nnz = static_cast<int>(Kff_csr.nonZeros());

            if (cudssCreate(&p_->cudssHandle) != CUDSS_STATUS_SUCCESS ||
                cudssConfigCreate(&p_->cudssCfg) != CUDSS_STATUS_SUCCESS ||
                cudssDataCreate(p_->cudssHandle, &p_->cudssDataS) != CUDSS_STATUS_SUCCESS)
            {
                p_->diag += " | [GPU] cuDSS context create failed; CPU lane used";
                p_->gpuReady = false;
            } else if (cudaMalloc(&p_->d_rowOff, sizeof(int) * (n + 1)) != cudaSuccess ||
                       cudaMalloc(&p_->d_colIdx, sizeof(int) * nnz)     != cudaSuccess ||
                       cudaMalloc(&p_->d_val,    sizeof(double) * nnz)  != cudaSuccess ||
                       cudaMalloc(&p_->d_b,      sizeof(double) * n)    != cudaSuccess ||
                       cudaMalloc(&p_->d_x,      sizeof(double) * n)    != cudaSuccess)
            {
                p_->diag += " | [GPU] cudaMalloc failed; CPU lane used";
                p_->gpuReady = false;
            } else {
                cudaMemcpy(p_->d_rowOff, Kff_csr.outerIndexPtr(), sizeof(int) * (n + 1), cudaMemcpyHostToDevice);
                cudaMemcpy(p_->d_colIdx, Kff_csr.innerIndexPtr(), sizeof(int) * nnz,     cudaMemcpyHostToDevice);
                cudaMemcpy(p_->d_val,    Kff_csr.valuePtr(),      sizeof(double) * nnz,  cudaMemcpyHostToDevice);

                cudssMatrixCreateCsr(&p_->cuK, n, n, nnz,
                                     p_->d_rowOff, nullptr, p_->d_colIdx, p_->d_val,
                                     CUDSS_R_32I, CUDSS_R_32I, CUDSS_R_64F,
                                     CUDSS_MTYPE_SPD, CUDSS_MVIEW_FULL, CUDSS_BASE_ZERO);
                cudssMatrixCreateDn(&p_->cuB, n, 1, n, p_->d_b, CUDSS_R_64F, CUDSS_LAYOUT_COL_MAJOR);
                cudssMatrixCreateDn(&p_->cuX, n, 1, n, p_->d_x, CUDSS_R_64F, CUDSS_LAYOUT_COL_MAJOR);

                const cudssStatus_t sA = cudssExecute(p_->cudssHandle, CUDSS_PHASE_ANALYSIS,
                                                       p_->cudssCfg, p_->cudssDataS,
                                                       p_->cuK, p_->cuX, p_->cuB);
                const cudssStatus_t sF = (sA == CUDSS_STATUS_SUCCESS)
                    ? cudssExecute(p_->cudssHandle, CUDSS_PHASE_FACTORIZATION,
                                   p_->cudssCfg, p_->cudssDataS, p_->cuK, p_->cuX, p_->cuB)
                    : sA;
                cudaDeviceSynchronize();
                if (sA == CUDSS_STATUS_SUCCESS && sF == CUDSS_STATUS_SUCCESS) {
                    p_->gpuReady = true;
                    p_->gpuNf    = n;
                    p_->gpuNnz   = nnz;
                    p_->diag += " | [GPU] cuDSS factor ready (nf=" + std::to_string(n) +
                                ", nnz=" + std::to_string(nnz) + ")";

                    // Phase 1 (v2.11): GPU SPMV for reactions = K * u - F. Reuses the same
                    // CUDA context. Uploads the FULL N x N S.K (not the reduced K_ff) so the
                    // SpMV mirrors the CPU expression `S.K * u - F` exactly. Failure here is
                    // non-fatal: reactions falls back to CPU SPMV in solveFrame.
                    Eigen::SparseMatrix<double, Eigen::RowMajor> Kfull(S.K);
                    Kfull.makeCompressed();
                    const int Nfull   = static_cast<int>(Kfull.rows());
                    const int Knnz    = static_cast<int>(Kfull.nonZeros());
                    bool spmvOk = (cusparseCreate(&p_->cusparseHdl) == CUSPARSE_STATUS_SUCCESS);
                    spmvOk = spmvOk && (cudaMalloc(&p_->d_Krow_full, sizeof(int) * (Nfull + 1)) == cudaSuccess);
                    spmvOk = spmvOk && (cudaMalloc(&p_->d_Kcol_full, sizeof(int) * Knnz)        == cudaSuccess);
                    spmvOk = spmvOk && (cudaMalloc(&p_->d_Kval_full, sizeof(double) * Knnz)     == cudaSuccess);
                    spmvOk = spmvOk && (cudaMalloc(&p_->d_u_full,    sizeof(double) * Nfull)    == cudaSuccess);
                    spmvOk = spmvOk && (cudaMalloc(&p_->d_F_full,    sizeof(double) * Nfull)    == cudaSuccess);
                    if (spmvOk) {
                        cudaMemcpy(p_->d_Krow_full, Kfull.outerIndexPtr(), sizeof(int) * (Nfull + 1), cudaMemcpyHostToDevice);
                        cudaMemcpy(p_->d_Kcol_full, Kfull.innerIndexPtr(), sizeof(int) * Knnz,        cudaMemcpyHostToDevice);
                        cudaMemcpy(p_->d_Kval_full, Kfull.valuePtr(),      sizeof(double) * Knnz,     cudaMemcpyHostToDevice);

                        const cusparseStatus_t smk = cusparseCreateCsr(&p_->cuKFull, Nfull, Nfull, Knnz,
                            p_->d_Krow_full, p_->d_Kcol_full, p_->d_Kval_full,
                            CUSPARSE_INDEX_32I, CUSPARSE_INDEX_32I,
                            CUSPARSE_INDEX_BASE_ZERO, CUDA_R_64F);
                        const cusparseStatus_t smu = cusparseCreateDnVec(&p_->cuVecU, Nfull, p_->d_u_full, CUDA_R_64F);
                        const cusparseStatus_t smr = cusparseCreateDnVec(&p_->cuVecR, Nfull, p_->d_F_full, CUDA_R_64F);

                        // Query workspace for the SpMV we will actually run: y = alpha*A*x + beta*y
                        // with alpha=1, beta=-1, A=K, x=u, y=F (in-place reused for reactions).
                        const double alpha = 1.0, beta = -1.0;
                        cusparseStatus_t smsz = CUSPARSE_STATUS_INTERNAL_ERROR;
                        size_t bufSize = 0;
                        if (smk == CUSPARSE_STATUS_SUCCESS && smu == CUSPARSE_STATUS_SUCCESS && smr == CUSPARSE_STATUS_SUCCESS) {
                            smsz = cusparseSpMV_bufferSize(p_->cusparseHdl, CUSPARSE_OPERATION_NON_TRANSPOSE,
                                                           &alpha, p_->cuKFull, p_->cuVecU, &beta, p_->cuVecR,
                                                           CUDA_R_64F, CUSPARSE_SPMV_ALG_DEFAULT, &bufSize);
                        }
                        if (smsz == CUSPARSE_STATUS_SUCCESS && cudaMalloc(&p_->d_spmvWork, bufSize) == cudaSuccess) {
                            p_->spmvWorkSize      = bufSize;
                            p_->gpuFullN          = Nfull;
                            p_->gpuFullNnz        = Knnz;
                            p_->gpuReactionsReady = true;
                            p_->diag += " + [GPU] cuSPARSE SpMV reactions ready (N=" + std::to_string(Nfull) +
                                        ", nnz=" + std::to_string(Knnz) + ")";
                        } else {
                            p_->gpuReactionsReady = false;
                            p_->diag += " | [GPU] cuSPARSE SpMV setup failed; reactions on CPU";
                        }
                    } else {
                        p_->gpuReactionsReady = false;
                        p_->diag += " | [GPU] cuSPARSE allocation failed; reactions on CPU";
                    }
                } else {
                    p_->gpuReady = false;
                    p_->diag += " | [GPU] cuDSS analysis/factor failed (analysis=" +
                                std::to_string((int)sA) + ", factor=" + std::to_string((int)sF) +
                                "); CPU lane used";
                }
            }
        } catch (...) {
            p_->gpuReady = false;
            p_->diag += " | [GPU] cuDSS setup threw; CPU lane used";
        }
    }
#else
    if (opts.useGpuBacksub) {
        p_->diag += " | [GPU] FRAMECORE_CUDA not compiled; CPU lane used";
    }
#endif
}

SnSession::~SnSession() {
#if defined(FRAMECORE_CUDA) && FRAMECORE_CUDA
    if (p_) {
        if (p_->cuK)         cudssMatrixDestroy(p_->cuK);
        if (p_->cuB)         cudssMatrixDestroy(p_->cuB);
        if (p_->cuX)         cudssMatrixDestroy(p_->cuX);
        if (p_->cudssDataS)  cudssDataDestroy(p_->cudssHandle, p_->cudssDataS);
        if (p_->cudssCfg)    cudssConfigDestroy(p_->cudssCfg);
        if (p_->cudssHandle) cudssDestroy(p_->cudssHandle);
        if (p_->d_rowOff)    cudaFree(p_->d_rowOff);
        if (p_->d_colIdx)    cudaFree(p_->d_colIdx);
        if (p_->d_val)       cudaFree(p_->d_val);
        if (p_->d_b)         cudaFree(p_->d_b);
        if (p_->d_x)         cudaFree(p_->d_x);
        // Phase 1 (v2.11) cuSPARSE SpMV cleanup
        if (p_->cuVecU)      cusparseDestroyDnVec(p_->cuVecU);
        if (p_->cuVecR)      cusparseDestroyDnVec(p_->cuVecR);
        if (p_->cuKFull)     cusparseDestroySpMat(p_->cuKFull);
        if (p_->cusparseHdl) cusparseDestroy(p_->cusparseHdl);
        if (p_->d_Krow_full) cudaFree(p_->d_Krow_full);
        if (p_->d_Kcol_full) cudaFree(p_->d_Kcol_full);
        if (p_->d_Kval_full) cudaFree(p_->d_Kval_full);
        if (p_->d_u_full)    cudaFree(p_->d_u_full);
        if (p_->d_F_full)    cudaFree(p_->d_F_full);
        if (p_->d_spmvWork)  cudaFree(p_->d_spmvWork);
    }
#endif
}
SnSession::SnSession(SnSession&&) noexcept = default;
SnSession& SnSession::operator=(SnSession&&) noexcept = default;

bool SnSession::valid() const { return p_ && p_->snReady; }
const std::string& SnSession::diagnostic() const {
    // R2.1 audit API-MEM SLV-NEW-1 guard: a moved-from SnSession has p_ == nullptr, and the
    // raw dereference here would crash. Return a static empty string instead -- matches the
    // PreparedSystem::diagnostic() pattern.
    static const std::string movedFromDiag;
    return p_ ? p_->diag : movedFromDiag;
}

SolveResult SnSession::solveFrame(const FrameModel& model) {
    // R2.1 audit API-FINAL-1 guard: a moved-from SnSession has p_ == nullptr; the unconditional
    // `*p_->S` dereference at the top would crash. Mirror the diagnostic() pattern and return
    // a well-formed singular SolveResult so the caller sees a clean error instead of UB.
    if (!p_) {
        SolveResult R;
        R.singular = true;
        R.diagnostic = "SnSession::solveFrame called on a moved-from session";
        return R;
    }
    SN_TIME_BEGIN(total);
    const PreparedSystem::Impl& S = *p_->S;
    SolveResult R;
    const int N = S.N;
    R.u.assign((size_t)std::max(0, N), 0.0);
    R.reactions.assign((size_t)std::max(0, N), 0.0);
    if (S.singular) { R.singular = true; R.diagnostic = S.diagnostic; return R; }

    // Reuse-validity guard -- identical to solveLoad (shared modelFingerprint).
    if (modelFingerprint(model) != p_->fingerprint) {
        R.singular = true;
        R.diagnostic = "SnSession::solveFrame: model changed since assembleAndFactor (geometry/topology/"
                       "support flags/distributed loads). Re-run assembleAndFactor.";
        return R;
    }

    // ---- RHS assembly: verbatim from solveLoad ----------------------------------------
    // R2.2 PERF: detect the "all-zero prescribed displacement" case and skip the entire
    // sparse-K column iteration. Research/R2_realtime_150k showed the iteration was 88 ms /
    // 161 ms (55%) on a 90k frame tower with base-fixed-at-0 supports — the loop runs O(nnz)
    // but every contribution is presc*K = 0*K = 0. The bit-equivalent fastpath: if no
    // prescribed displacement is non-zero, Ff = F (mapped to free DOFs). The slow path is
    // EXACTLY the original code so prescribed-displacement fixtures (F53 tip Dirichlet, F58
    // patch warping) stay bit-identical.
    // R2.2 PERF: lazy-build NodeId map (one-shot per session lifetime; fingerprint guards stability).
    if (p_->nodeIdToIdx.empty() && !model.nodes.empty()) {
        p_->nodeIdToIdx.reserve(model.nodes.size() * 2);
        for (size_t k = 0; k < model.nodes.size(); ++k)
            p_->nodeIdToIdx.emplace(model.nodes[k].id, static_cast<int>(k));
    }
    auto nodeIdxFast = [&](NodeId id) -> int {
        const auto it = p_->nodeIdToIdx.find(id);
        return (it == p_->nodeIdToIdx.end()) ? -1 : it->second;
    };

    SN_TIME_BEGIN(rhs);
    VecX F = VecX::Zero(N);
    for (const auto& nl : model.nodalLoads) {
        const int ni = nodeIdxFast(nl.node);                        // R2.2: O(1) via cache
        if (ni < 0) continue;
        for (int d = 0; d < 6; ++d) F(gdof(ni, d)) += nl.comp[d];
    }
    SN_TIME_BEGIN(rhsEq);
    for (const auto& el : S.elems) el->addEquivalentNodalLoads(F);
    SN_TIME_END(rhsEq, p_->lastT.rhsEqMs);

    bool hasNonZeroPresc = false;
    std::vector<real> presc((size_t)N, 0.0);
    for (size_t k = 0; k < model.nodes.size(); ++k)
        for (int d = 0; d < 6; ++d)
            if (model.nodes[k].fixed[d]) {
                const real v = model.nodes[k].prescribed[d];
                presc[(size_t)gdof((int)k, d)] = v;
                if (v != real(0)) hasNonZeroPresc = true;
            }

    VecX Ff = VecX::Zero(S.nf);
    SN_TIME_BEGIN(rhsK);
    if (hasNonZeroPresc) {
        for (int c = 0; c < N; ++c)
            for (SpMat::InnerIterator it(S.K, c); it; ++it) {
                const int r = it.row();
                if (S.fmap[r] < 0) continue;
                if (S.fmap[c] < 0 && presc[(size_t)c] != 0.0) Ff(S.fmap[r]) -= it.value() * presc[(size_t)c];
            }
    }
    SN_TIME_END(rhsK, p_->lastT.rhsKMs);
    for (int g = 0; g < N; ++g) if (S.fmap[g] >= 0) Ff(S.fmap[g]) += F(g);
    SN_TIME_END(rhs, p_->lastT.rhsMs);

    // ---- solve: reuse the PREBUILT supernodal factor (the ONLY departure from solveLoad) --
    VecX uf;
    bool used = false;
    int  irApplied = 0;
    double irFinalRes = 0.0;
#if FRAMECORE_SUPERNODAL
    if (p_->snReady) {
        const int n = S.nf;
        // R2.1 PERF-01: route to the external (PreparedSystem-owned) factor when the session
        // was built on a supernodal-primary PreparedSystem; otherwise use the session's own.
        const sn::SnSuper&    fac = p_->useExternalFac ? S.snFac : p_->fac;
        const sn::SnSymbolic& sym = p_->useExternalFac ? S.snSym : p_->sym;
        std::vector<double> b((size_t)n), x((size_t)n);
        for (int i = 0; i < n; ++i) b[(size_t)i] = static_cast<double>(Ff(i));
        SN_TIME_BEGIN(bsub);
#if defined(FRAMECORE_CUDA) && FRAMECORE_CUDA
        bool gpuSolved = false;
        if (p_->gpuReady && p_->gpuNf == n) {
            // Upload b, run PHASE_SOLVE on GPU, download x.
            if (cudaMemcpy(p_->d_b, b.data(), sizeof(double) * n, cudaMemcpyHostToDevice) == cudaSuccess) {
                const cudssStatus_t sS = cudssExecute(p_->cudssHandle, CUDSS_PHASE_SOLVE,
                                                       p_->cudssCfg, p_->cudssDataS,
                                                       p_->cuK, p_->cuX, p_->cuB);
                if (sS == CUDSS_STATUS_SUCCESS &&
                    cudaMemcpy(x.data(), p_->d_x, sizeof(double) * n, cudaMemcpyDeviceToHost) == cudaSuccess &&
                    cudaDeviceSynchronize() == cudaSuccess)
                {
                    gpuSolved = true;
                }
            }
        }
        if (!gpuSolved) {
            sn::solveSuper(fac, sym, b.data(), x.data());   // CPU fallback
        }
#else
        sn::solveSuper(fac, sym, b.data(), x.data());   // forward/back subst on the reused factor
#endif
        SN_TIME_END(bsub, p_->lastT.backsubMs);

        // R2: Neumaier-compensated IR. Cache populated only when opts.irSteps>0 at ctor, so the
        // empty check below also gates "session was built without IR, ignore the runtime opt".
        SN_TIME_BEGIN(ir);
        if (p_->opts.irSteps > 0 && !p_->Ap.empty()) {
            const double bNorm = sn::infNorm(n, b.data());
            const double absTol = (p_->opts.irTol > 0.0) ? (p_->opts.irTol * bNorm) : 0.0;
            std::vector<double> r((size_t)n), d((size_t)n);
            for (int k = 0; k < p_->opts.irSteps; ++k) {
                sn::neumaierResidualFullSym(n, p_->Ap.data(), p_->Ai.data(), p_->Ax.data(),
                                            b.data(), x.data(), r.data());
                irFinalRes = sn::infNorm(n, r.data());
                if (absTol > 0.0 && irFinalRes <= absTol) break;
                sn::solveSuper(fac, sym, r.data(), d.data());
                for (int i = 0; i < n; ++i) x[(size_t)i] += d[(size_t)i];
                ++irApplied;
            }
        }

        SN_TIME_END(ir, p_->lastT.irMs);
        uf.resize(n);
        for (int i = 0; i < n; ++i) uf(i) = static_cast<real>(x[(size_t)i]);
        if (uf.allFinite()) used = true;
    }
#endif

    std::string note;
    if (used) {
        note = "[SnSession] supernodal solve (reused factor)";
        if (irApplied > 0) {
            note += " + IR" + std::to_string(irApplied) + "/" + std::to_string(p_->opts.irSteps);
            if (p_->opts.irTol > 0.0) note += " (resInf=" + std::to_string(irFinalRes) + ")";
        }
    } else {
        if (p_->snReady && p_->opts.enabled && !p_->opts.fallbackOnFail) {
            R.singular = true;
            R.diagnostic = "SnSession::solveFrame: supernodal solve non-finite; fallback disabled";
            return R;
        }
#if FRAMECORE_SUPERNODAL
        // R2.1 PERF-01: supernodal-primary PreparedSystems never computed the LDLT factor,
        // so the LDLT fallback is unavailable. Surface a clean diagnostic instead of UB.
        if (S.useSnPrimary) {
            R.singular = true;
            R.diagnostic = "SnSession::solveFrame: supernodal failed and LDLT fallback unavailable "
                           "(PreparedSystem was built with useSupernodalPrimary=true)";
            return R;
        }
#endif
        uf = S.ldlt.solve(Ff);                      // the oracle / safety net
        if (S.ldlt.info() != Eigen::Success || !uf.allFinite()) {
            R.singular = true;
            R.diagnostic = "SnSession::solveFrame: LDLT fallback produced non-finite displacements (mechanism)";
            return R;
        }
        note = p_->snReady ? "[SnSession] LDLT (supernodal solve non-finite)"
                           : std::string("[SnSession] LDLT (") + p_->diag + ")";
    }

    // ---- scatter / reactions / recover: verbatim from solveLoad ---------------------
    SN_TIME_BEGIN(scat);
    VecX u = VecX::Zero(N);
    for (int g = 0; g < N; ++g) u(g) = (S.fmap[g] >= 0) ? uf(S.fmap[g]) : presc[(size_t)g];
    for (int g = 0; g < N; ++g) R.u[(size_t)g] = u(g);
    SN_TIME_END(scat, p_->lastT.scatterMs);

    SN_TIME_BEGIN(spmv);
#if defined(FRAMECORE_CUDA) && FRAMECORE_CUDA
    bool gpuSpmvDone = false;
    if (p_->gpuReactionsReady && p_->gpuFullN == N) {
        // Upload u (full N) + F (full N, will be overwritten with K*u - F = reactions).
        // SpMV uses beta=-1.0 + the existing F vector buffer so y_out = K*u - F in place.
        std::vector<double> u_h((size_t)N);
        std::vector<double> F_h((size_t)N);
        for (int g = 0; g < N; ++g) { u_h[(size_t)g] = (double)u(g); F_h[(size_t)g] = (double)F(g); }
        if (cudaMemcpy(p_->d_u_full, u_h.data(), sizeof(double) * N, cudaMemcpyHostToDevice) == cudaSuccess &&
            cudaMemcpy(p_->d_F_full, F_h.data(), sizeof(double) * N, cudaMemcpyHostToDevice) == cudaSuccess)
        {
            const double alpha = 1.0, beta = -1.0;
            const cusparseStatus_t st = cusparseSpMV(p_->cusparseHdl, CUSPARSE_OPERATION_NON_TRANSPOSE,
                                                      &alpha, p_->cuKFull, p_->cuVecU, &beta, p_->cuVecR,
                                                      CUDA_R_64F, CUSPARSE_SPMV_ALG_DEFAULT, p_->d_spmvWork);
            if (st == CUSPARSE_STATUS_SUCCESS) {
                std::vector<double> r_h((size_t)N);
                if (cudaMemcpy(r_h.data(), p_->d_F_full, sizeof(double) * N, cudaMemcpyDeviceToHost) == cudaSuccess &&
                    cudaDeviceSynchronize() == cudaSuccess)
                {
                    for (int g = 0; g < N; ++g) R.reactions[(size_t)g] = (real)r_h[(size_t)g];
                    gpuSpmvDone = true;
                }
            }
        }
    }
    if (!gpuSpmvDone) {
        const VecX Rv = S.K * u - F;
        for (int g = 0; g < N; ++g) R.reactions[(size_t)g] = Rv(g);
    }
#else
    const VecX Rv = S.K * u - F;
    for (int g = 0; g < N; ++g) R.reactions[(size_t)g] = Rv(g);
#endif
    SN_TIME_END(spmv, p_->lastT.spmvMs);

    SN_TIME_BEGIN(rec);
    if (!p_->opts.skipForceRecovery) {
        R.memberForces.resize(model.members.size());
        R.shellForces.resize(model.shells.size());
        for (size_t e = 0; e < model.members.size(); ++e) R.memberForces[e].member = model.members[e].id;
        for (size_t s = 0; s < model.shells.size(); ++s)  R.shellForces[s].shell   = model.shells[s].id;
        for (const auto& el : S.elems) el->recover(u, R);
    } else {
        // R2.2 lazy-recover: callers requested only u + reactions. R.memberForces /
        // R.shellForces stay empty; downstream readers must check .empty() and re-solve
        // with skipForceRecovery=false if they need stress resultants.
        note += " [lazy-recover]";
    }
    SN_TIME_END(rec, p_->lastT.recoverMs);
    R.pivotMargin = S.pivotMargin;
    R.diagnostic = note;
    SN_TIME_END(total, p_->lastT.totalMs);
    return R;
}

SnSessionTimings SnSession::lastTimings() const {
    return p_ ? p_->lastT : SnSessionTimings{};
}

} // namespace frame
