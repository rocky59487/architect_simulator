// exp_openblas_smoke.cpp — M0 de-risk (research-only, NOT engine): can OpenBLAS's BLAS3 be
// MSVC-linked + give correct results + decent GFLOPS on this AMD CPU? These are exactly the
// kernels a self-built supernodal Cholesky needs: dgemm (trailing update), dpotrf (diag block
// Cholesky), dtrsm (off-diag solve), dsyrk (symmetric rank-k update). PASS = all link, run,
// residual <= 1e-9, dpotrf info 0, dgemm GFLOPS above a sanity floor.

#include <cblas.h>
// lapacke.h pulls in lapack.h, which declares complex routines with C99 `_Complex` — invalid
// in MSVC C++. We only need ONE LAPACK routine (dpotrf, real double), so declare its standard
// LAPACKE C prototype directly and skip the poisoned header. Symbol lives in openblas.lib.
#define LAPACK_COL_MAJOR 102
extern "C" int LAPACKE_dpotrf(int matrix_layout, char uplo, int n, double* a, int lda);
#include <cstdio>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <vector>

using Clock = std::chrono::steady_clock;
static double sinceMs(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// dgemm GFLOPS: C = A*B, n x n row-major, FLOP = 2 n^3
static double benchDgemm(int n, int reps) {
    std::vector<double> A(static_cast<size_t>(n) * n, 1.0 / n),
                        B(static_cast<size_t>(n) * n, 1.0 / n),
                        C(static_cast<size_t>(n) * n, 0.0);
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, n, n, n,
                1.0, A.data(), n, B.data(), n, 0.0, C.data(), n);   // warm-up
    const auto t0 = Clock::now();
    for (int i = 0; i < reps; ++i)
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, n, n, n,
                    1.0, A.data(), n, B.data(), n, 0.0, C.data(), n);
    const double ms = sinceMs(t0) / reps;
    return 2.0 * n * n * n / (ms * 1e-3) / 1e9;
}

int main() {
    int bad = 0;

    // 1) dgemm GFLOPS (default thread count)
    {
        const double g = benchDgemm(1024, 5);
        std::printf("[ob-smoke] cblas_dgemm n=1024: %.1f GFLOPS\n", g);
        if (g < 5.0) ++bad;   // sanity floor; a working BLAS3 on this CPU is well above this
    }

    // 2) dpotrf: SPD A (strongly diag-dominant), col-major lower Cholesky
    {
        const int n = 256;
        std::vector<double> A(static_cast<size_t>(n) * n, 0.0);
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i)
                A[i + static_cast<size_t>(j) * n] = (i == j) ? static_cast<double>(n + 1) : 0.05;
        const int info = LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', n, A.data(), n);
        double minDiag = 1e300;
        for (int i = 0; i < n; ++i) minDiag = std::min(minDiag, A[i + static_cast<size_t>(i) * n]);
        std::printf("[ob-smoke] LAPACKE_dpotrf n=256: info=%d minDiagL=%.3f\n", info, minDiag);
        if (info != 0 || minDiag <= 0) ++bad;
    }

    // 3) dtrsm: factor A=LL^T then solve A*Y=B via two triangular solves; check residual.
    {
        const int n = 200, nrhs = 16;
        std::vector<double> A(static_cast<size_t>(n) * n, 0.0);
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i)
                A[i + static_cast<size_t>(j) * n] = (i == j) ? static_cast<double>(n + 1) : 0.1;
        std::vector<double> L = A;
        LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', n, L.data(), n);
        std::vector<double> B(static_cast<size_t>(n) * nrhs), Y;
        for (int k = 0; k < n * nrhs; ++k) B[k] = std::sin(0.1 * k);
        Y = B;
        // L*(L^T*Y) = B  ->  forward then backward
        cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasNoTrans, CblasNonUnit,
                    n, nrhs, 1.0, L.data(), n, Y.data(), n);
        cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasTrans, CblasNonUnit,
                    n, nrhs, 1.0, L.data(), n, Y.data(), n);
        double num = 0, den = 0;
        for (int c = 0; c < nrhs; ++c)
            for (int i = 0; i < n; ++i) {
                double av = 0;
                for (int j = 0; j < n; ++j) av += A[i + static_cast<size_t>(j) * n] * Y[j + static_cast<size_t>(c) * n];
                const double d = av - B[i + static_cast<size_t>(c) * n];
                num += d * d; den += B[i + static_cast<size_t>(c) * n] * B[i + static_cast<size_t>(c) * n];
            }
        const double res = std::sqrt(num / den);
        std::printf("[ob-smoke] cblas_dtrsm (A=LL^T solve) relRes=%.2e\n", res);
        if (res > 1e-9) ++bad;
    }

    // 4) dsyrk: C = A*A^T (lower), spot-check one entry
    {
        const int n = 64, k = 32;
        std::vector<double> Am(static_cast<size_t>(n) * k);
        for (int x = 0; x < n * k; ++x) Am[x] = std::cos(0.05 * x);
        std::vector<double> C(static_cast<size_t>(n) * n, 0.0);
        cblas_dsyrk(CblasColMajor, CblasLower, CblasNoTrans, n, k,
                    1.0, Am.data(), n, 0.0, C.data(), n);
        const int ti = 10, tj = 3;
        double ref = 0;
        for (int p = 0; p < k; ++p) ref += Am[ti + static_cast<size_t>(p) * n] * Am[tj + static_cast<size_t>(p) * n];
        const double got = C[ti + static_cast<size_t>(tj) * n];
        std::printf("[ob-smoke] cblas_dsyrk spot diff=%.2e\n", std::abs(got - ref));
        if (std::abs(got - ref) > 1e-9) ++bad;
    }

    std::printf(bad == 0 ? "[ob-smoke] PASS (OpenBLAS BLAS3 links + correct)\n"
                         : "[ob-smoke] FAIL (%d check(s) bad)\n", bad);
    return bad == 0 ? 0 : 1;
}
