#pragma once
// sn_chol.h - research-only self-built supernodal Cholesky (BLAS3 via OpenBLAS).
//
// PHASE (this file, M1 step 1): SYMBOLIC ONLY -- fill-reducing ordering (METIS) +
//   elimination tree + symbolic factorization (L nonzero pattern / column counts).
//   Numeric (supernode amalgamation + left-looking BLAS3 factor) and solve come next.
//
// Deps: METIS (ordering) this phase; cblas/lapacke (OpenBLAS) added for numeric.
// NO Eigen -- input is a raw FULL-symmetric CSC (col-major), exactly how Eigen's
// SparseMatrix<double> stores K_ff. CHOLMOD is only an external oracle in the driver.

#include <metis.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <utility>
#include <iterator>
#include <cblas.h>                 // OpenBLAS BLAS3 (dgemm/dtrsm/dtrsv/dgemv) for the supernodal path
#include <cassert>
// lapacke.h pulls in C99 _Complex (invalid in MSVC C++); declare the one routine we use directly.
extern "C" int LAPACKE_dpotrf(int matrix_layout, char uplo, int n, double* a, int lda);
#ifndef LAPACK_COL_MAJOR
#define LAPACK_COL_MAJOR 102
#endif

namespace sn {

struct SnSymbolic {
    int  n = 0;
    bool usedMetis = false;
    std::vector<int> perm, iperm;        // perm[new]=old, iperm[old]=new
    std::vector<int> Ap, Ai;             // permuted FULL symmetric CSC structure (col-major)
    std::vector<int> parent;             // elimination tree (permuted order)
    std::vector<int> colcnt;             // nnz per column of L (incl diagonal), permuted
    long long        Lnnz = 0;           // total nnz(L) incl diagonal
    std::vector<std::vector<int>> Lpat;  // L pattern per column (rows>=col, sorted) -- prototype
                                         // storage; numeric phase consumes/compresses it.
};

namespace detail {

// Permute a full-symmetric CSC by iperm (old->new); output full-symmetric permuted CSC.
inline void permuteFull(int n, const int* op, const int* oi, const std::vector<int>& iperm,
                        std::vector<int>& Ap, std::vector<int>& Ai) {
    std::vector<std::vector<int>> cols(n);
    for (int j = 0; j < n; ++j) {
        const int nj = iperm[j];
        for (int p = op[j]; p < op[j + 1]; ++p)
            cols[nj].push_back(iperm[oi[p]]);            // keep both triangles
    }
    Ap.assign(n + 1, 0);
    for (int j = 0; j < n; ++j) {
        auto& c = cols[j];
        std::sort(c.begin(), c.end());
        c.erase(std::unique(c.begin(), c.end()), c.end());
        Ap[j + 1] = Ap[j] + static_cast<int>(c.size());
    }
    Ai.resize(Ap[n]);
    for (int j = 0; j < n; ++j) std::copy(cols[j].begin(), cols[j].end(), Ai.begin() + Ap[j]);
}

// Elimination tree from full-symmetric CSC (Davis cs_etree; uses strictly-upper i<k entries).
inline void etree(int n, const std::vector<int>& Ap, const std::vector<int>& Ai,
                  std::vector<int>& parent) {
    parent.assign(n, -1);
    std::vector<int> anc(n, -1);
    for (int k = 0; k < n; ++k)
        for (int p = Ap[k]; p < Ap[k + 1]; ++p) {
            int i = Ai[p];
            while (i != -1 && i < k) {
                const int inext = anc[i];
                anc[i] = k;
                if (inext == -1) parent[i] = k;
                i = inext;
            }
        }
}

// Symbolic factorization: L(:,j) pattern = A(j:,j) lower  UNION  children's patterns (rows>=j).
// Children processed before j (natural increasing order), so each Lpat[c] is already complete.
inline void symbolic(int n, const std::vector<int>& Ap, const std::vector<int>& Ai,
                     const std::vector<int>& parent,
                     std::vector<std::vector<int>>& Lpat, std::vector<int>& colcnt,
                     long long& Lnnz) {
    std::vector<std::vector<int>> children(n);
    for (int j = 0; j < n; ++j) if (parent[j] >= 0) children[parent[j]].push_back(j);
    Lpat.assign(n, {});
    colcnt.assign(n, 0);
    Lnnz = 0;
    for (int j = 0; j < n; ++j) {
        auto& pat = Lpat[j];
        for (int p = Ap[j]; p < Ap[j + 1]; ++p) { const int i = Ai[p]; if (i >= j) pat.push_back(i); }
        for (int c : children[j]) for (int i : Lpat[c]) if (i >= j) pat.push_back(i);
        std::sort(pat.begin(), pat.end());
        pat.erase(std::unique(pat.begin(), pat.end()), pat.end());
        colcnt[j] = static_cast<int>(pat.size());
        Lnnz += colcnt[j];
    }
}

// Fill-reducing ordering via METIS nested dissection. Builds an undirected graph (no self loops)
// from the full-symmetric structure. Falls back to natural ordering if METIS fails.
inline void metisOrder(int n, const int* op, const int* oi,
                       std::vector<int>& perm, std::vector<int>& iperm, bool& ok) {
    std::vector<idx_t> deg(n, 0);
    for (int j = 0; j < n; ++j)
        for (int p = op[j]; p < op[j + 1]; ++p) if (oi[p] != j) deg[j]++;
    std::vector<idx_t> xadj(n + 1, 0);
    for (int k = 0; k < n; ++k) xadj[k + 1] = xadj[k] + deg[k];
    std::vector<idx_t> adjncy(static_cast<size_t>(xadj[n]));
    std::vector<idx_t> pos(xadj.begin(), xadj.end() - 1);
    for (int j = 0; j < n; ++j)
        for (int p = op[j]; p < op[j + 1]; ++p) { const int i = oi[p]; if (i != j) adjncy[pos[j]++] = i; }
    idx_t nv = n;
    std::vector<idx_t> mp(n), mip(n);
    const int st = METIS_NodeND(&nv, xadj.data(), adjncy.data(), nullptr, nullptr, mp.data(), mip.data());
    perm.resize(n); iperm.resize(n);
    ok = (st == METIS_OK);
    if (!ok) { for (int i = 0; i < n; ++i) perm[i] = iperm[i] = i; return; }
    for (int i = 0; i < n; ++i) { perm[i] = static_cast<int>(mp[i]); iperm[i] = static_cast<int>(mip[i]); }
}

} // namespace detail

// Symbolic analysis. Input: FULL-symmetric CSC (col-major) -- exactly Eigen SparseMatrix layout.
// useMetis=false -> natural ordering, for an EXACT cross-check of fill vs Eigen NaturalOrdering.
inline SnSymbolic analyze(int n, const int* outerPtr, const int* innerIdx, bool useMetis = true) {
    SnSymbolic s;
    s.n = n;
    if (useMetis) {
        detail::metisOrder(n, outerPtr, innerIdx, s.perm, s.iperm, s.usedMetis);
    } else {
        s.perm.resize(n); s.iperm.resize(n);
        for (int i = 0; i < n; ++i) s.perm[i] = s.iperm[i] = i;
        s.usedMetis = false;
    }
    detail::permuteFull(n, outerPtr, innerIdx, s.iperm, s.Ap, s.Ai);
    detail::etree(s.n, s.Ap, s.Ai, s.parent);
    detail::symbolic(s.n, s.Ap, s.Ai, s.parent, s.Lpat, s.colcnt, s.Lnnz);
    return s;
}

// ---- Numeric (M1 step 2a): column-based left-looking Cholesky ----
// Correctness reference. Supernodal dense-panel factor via OpenBLAS BLAS3 is the next step
// (M1 step 2b); this version is scalar/sparse and validates the numeric logic against CHOLMOD.
struct SnFactor {
    int n = 0;
    std::vector<int>    Lp, Li;   // L CSC (permuted, lower incl diagonal; diagonal is first in each col)
    std::vector<double> Lx;       // L values (diagonal holds the Cholesky pivot, not 1)
    bool spd = true;
};

inline SnFactor factorize(int n, const int* outerPtr, const int* innerIdx,
                          const double* values, const SnSymbolic& sym) {
    SnFactor f;
    f.n = n;
    f.Lp.assign(n + 1, 0);
    for (int j = 0; j < n; ++j) f.Lp[j + 1] = f.Lp[j] + static_cast<int>(sym.Lpat[j].size());
    f.Li.resize(f.Lp[n]);
    for (int j = 0; j < n; ++j) std::copy(sym.Lpat[j].begin(), sym.Lpat[j].end(), f.Li.begin() + f.Lp[j]);
    f.Lx.assign(f.Lp[n], 0.0);

    // permuted lower A values: take each entry once, where iperm[i] >= iperm[j] (maps to lower)
    std::vector<std::vector<std::pair<int, double>>> acol(n);
    for (int j = 0; j < n; ++j)
        for (int p = outerPtr[j]; p < outerPtr[j + 1]; ++p) {
            const int ni = sym.iperm[innerIdx[p]], nj = sym.iperm[j];
            if (ni >= nj) acol[nj].push_back({ni, values[p]});
        }

    // transpose of L off-diagonal: rowOf[i] = columns k<i with L(i,k)!=0 (left-looking sources)
    std::vector<std::vector<int>> rowOf(n);
    for (int k = 0; k < n; ++k)
        for (int p = f.Lp[k] + 1; p < f.Lp[k + 1]; ++p) rowOf[f.Li[p]].push_back(k);

    std::vector<double> x(n, 0.0);                       // dense accumulator
    for (int j = 0; j < n; ++j) {
        for (auto& e : acol[j]) x[e.first] += e.second;  // scatter A(:,j) lower
        for (int k : rowOf[j]) {                          // cmod: subtract column k's contribution
            int pj = f.Lp[k];
            const int hi = f.Lp[k + 1];
            while (pj < hi && f.Li[pj] < j) ++pj;         // locate row j in column k (Li sorted)
            const double ljk = f.Lx[pj];
            for (int p = pj; p < hi; ++p) x[f.Li[p]] -= f.Lx[p] * ljk;
        }
        const int dj = f.Lp[j];                           // diagonal entry (row j) is first
        double d = x[j];
        if (d <= 0.0) { f.spd = false; d = (d == 0.0) ? 1e-300 : -d; }
        const double djj = std::sqrt(d);
        f.Lx[dj] = djj;
        x[j] = 0.0;
        for (int p = dj + 1; p < f.Lp[j + 1]; ++p) { const int i = f.Li[p]; f.Lx[p] = x[i] / djj; x[i] = 0.0; }
    }
    return f;
}

// Solve A x = b in ORIGINAL ordering (b, x length n).
inline void solve(const SnFactor& f, const SnSymbolic& sym, const double* b, double* x) {
    const int n = f.n;
    std::vector<double> y(n);
    for (int i = 0; i < n; ++i) y[sym.iperm[i]] = b[i];   // permute RHS into factor order
    for (int j = 0; j < n; ++j) {                          // forward: L y = b
        const int dj = f.Lp[j];
        const double yj = y[j] / f.Lx[dj];
        y[j] = yj;
        for (int p = dj + 1; p < f.Lp[j + 1]; ++p) y[f.Li[p]] -= f.Lx[p] * yj;
    }
    for (int j = n - 1; j >= 0; --j) {                     // backward: L^T x = y
        const int dj = f.Lp[j];
        double s = y[j];
        for (int p = dj + 1; p < f.Lp[j + 1]; ++p) s -= f.Lx[p] * y[f.Li[p]];
        y[j] = s / f.Lx[dj];
    }
    for (int i = 0; i < n; ++i) x[i] = y[sym.iperm[i]];   // unpermute
}

// ---- Numeric (M1 step 2b): supernodal left-looking Cholesky, dense panels via OpenBLAS BLAS3 ----
// Fundamental supernodes detected as runs of consecutive columns with nested L pattern (relaxed:
// no postorder, so an unfavourable ordering just yields smaller supernodes -- still correct).
// Each supernode is a dense nrow x ncol col-major panel; updates use dgemm, diagonal dpotrf,
// off-diagonal dtrsm. This is the speed path; the column-based factorize() stays as a cross-oracle.
struct SnSuper {
    int n = 0, nsn = 0;
    std::vector<int> c0, ncol, nrow;        // per supernode: first pivot col, width, panel rows
    std::vector<std::vector<int>>    rows;  // global row indices per supernode (rows[0..ncol)=pivots)
    std::vector<std::vector<double>> data;  // nrow*ncol col-major factored panel
    std::vector<int> snOf;                  // column -> supernode index
    bool spd = true;
};

inline SnSuper factorizeSuper(int n, const int* outerPtr, const int* innerIdx,
                              const double* values, const SnSymbolic& sym,
                              int amalgRelax = 0, int amalgMaxCol = 64) {
    SnSuper S;
    S.n = n;
    // fundamental supernodes: column j continues j-1 iff Lpat[j-1] == {j-1} ++ Lpat[j]
    std::vector<int> snStart;
    snStart.push_back(0);
    for (int j = 1; j < n; ++j) {
        const auto& pj = sym.Lpat[j];
        const auto& pp = sym.Lpat[j - 1];
        bool cont = (pp.size() == pj.size() + 1) && pp.size() >= 2 && pp[1] == j;
        if (cont)
            for (size_t t = 0; t < pj.size(); ++t)
                if (pp[t + 1] != pj[t]) { cont = false; break; }
        if (!cont) snStart.push_back(j);
    }
    snStart.push_back(n);
    // amalgamation: greedily merge consecutive supernodes (padding the panel with explicit zeros)
    // when the merge adds few rows (<= amalgRelax) and stays within amalgMaxCol columns. Bigger
    // panels -> dgemm closer to peak. relax=0 leaves fundamental supernodes untouched.
    if (amalgRelax > 0 && static_cast<int>(snStart.size()) > 2) {
        const int nf = static_cast<int>(snStart.size()) - 1;
        std::vector<int> merged;
        merged.push_back(snStart[0]);
        int curStart = snStart[0];
        std::vector<int> curRows = sym.Lpat[curStart];
        for (int s = 1; s < nf; ++s) {
            const int candC0 = snStart[s], candC1 = snStart[s + 1];
            std::vector<int> u;
            std::set_union(curRows.begin(), curRows.end(),
                           sym.Lpat[candC0].begin(), sym.Lpat[candC0].end(), std::back_inserter(u));
            const int mergedNcol = candC1 - curStart;
            const int addedRows  = static_cast<int>(u.size()) - static_cast<int>(curRows.size());
            if (mergedNcol <= amalgMaxCol && addedRows <= amalgRelax) {
                curRows.swap(u);
            } else {
                merged.push_back(candC0); curStart = candC0; curRows = sym.Lpat[candC0];
            }
        }
        merged.push_back(n);
        snStart.swap(merged);
    }
    S.nsn = static_cast<int>(snStart.size()) - 1;
    S.c0.resize(S.nsn); S.ncol.resize(S.nsn); S.nrow.resize(S.nsn);
    S.rows.resize(S.nsn); S.data.resize(S.nsn); S.snOf.assign(n, -1);
    for (int s = 0; s < S.nsn; ++s) {
        const int c0 = snStart[s], c1 = snStart[s + 1];
        S.c0[s] = c0; S.ncol[s] = c1 - c0;
        std::vector<int> r = sym.Lpat[c0];                          // panel rows = union of member patterns
        for (int c = c0 + 1; c < c1; ++c) {
            std::vector<int> u;
            std::set_union(r.begin(), r.end(), sym.Lpat[c].begin(), sym.Lpat[c].end(), std::back_inserter(u));
            r.swap(u);
        }
        S.nrow[s] = static_cast<int>(r.size());
        S.rows[s] = std::move(r);
        S.data[s].assign(static_cast<size_t>(S.nrow[s]) * S.ncol[s], 0.0);
        for (int c = c0; c < c1; ++c) S.snOf[c] = s;
    }
    // permuted lower A values (each entry once: iperm[i] >= iperm[j])
    std::vector<std::vector<std::pair<int, double>>> acol(n);
    for (int j = 0; j < n; ++j)
        for (int p = outerPtr[j]; p < outerPtr[j + 1]; ++p) {
            const int ni = sym.iperm[innerIdx[p]], nj = sym.iperm[j];
            if (ni >= nj) acol[nj].push_back({ni, values[p]});
        }
    // updaters[J] = supernodes K whose below-diagonal rows touch J's pivot columns
    std::vector<std::vector<int>> updaters(S.nsn);
    for (int K = 0; K < S.nsn; ++K) {
        int last = -1;
        for (int t = S.ncol[K]; t < S.nrow[K]; ++t) {
            const int J = S.snOf[S.rows[K][t]];
            if (J != last) { updaters[J].push_back(K); last = J; }
        }
    }
    std::vector<int> rowpos(n, -1);
    std::vector<double> U;
    for (int J = 0; J < S.nsn; ++J) {
        const int c0 = S.c0[J], nc = S.ncol[J], nr = S.nrow[J];
        auto& Jd = S.data[J];
        const auto& Jr = S.rows[J];
        for (int t = 0; t < nr; ++t) rowpos[Jr[t]] = t;
        for (int c = c0; c < c0 + nc; ++c) {                        // scatter A(:,c) into panel
            const int relc = c - c0;
            for (auto& e : acol[c]) Jd[(size_t)rowpos[e.first] + (size_t)relc * nr] += e.second;
        }
        for (int K : updaters[J]) {                                 // left-looking BLAS3 updates
            const int nck = S.ncol[K], nrk = S.nrow[K];
            const auto& Kd = S.data[K];
            const auto& Kr = S.rows[K];
            int prow0 = nck; while (prow0 < nrk && Kr[prow0] < c0) ++prow0;   // first below-row >= c0
            const int mrow = nrk - prow0;
            if (mrow <= 0) continue;
            int mcol = 0; while (prow0 + mcol < nrk && Kr[prow0 + mcol] < c0 + nc) ++mcol;
            U.assign(static_cast<size_t>(mrow) * mcol, 0.0);
            // U = RowMat(mrow x nck) * ColMat(mcol x nck)^T ; both are Kd sub-blocks from row prow0
            cblas_dgemm(CblasColMajor, CblasNoTrans, CblasTrans, mrow, mcol, nck,
                        1.0, Kd.data() + prow0, nrk, Kd.data() + prow0, nrk, 0.0, U.data(), mrow);
            for (int jj = 0; jj < mcol; ++jj) {
                const int relc = Kr[prow0 + jj] - c0;               // J pivot column (relative)
                for (int ii = 0; ii < mrow; ++ii) {
                    const int rp = rowpos[Kr[prow0 + ii]];
                    // A zero-padded amalgamation row outside R_J has a provably-zero Schur
                    // contribution (else some K column would fill it into R_J), so skip it.
                    if (rp < 0) continue;
                    Jd[(size_t)rp + (size_t)relc * nr] -= U[(size_t)ii + (size_t)jj * mrow];
                }
            }
        }
        const int info = LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', nc, Jd.data(), nr);   // lda = nr
        if (info != 0) S.spd = false;
        const int noff = nr - nc;
        if (noff > 0)
            cblas_dtrsm(CblasColMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
                        noff, nc, 1.0, Jd.data(), nr, Jd.data() + nc, nr);
        for (int t = 0; t < nr; ++t) rowpos[Jr[t]] = -1;
    }
    return S;
}

// Solve A x = b in ORIGINAL ordering, using the supernodal panels.
inline void solveSuper(const SnSuper& S, const SnSymbolic& sym, const double* b, double* x) {
    const int n = S.n;
    std::vector<double> y(n), tmp;
    for (int i = 0; i < n; ++i) y[sym.iperm[i]] = b[i];
    for (int J = 0; J < S.nsn; ++J) {                               // forward L y = b
        const int c0 = S.c0[J], nc = S.ncol[J], nr = S.nrow[J], noff = nr - nc;
        const auto& d = S.data[J]; const auto& r = S.rows[J];
        cblas_dtrsv(CblasColMajor, CblasLower, CblasNoTrans, CblasNonUnit, nc, d.data(), nr, &y[c0], 1);
        if (noff > 0) {
            tmp.assign(noff, 0.0);
            cblas_dgemv(CblasColMajor, CblasNoTrans, noff, nc, 1.0, d.data() + nc, nr, &y[c0], 1, 0.0, tmp.data(), 1);
            for (int t = 0; t < noff; ++t) y[r[nc + t]] -= tmp[t];
        }
    }
    for (int J = S.nsn - 1; J >= 0; --J) {                          // backward L^T x = y
        const int c0 = S.c0[J], nc = S.ncol[J], nr = S.nrow[J], noff = nr - nc;
        const auto& d = S.data[J]; const auto& r = S.rows[J];
        if (noff > 0) {
            tmp.assign(noff, 0.0);
            for (int t = 0; t < noff; ++t) tmp[t] = y[r[nc + t]];
            cblas_dgemv(CblasColMajor, CblasTrans, noff, nc, -1.0, d.data() + nc, nr, tmp.data(), 1, 1.0, &y[c0], 1);  // y_diag -= L_od^T x_below
        }
        cblas_dtrsv(CblasColMajor, CblasLower, CblasTrans, CblasNonUnit, nc, d.data(), nr, &y[c0], 1);
    }
    for (int i = 0; i < n; ++i) x[i] = y[sym.iperm[i]];
}

} // namespace sn
