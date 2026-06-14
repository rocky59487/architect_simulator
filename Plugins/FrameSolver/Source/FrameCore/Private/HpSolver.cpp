//
// HP-FEM opt-in iterative solve lane (A1: serial matrix-free PCG + Jacobi precond,
// frame-only, LDLT fallback). See Public/FrameCore/HpSolver.h.
//
// Design contract: solveLoadHP must return a SolveResult bit-equivalent to solveLoad()
// except for the solve step itself. The RHS assembly, prescribed reduction, scatter,
// reactions (K*u - F) and element recovery below are therefore copied verbatim from
// FrameSolver.cpp::solveLoad — the ONLY departure is replacing `S.ldlt.solve(Ff)` with a
// matrix-free preconditioned conjugate gradient. The LDLT factor remains the oracle and
// the fallback. Eigen is confined to this Private .cpp; the public header is POD.
//
#include "FrameCore/HpSolver.h"
#include "PreparedSystemImpl.h"   // PreparedSystem::Impl (K/fmap/nf/ldlt/elems/...) + FrameEigen.h
#include "IElement.h"             // el->assemble / addEquivalentNodalLoads / recover / localDof
#include "ModelHash.h"            // modelFingerprint (the shared reuse-validity guard)

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace frame {

namespace {

// Per-element reduced (free-DOF) operator entries: the element's global stiffness triplets
// (T^T kl T scattered to its global DOFs) with both indices mapped through fmap and the
// constrained rows/cols dropped. Summing these scalar contributions over all elements
// equals K_ff * x by construction — it uses the exact triplets reduceFF() builds K_ff from,
// so the matrix-free apply matches the LDLT operator to the last bit.
struct ElemReduced {
    std::vector<int>  ri, ci;
    std::vector<real> v;
};

}  // namespace

SolveResult solveLoadHP(const PreparedSystem& prepared, const FrameModel& model,
                        const HpSolveOptions& opts) {
    const PreparedSystem::Impl& S = *prepared.impl;
    SolveResult R;
    const int N = S.N;
    R.u.assign((size_t)std::max(0, N), 0.0);
    R.reactions.assign((size_t)std::max(0, N), 0.0);
    if (S.singular) { R.singular = true; R.diagnostic = S.diagnostic; return R; }

    // Reuse-validity guard — identical to solveLoad (shared modelFingerprint).
    if (modelFingerprint(model) != S.fingerprint) {
        R.singular = true;
        R.diagnostic = "solveLoadHP: model changed since assembleAndFactor (geometry/topology/"
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

    // ---- the ONLY departure from solveLoad: matrix-free PCG vs ldlt.solve(Ff) -------
    // Build the per-element reduced operator + Jacobi diagonal. A1 is frame-only: any
    // element that is not a 12-DOF beam (e.g. a shell) routes the whole solve to LDLT.
    bool hasShell = false;
    std::vector<ElemReduced> blocks;
    blocks.reserve(S.elems.size());
    VecX diag = VecX::Zero(S.nf);
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
            if (ri == ci) diag(ri) += t.value();
        }
        blocks.push_back(std::move(b));
    }

    auto applyKff = [&](const VecX& x, VecX& y) {
        y.setZero();
        for (const auto& b : blocks)
            for (size_t k = 0; k < b.ri.size(); ++k) y(b.ri[k]) += b.v[k] * x(b.ci[k]);
    };
    auto precond = [&](const VecX& rr, VecX& zz) {
        for (int i = 0; i < S.nf; ++i) zz(i) = (diag(i) != real(0)) ? rr(i) / diag(i) : rr(i);
    };

    VecX uf;
    bool pcgConverged = false;
    int pcgIters = 0;
    const bool tryPcg = opts.enabled && !hasShell && S.nf > 0;
    if (tryPcg) {
        VecX x = VecX::Zero(S.nf);
        VecX r = Ff;                                // r = Ff - K_ff * x0, with x0 = 0
        const real bnorm = std::max<real>(real(1e-300), Ff.norm());
        // x0 = 0 already solves a zero RHS (e.g. a load-free settlement that does not excite
        // the free DOFs); otherwise iterate, counting a step only after x actually advances and
        // testing convergence right after, so the reported iteration count is honest.
        pcgConverged = static_cast<double>(r.norm() / bnorm) <= opts.pcgTol;
        VecX z(S.nf), p(S.nf), Ap(S.nf);
        precond(r, z);
        p = z;
        real rz = r.dot(z);
        while (!pcgConverged && pcgIters < opts.pcgMaxIter) {
            applyKff(p, Ap);
            const real pAp = p.dot(Ap);
            if (!(pAp > real(0)) || !std::isfinite(static_cast<double>(pAp))) break;
            const real alpha = rz / pAp;
            x.noalias() += alpha * p;
            r.noalias() -= alpha * Ap;
            ++pcgIters;                             // a real CG step has advanced x
            if (static_cast<double>(r.norm() / bnorm) <= opts.pcgTol) { pcgConverged = true; break; }
            precond(r, z);
            const real rzNew = r.dot(z);
            if (!(rzNew > real(0)) || !std::isfinite(static_cast<double>(rzNew))) break;
            p = z + (rzNew / rz) * p;
            rz = rzNew;
        }
        if (pcgConverged && x.allFinite()) uf = x;
        else pcgConverged = false;
    }

    std::string note;
    if (pcgConverged) {
        note = "[HpSolver] matrix-free PCG converged in " + std::to_string(pcgIters) + " iter";
    } else {
        const std::string why = !opts.enabled ? "HP lane disabled"
                              : hasShell       ? "shell element present (A1 is frame-only)"
                                               : "PCG did not converge to tol";
        if (opts.enabled && !opts.fallbackOnFail) {
            R.singular = true;
            R.diagnostic = "solveLoadHP: " + why + "; fallback disabled";
            return R;
        }
        uf = S.ldlt.solve(Ff);
        note = "[HpSolver] LDLT (" + why + ")";
    }

    if (S.ldlt.info() != Eigen::Success || !uf.allFinite()) {
        R.singular = true;
        R.diagnostic = "solveLoadHP: solve produced non-finite displacements (mechanism)";
        return R;
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
    R.diagnostic = note;   // which path ran; does not affect the numeric result
    return R;
}

}  // namespace frame
