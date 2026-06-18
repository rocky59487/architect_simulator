//
// Opt-in self-built supernodal Cholesky solve lane. See Public/FrameCore/SnSolver.h.
//
// Design contract: solveLoadSupernodal must return a SolveResult
// bit-equivalent to solveLoad() except for the solve step itself. The RHS assembly, prescribed
// reduction, scatter, reactions (K*u - F) and element recovery are therefore copied verbatim from
// FrameSolver.cpp::solveLoad -- the ONLY departure is replacing `S.ldlt.solve(Ff)` with the
// self-built supernodal factor (METIS ordering + BLAS3 panels). The LDLT factor remains the oracle
// and the fallback. Eigen is confined to this Private .cpp; the public header is POD.
//
// FRAMECORE_SUPERNODAL (from FrameSnChol.h) gates the supernodal body: when 0 (conda OpenBLAS/METIS
// env absent) the lane compiles but routes straight to LDLT, so the function still links and tests
// still exercise the drop-in contract.
//
#include "FrameCore/SnSolver.h"
#include "PreparedSystemImpl.h"   // PreparedSystem::Impl (K/fmap/nf/ldlt/elems/...) + reduceFF + FrameEigen.h
#include "IElement.h"             // el->assemble / addEquivalentNodalLoads / recover
#include "ModelHash.h"            // modelFingerprint (the shared reuse-validity guard)
#include "FrameSnChol.h"          // FRAMECORE_SUPERNODAL + sn:: (guarded; pulls metis/cblas when on)

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace frame {

SolveResult solveLoadSupernodal(const PreparedSystem& prepared, const FrameModel& model,
                                const SnSolveOptions& opts) {
    const PreparedSystem::Impl& S = *prepared.impl;
    SolveResult R;
    const int N = S.N;
    R.u.assign((size_t)std::max(0, N), 0.0);
    R.reactions.assign((size_t)std::max(0, N), 0.0);
    if (S.singular) { R.singular = true; R.diagnostic = S.diagnostic; return R; }

    // Reuse-validity guard -- identical to solveLoad (shared modelFingerprint).
    if (modelFingerprint(model) != S.fingerprint) {
        R.singular = true;
        R.diagnostic = "solveLoadSupernodal: model changed since assembleAndFactor (geometry/topology/"
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

    // ---- the ONLY departure from solveLoad: self-built supernodal vs ldlt.solve(Ff) -
    VecX uf;
    bool snOk = false;
    int  irApplied = 0;
    double irFinalRes = 0.0;
#if FRAMECORE_SUPERNODAL
    if (opts.enabled && S.nf > 0) {
        try {
            const int n = S.nf;
            SpMat Kff = reduceFF(S.K, S.fmap, n);     // full-symmetric nf x nf CSC (col-major) -- sn input
            Kff.makeCompressed();
            sn::SnSymbolic sym = sn::analyze(n, Kff.outerIndexPtr(), Kff.innerIndexPtr(), opts.useMetis);
            sn::SnSuper fac = sn::factorizeSuperParallel(
                n, Kff.outerIndexPtr(), Kff.innerIndexPtr(), Kff.valuePtr(), sym,
                opts.amalgRelax, opts.amalgMaxCol, opts.numThreads, opts.blasThreadsRoot);
            if (fac.spd) {
                std::vector<double> b((size_t)n), x((size_t)n);
                for (int i = 0; i < n; ++i) b[(size_t)i] = static_cast<double>(Ff(i));
                sn::solveSuper(fac, sym, b.data(), x.data());

                // R2: Neumaier-compensated iterative refinement on top of the supernodal factor.
                // Loop reuses the SAME (sym, fac) so no re-factor cost; each step is one compensated
                // SpMV + one forward/back substitution. opts.irSteps==0 -> no extra work (bit-identical
                // to no-IR). If irTol>0, early-stop when ||r||_inf <= irTol * ||b||_inf.
                if (opts.irSteps > 0) {
                    const double bNorm = sn::infNorm(n, b.data());
                    const double absTol = (opts.irTol > 0.0) ? (opts.irTol * bNorm) : 0.0;
                    std::vector<double> r((size_t)n), d((size_t)n);
                    for (int k = 0; k < opts.irSteps; ++k) {
                        sn::neumaierResidualFullSym(n, Kff.outerIndexPtr(), Kff.innerIndexPtr(),
                                                    Kff.valuePtr(), b.data(), x.data(), r.data());
                        irFinalRes = sn::infNorm(n, r.data());
                        if (absTol > 0.0 && irFinalRes <= absTol) break;
                        sn::solveSuper(fac, sym, r.data(), d.data());
                        for (int i = 0; i < n; ++i) x[(size_t)i] += d[(size_t)i];
                        ++irApplied;
                    }
                }

                uf.resize(n);
                for (int i = 0; i < n; ++i) uf(i) = static_cast<real>(x[(size_t)i]);
                if (uf.allFinite()) snOk = true;
            }
        } catch (...) { snOk = false; }   // any supernodal failure -> LDLT fallback below
    }
#endif

    std::string note;
    if (snOk) {
        note = "[SnSolver] self-built supernodal Cholesky";
        if (irApplied > 0) {
            note += " + IR" + std::to_string(irApplied) + "/" + std::to_string(opts.irSteps);
            if (opts.irTol > 0.0) note += " (resInf=" + std::to_string(irFinalRes) + ")";
        }
    } else {
        const char* why =
            !opts.enabled ? "supernodal lane disabled"
#if FRAMECORE_SUPERNODAL
                          : (S.nf <= 0 ? "no free DOF" : "supernodal factor not SPD / non-finite");
#else
                          : "supernodal not compiled (FRAMECORE_SUPERNODAL=0; conda OpenBLAS/METIS env absent)";
#endif
        if (opts.enabled && !opts.fallbackOnFail) {
            R.singular = true;
            R.diagnostic = std::string("solveLoadSupernodal: ") + why + "; fallback disabled";
            return R;
        }
#if FRAMECORE_SUPERNODAL
        // R2.1 PERF-01: supernodal-primary PreparedSystem has no LDLT factor.
        if (S.useSnPrimary) {
            R.singular = true;
            R.diagnostic = std::string("solveLoadSupernodal: ") + why +
                           "; LDLT fallback unavailable (useSupernodalPrimary=true)";
            return R;
        }
#endif
        uf = S.ldlt.solve(Ff);                        // the oracle / safety net
        if (S.ldlt.info() != Eigen::Success || !uf.allFinite()) {
            R.singular = true;
            R.diagnostic = "solveLoadSupernodal: LDLT fallback produced non-finite displacements (mechanism)";
            return R;
        }
        note = std::string("[SnSolver] LDLT (") + why + ")";
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
