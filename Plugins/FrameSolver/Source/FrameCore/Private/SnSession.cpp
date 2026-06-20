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
        return;
    }
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
}

SnSession::~SnSession() = default;
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
        sn::solveSuper(fac, sym, b.data(), x.data());   // forward/back subst on the reused factor

        // R2: Neumaier-compensated IR. Cache populated only when opts.irSteps>0 at ctor, so the
        // empty check below also gates "session was built without IR, ignore the runtime opt".
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
    VecX u = VecX::Zero(N);
    for (int g = 0; g < N; ++g) u(g) = (S.fmap[g] >= 0) ? uf(S.fmap[g]) : presc[(size_t)g];
    for (int g = 0; g < N; ++g) R.u[(size_t)g] = u(g);

    const VecX Rv = S.K * u - F;
    for (int g = 0; g < N; ++g) R.reactions[(size_t)g] = Rv(g);

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
    R.pivotMargin = S.pivotMargin;
    R.diagnostic = note;
    return R;
}

} // namespace frame
