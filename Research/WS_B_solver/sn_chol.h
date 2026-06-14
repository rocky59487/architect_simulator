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

} // namespace sn
