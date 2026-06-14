//
// Seeded HP-FEM solve session (A2c: serial matrix-free PCG + Jacobi + Galerkin projection
// onto a seeded load-response basis, frame-only, LDLT fallback). See Public/FrameCore/HpSession.h.
//
// Design contract: solveFrame must return a SolveResult bit-equivalent to solveLoad() except for
// the solve step itself. The RHS assembly, prescribed reduction, scatter, reactions (K*u - F) and
// element recovery below are therefore copied verbatim from FrameSolver.cpp::solveLoad (mirrored in
// HpSolver.cpp::solveLoadHP) — the ONLY departure is replacing `S.ldlt.solve(Ff)` with a seeded
// Galerkin projection + matrix-free preconditioned conjugate gradient. The LDLT factor remains the
// oracle and the fallback. Eigen is confined to this Private .cpp; the public header is POD.
//
// A2c is serial: it reuses A1's per-element reduced operator (ElemReduced) and Jacobi diagonal.
// A2a will swap the apply/precond internals (ThreadApplyPool + parallel block6) WITHOUT changing
// HpSession.h, turning the per-frame ~14ms serial apply into the ~0.9ms 16-thread apply that makes
// the large-problem ~19x real. The RecycleBasis is ported from Research/WS_B_solver/exp_parallel_pcg.cpp.
//
#include "FrameCore/HpSession.h"
#include "PreparedSystemImpl.h"   // PreparedSystem::Impl (K/fmap/nf/ldlt/elems/...) + FrameEigen.h
#include "IElement.h"             // el->assemble / addEquivalentNodalLoads / recover / localDof
#include "ModelHash.h"            // modelFingerprint (the shared reuse-validity guard)

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace frame {

namespace {

// Per-element reduced (free-DOF) operator entries — identical to HpSolver.cpp. Summing these
// scalar contributions over all elements equals K_ff * x by construction (it uses the exact
// triplets reduceFF() builds K_ff from), so the matrix-free apply matches the LDLT operator to
// the last bit.
struct ElemReduced {
    std::vector<int>  ri, ci;
    std::vector<real> v;
};

// A-orthonormal (K-inner-product) basis of seeded load responses + Galerkin initial guess. Ported
// from Research/WS_B_solver/exp_parallel_pcg.cpp (RecycleBasis, L1062-1113), with the research-only
// timers removed; the math is unchanged. Invariant: v_i^T K v_j = delta_ij (research cross-check
// ||VtKV - I|| = 3.4e-15), so initialGuess(b) = sum_i (v_i . b) v_i is the EXACT Galerkin solution
// whenever K^{-1} b lies in span{v_i} (=> ~0 PCG iters in-subspace).
struct RecycleBasis {
    int maxSize = 0;
    std::vector<VecX> v;    // K-orthonormal responses
    std::vector<VecX> av;   // av[i] = K * v[i]
    int accepted = 0;
    int rejected = 0;

    RecycleBasis() = default;
    explicit RecycleBasis(int maxBasis) : maxSize(maxBasis) {}

    VecX initialGuess(int n, const VecX& b) const {
        VecX x = VecX::Zero(n);
        for (size_t i = 0; i < v.size(); ++i) x.noalias() += v[i].dot(b) * v[i];
        return x;
    }

    // Append a new response x (= K^{-1} b for some seed load b), A-orthonormalizing it against the
    // existing basis. `apply(q, Aq)` must set Aq = K * q (the matrix-free element apply). A linearly
    // dependent / null seed is rejected. FIFO eviction past maxSize (capacity is sized up-front to
    // hold every seed, so eviction never fires during normal seeding).
    template <typename ApplyFn>
    void add(const VecX& x, ApplyFn apply) {
        if (maxSize <= 0) return;
        VecX q = x;
        VecX Aq(x.size());
        apply(q, Aq);
        for (size_t i = 0; i < v.size(); ++i) {
            const real c = v[i].dot(Aq);
            q.noalias()  -= c * v[i];
            Aq.noalias() -= c * av[i];
        }
        const real normA2 = q.dot(Aq);
        const double scaleRef = std::max(1e-300, static_cast<double>(x.norm()));
        if (!(normA2 > real(0)) || !std::isfinite(static_cast<double>(normA2)) ||
            static_cast<double>(q.norm()) <= 1e-12 * scaleRef) {
            ++rejected;
            return;
        }
        const real inv = real(1) / std::sqrt(normA2);
        q  *= inv;
        Aq *= inv;
        if (static_cast<int>(v.size()) >= maxSize) {
            v.erase(v.begin());
            av.erase(av.begin());
        }
        v.push_back(std::move(q));
        av.push_back(std::move(Aq));
        ++accepted;
    }
};

}  // namespace

// ============================================================================
// PIMPL: non-owning view of the factorization + the seeded basis + apply/precond.
// ============================================================================
struct HpSession::Impl {
    const PreparedSystem::Impl* ps_raw = nullptr;   // NON-owning; caller guarantees it outlives us
    HpSessionOptions            opts;
    bool                        baseValid = false;   // HP path ready (non-null, non-singular, frame-only)
    std::string                 diag;
    RecycleBasis                basis;
    int                         effectiveBasisMax = 0;
    std::vector<ElemReduced>    blocks;              // per-element reduced operator (frame-only)
    VecX                        diagJ;               // Jacobi diagonal of K_ff
    bool                        hasShell = false;

    Impl(const PreparedSystem& prepared, const HpSessionOptions& o)
        : ps_raw(prepared.impl.get()), opts(o),
          basis(std::max(0, o.basisMax)), effectiveBasisMax(std::max(0, o.basisMax)) {
        if (!ps_raw) { diag = "HpSession: null PreparedSystem"; return; }
        const PreparedSystem::Impl& S = *ps_raw;
        if (S.singular) { diag = "HpSession: PreparedSystem is singular: " + S.diagnostic; return; }

        // Build the per-element reduced operator + Jacobi diagonal (verbatim from HpSolver.cpp).
        // Frame-only: a non-12-DOF element (shell) -> hasShell -> every frame routes to LDLT.
        blocks.reserve(S.elems.size());
        diagJ = VecX::Zero(S.nf);
        for (const auto& el : S.elems) {
            if (el->localDof() != 12) { hasShell = true; break; }
            std::vector<Triplet> trips;
            el->assemble(trips);
            ElemReduced b;
            b.ri.reserve(trips.size()); b.ci.reserve(trips.size()); b.v.reserve(trips.size());
            for (const auto& t : trips) {
                const int ri = S.fmap[(size_t)t.row()];
                const int ci = S.fmap[(size_t)t.col()];
                if (ri < 0 || ci < 0) continue;
                b.ri.push_back(ri); b.ci.push_back(ci); b.v.push_back(t.value());
                if (ri == ci) diagJ(ri) += t.value();
            }
            blocks.push_back(std::move(b));
        }
        if (hasShell) {
            baseValid = false;
            blocks.clear();   // partial; never used (solveFrame routes shells to LDLT)
            diag = "HpSession: shell element present (A2c is frame-only; solveFrame uses LDLT)";
        } else {
            baseValid = true;
            diag = "HpSession: ready (frame-only seeded lane)";
        }
    }

    // y = K_ff * x via the per-element reduced operator (matrix-free; bit-consistent with LDLT's K_ff).
    void applyKff(const VecX& x, VecX& y) const {
        y.setZero();
        for (const auto& b : blocks)
            for (size_t k = 0; k < b.ri.size(); ++k) y(b.ri[k]) += b.v[k] * x(b.ci[k]);
    }
    // Jacobi preconditioner z = D^{-1} r.
    void precondJacobi(const VecX& rr, VecX& zz) const {
        const int nf = ps_raw->nf;
        for (int i = 0; i < nf; ++i) zz(i) = (diagJ(i) != real(0)) ? rr(i) / diagJ(i) : rr(i);
    }
};

// ============================================================================
// Public surface.
// ============================================================================
HpSession::HpSession(const PreparedSystem& prepared, const HpSessionOptions& opts)
    : p_(std::make_unique<Impl>(prepared, opts)) {}
HpSession::~HpSession() = default;
HpSession::HpSession(HpSession&&) noexcept = default;
HpSession& HpSession::operator=(HpSession&&) noexcept = default;

bool HpSession::valid() const { return p_->baseValid; }
const std::string& HpSession::diagnostic() const { return p_->diag; }

bool HpSession::setLoadBasis(const std::vector<std::vector<real>>& loadVectors) {
    Impl& P = *p_;
    if (!P.baseValid) return false;
    const PreparedSystem::Impl& S = *P.ps_raw;
    const int N = S.N;

    // FIFO eviction would drop early seeds and break the in-subspace guarantee, so size the cap to
    // hold every seed (A1/B hit this blind spot). Rebuild the basis from scratch.
    P.effectiveBasisMax = std::max(P.opts.basisMax, (int)loadVectors.size());
    P.basis = RecycleBasis(P.effectiveBasisMax);

    for (const auto& lv : loadVectors) {
        if ((int)lv.size() != N) {
            P.diag = "HpSession::setLoadBasis: load vector size " + std::to_string(lv.size()) +
                     " != 6N (" + std::to_string(N) + ")";
            return false;
        }
        // The seed is a PURE nodal load (prescribed = 0): reduce the 6N global load to the free DOFs,
        // solve via the already-factored LDLT, and A-orthonormalize the response into the basis.
        VecX Ff = VecX::Zero(S.nf);
        for (int g = 0; g < N; ++g)
            if (S.fmap[g] >= 0) Ff(S.fmap[g]) += lv[(size_t)g];
        const VecX resp = S.ldlt.solve(Ff);
        if (S.ldlt.info() != Eigen::Success || !resp.allFinite()) continue;   // skip a degenerate seed
        P.basis.add(resp, [&](const VecX& q, VecX& Aq) { P.applyKff(q, Aq); });
    }
    return true;
}

SolveResult HpSession::solveFrame(const FrameModel& model, HpSessionStats* stats) {
    Impl& P = *p_;
    HpSessionStats st;
    st.basisSize = (int)P.basis.v.size();
    SolveResult R;

    if (!P.ps_raw) {
        R.singular = true; R.diagnostic = "HpSession::solveFrame: null PreparedSystem";
        if (stats) *stats = st;
        return R;
    }
    const PreparedSystem::Impl& S = *P.ps_raw;
    const int N = S.N;
    R.u.assign((size_t)std::max(0, N), 0.0);
    R.reactions.assign((size_t)std::max(0, N), 0.0);
    if (S.singular) {
        R.singular = true; R.diagnostic = S.diagnostic;
        if (stats) *stats = st;
        return R;
    }

    // Reuse-validity guard — identical to solveLoad / solveLoadHP (shared modelFingerprint).
    if (modelFingerprint(model) != S.fingerprint) {
        R.singular = true;
        R.diagnostic = "HpSession::solveFrame: model changed since assembleAndFactor (geometry/topology/"
                       "support flags/distributed loads). Re-run assembleAndFactor.";
        if (stats) *stats = st;
        return R;
    }

    // ---- RHS assembly: verbatim from solveLoad (sync if changed) --------------------
    VecX F = VecX::Zero(N);
    for (const auto& nl : model.nodalLoads) {
        const int ni = model.nodeIndex(nl.node);
        if (ni < 0) continue;
        for (int d = 0; d < 6; ++d) F(gdof(ni, d)) += nl.comp[d];
    }
    for (const auto& el : S.elems) el->addEquivalentNodalLoads(F);

    std::vector<real> presc((size_t)N, 0.0);
    for (size_t k = 0; k < model.nodes.size(); ++k)
        for (int d = 0; d < 6; ++d)
            if (model.nodes[k].fixed[d]) presc[(size_t)gdof((int)k, d)] = model.nodes[k].prescribed[d];

    VecX Ff = VecX::Zero(S.nf);
    for (int c = 0; c < N; ++c)
        for (SpMat::InnerIterator it(S.K, c); it; ++it) {
            const int r = it.row();
            if (S.fmap[r] < 0) continue;
            if (S.fmap[c] < 0 && presc[(size_t)c] != 0.0) Ff(S.fmap[r]) -= it.value() * presc[(size_t)c];
        }
    for (int g = 0; g < N; ++g) if (S.fmap[g] >= 0) Ff(S.fmap[g]) += F(g);

    // ---- the ONLY departure: seeded projection / warm PCG / full PCG / LDLT safety net ----
    const bool canHp = P.opts.enabled && P.baseValid && S.nf > 0 && !P.basis.v.empty();
    const real bnorm = Ff.norm();
    VecX uf;
    bool solved = false;
    std::string note;

    if (canHp && !(bnorm > real(0))) {
        // A zero RHS has the exact solution uf = 0 (same hardening as HpSolver.cpp); take it directly
        // so the relative-residual test never divides by a clamped tiny bnorm. Trivially in-subspace.
        uf = VecX::Zero(S.nf);
        solved = true;
        st.usedProjection = true;
        st.initialRel = 0;
        note = "[HpSession] zero RHS -> uf=0";
    } else if (canHp) {
        const VecX x0 = P.basis.initialGuess(S.nf, Ff);          // Galerkin projection onto the seeds
        VecX Kx0(S.nf); P.applyKff(x0, Kx0);
        const real initRel = (Ff - Kx0).norm() / bnorm;          // bnorm > 0 here -> well-posed
        st.initialRel = initRel;
        const bool inSub = (static_cast<double>(initRel) < P.opts.projGateTol);
        const int  maxIt = inSub ? P.opts.pcgWarmIter : P.opts.pcgMaxIter;

        // Warm-start PCG from x0 (a residual safety net atop the projection, never raw x0). Count a
        // step only AFTER x advances and test convergence right after, so pcgIters is honest.
        VecX x = x0;
        VecX r = Ff - Kx0;
        bool conv = static_cast<double>(r.norm() / bnorm) <= P.opts.pcgTol;
        int  iters = 0;
        if (!conv) {
            VecX z(S.nf), p(S.nf), Ap(S.nf);
            P.precondJacobi(r, z);
            p = z;
            real rz = r.dot(z);
            while (!conv && iters < maxIt) {
                P.applyKff(p, Ap);
                const real pAp = p.dot(Ap);
                if (!(pAp > real(0)) || !std::isfinite(static_cast<double>(pAp))) break;   // indefinite -> LDLT
                const real alpha = rz / pAp;
                x.noalias() += alpha * p;
                r.noalias() -= alpha * Ap;
                ++iters;
                if (static_cast<double>(r.norm() / bnorm) <= P.opts.pcgTol) { conv = true; break; }
                P.precondJacobi(r, z);
                const real rzNew = r.dot(z);
                if (!(rzNew > real(0)) || !std::isfinite(static_cast<double>(rzNew))) break;
                p = z + (rzNew / rz) * p;
                rz = rzNew;
            }
        }
        st.pcgIters = iters;
        if (conv && x.allFinite()) {
            uf = x;
            solved = true;
            if (inSub) { st.usedProjection = true; note = "[HpSession] seeded projection + warm PCG (" + std::to_string(iters) + " iter)"; }
            else       { st.usedPcg        = true; note = "[HpSession] full PCG (" + std::to_string(iters) + " iter)"; }
        }
        // not converged (or indefinite) -> fall through to the LDLT safety net below
    }

    if (!solved) {
        if (canHp && !P.opts.fallbackOnFail) {
            R.singular = true;
            R.diagnostic = "HpSession::solveFrame: HP path did not converge; fallback disabled";
            if (stats) *stats = st;
            return R;
        }
        uf = S.ldlt.solve(Ff);                      // the oracle / safety net
        if (S.ldlt.info() != Eigen::Success || !uf.allFinite()) {
            R.singular = true;
            R.diagnostic = "HpSession::solveFrame: LDLT fallback produced non-finite displacements (mechanism)";
            if (stats) *stats = st;
            return R;
        }
        st.usedLdlt = true;
        if (note.empty()) note = "[HpSession] LDLT (disabled / un-seeded / shell / non-converge)";
    }

    // ---- scatter / reactions / recover: verbatim from solveLoad (sync if changed) ---
    VecX u = VecX::Zero(N);
    for (int g = 0; g < N; ++g) u(g) = (S.fmap[g] >= 0) ? uf(S.fmap[g]) : presc[(size_t)g];
    for (int g = 0; g < N; ++g) R.u[(size_t)g] = u(g);

    const VecX Rv = S.K * u - F;
    for (int g = 0; g < N; ++g) R.reactions[(size_t)g] = Rv(g);

    R.memberForces.resize(model.members.size());
    R.shellForces.resize(model.shells.size());
    for (size_t e = 0; e < model.members.size(); ++e) R.memberForces[e].member = model.members[e].id;
    for (size_t s = 0; s < model.shells.size(); ++s)  R.shellForces[s].shell   = model.shells[s].id;
    for (const auto& el : S.elems) el->recover(u, R);
    R.pivotMargin = S.pivotMargin;
    R.diagnostic = note;   // which path ran; does not affect the numeric result
    if (stats) *stats = st;
    return R;
}

}  // namespace frame
