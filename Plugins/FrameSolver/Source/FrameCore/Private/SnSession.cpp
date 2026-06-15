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
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdint>

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
    if (opts.enabled && S.nf > 0) {
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
}

SnSession::~SnSession() = default;
SnSession::SnSession(SnSession&&) noexcept = default;
SnSession& SnSession::operator=(SnSession&&) noexcept = default;

bool SnSession::valid() const { return p_ && p_->snReady; }
const std::string& SnSession::diagnostic() const { return p_->diag; }

SolveResult SnSession::solveFrame(const FrameModel& model) {
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

    // ---- RHS assembly: verbatim from solveLoad --------------------------------------
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

    // ---- solve: reuse the PREBUILT supernodal factor (the ONLY departure from solveLoad) --
    VecX uf;
    bool used = false;
#if FRAMECORE_SUPERNODAL
    if (p_->snReady) {
        const int n = S.nf;
        std::vector<double> b((size_t)n), x((size_t)n);
        for (int i = 0; i < n; ++i) b[(size_t)i] = static_cast<double>(Ff(i));
        sn::solveSuper(p_->fac, p_->sym, b.data(), x.data());   // forward/back subst on the reused factor
        uf.resize(n);
        for (int i = 0; i < n; ++i) uf(i) = static_cast<real>(x[(size_t)i]);
        if (uf.allFinite()) used = true;
    }
#endif

    std::string note;
    if (used) {
        note = "[SnSession] supernodal solve (reused factor)";
    } else {
        if (p_->snReady && p_->opts.enabled && !p_->opts.fallbackOnFail) {
            R.singular = true;
            R.diagnostic = "SnSession::solveFrame: supernodal solve non-finite; fallback disabled";
            return R;
        }
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
    R.diagnostic = note;
    return R;
}

} // namespace frame
