#include "FrameCore/FrameSolver.h"
#include "PreparedSystemImpl.h"
#include "BeamColumnElement.h"
#include "MITC4ShellElement.h"
#include "ElementFactory.h"
#include "ModelHash.h"

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <limits>
#include <memory>
#include <cstring>
#include <cstdint>

namespace frame {

// fpMix / fpBits / modelFingerprint moved to ModelHash.h so the opt-in supernodal lane
// (SnSolver.cpp / SnSession.cpp) shares the exact same reuse-validity hash (logic byte-identical).

PreparedSystem::PreparedSystem() : impl(std::make_unique<Impl>()) {}
PreparedSystem::~PreparedSystem() = default;
PreparedSystem::PreparedSystem(PreparedSystem&&) noexcept = default;
PreparedSystem& PreparedSystem::operator=(PreparedSystem&&) noexcept = default;

// ============================================================================
// Phase 1: assemble + factorize. Everything that depends on geometry / topology /
// support flags / distributed loads is done ONCE here. The result is reused by any
// number of solveLoad() calls that only vary nodal loads + prescribed values.
// ============================================================================
PreparedSystem assembleAndFactor(const FrameModel& model, const SolveOptions& opts) {
    PreparedSystem ps;
    PreparedSystem::Impl& S = *ps.impl;

    std::string why;
    if (!model.validate(why, opts.warpTolerance)) {
        S.singular = true;
        S.diagnostic = "invalid model: " + why;
        S.N = std::max(0, model.dofCount());
        return ps;
    }
    S.N = model.dofCount();
    const int N = S.N;

    // build the element list (beams + shells), prepare geometry + baked distributed loads.
    S.elems.reserve(model.members.size() + model.shells.size());
    for (size_t e = 0; e < model.members.size(); ++e)
        if (model.members[e].active)   // inactive members are excluded from assembly (element removal)
            S.elems.push_back(makeMemberElem((int)e));
    for (size_t s = 0; s < model.shells.size(); ++s)
        if (model.shells[s].active)   // inactive shells are excluded from assembly (element removal)
            S.elems.push_back(makeShellElem((int)s));

    for (auto& el : S.elems) {
        std::string ewhy;
        if (!el->prepare(model, opts, ewhy)) {
            S.singular = true;
            S.diagnostic = ewhy;
        }
    }
    if (S.singular) return ps;

    // assemble global K
    std::vector<Triplet> trips;
    size_t cap = 0;
    for (const auto& el : S.elems) { const size_t d = (size_t)el->localDof(); cap += d * d; }
    trips.reserve(cap);
    for (const auto& el : S.elems) el->assemble(trips);
    SpMat K(N, N);
    K.setFromTriplets(trips.begin(), trips.end());
    K.makeCompressed();
    S.K = std::move(K);

    // free-DOF map from support FLAGS (prescribed VALUES are applied later, in solveLoad)
    S.fmap.assign(N, -1);
    S.nf = 0;
    for (size_t k = 0; k < model.nodes.size(); ++k)
        for (int d = 0; d < 6; ++d) {
            const int g = gdof((int)k, d);
            if (!model.nodes[k].fixed[d]) S.fmap[g] = S.nf++;
        }
    if (S.nf == 0) { S.singular = true; S.diagnostic = "fully constrained (no free DOF)"; return ps; }

    // R2.1 AC-07 architectural fix: opt-in coarse-curved-shell-mesh guard. Worst-case
    // adjacent-facet normal angle across the shell set. Refuses the build if it exceeds
    // SolveOptions::shellCurvatureMaxAngleDeg so the user gets a hard signal that the mesh
    // won't deliver curved-surface accuracy. 0 (default) skips the check entirely.
    if (opts.shellCurvatureMaxAngleDeg > 0 && model.shells.size() >= 2) {
        std::unordered_map<uint64_t, int> edgeToShell;
        edgeToShell.reserve(model.shells.size() * 4);
        auto edgeKey = [](int a, int b) -> uint64_t {
            if (a > b) std::swap(a, b);
            return (static_cast<uint64_t>(static_cast<uint32_t>(a)) << 32)
                 |  static_cast<uint64_t>(static_cast<uint32_t>(b));
        };
        auto facetNormal = [&](size_t s) -> Vec3 {
            const auto& sh = model.shells[s];
            int idx[4];
            for (int k = 0; k < 4; ++k) idx[k] = model.nodeIndex(sh.n[k]);
            if (idx[0] < 0 || idx[1] < 0 || idx[2] < 0 || idx[3] < 0) return {0, 0, 0};
            const Vec3 nrm = cross(model.nodes[idx[2]].pos - model.nodes[idx[0]].pos,
                                   model.nodes[idx[3]].pos - model.nodes[idx[1]].pos);
            const real m = norm(nrm);
            return (m > 0) ? (nrm * (real(1) / m)) : Vec3{0, 0, 0};
        };
        real maxAngleDeg = 0;
        int  worstA = -1, worstB = -1;
        for (size_t s = 0; s < model.shells.size(); ++s) {
            const auto& sh = model.shells[s];
            // R2.1 audit BLDG SLV-NEW-1 (confirmed): skip inactive shells so a progressive-collapse
            // model with shell removal doesn't fire a false geometric rejection on the (now-gone)
            // facet's pre-removal neighbour relationship.
            if (!sh.active) continue;
            const Vec3 nA = facetNormal(s);
            if (nA.x == 0 && nA.y == 0 && nA.z == 0) continue;
            for (int e = 0; e < 4; ++e) {
                const int a = model.nodeIndex(sh.n[e]);
                const int b = model.nodeIndex(sh.n[(e + 1) % 4]);
                if (a < 0 || b < 0) continue;
                const uint64_t k = edgeKey(a, b);
                auto it = edgeToShell.find(k);
                if (it == edgeToShell.end()) {
                    edgeToShell.emplace(k, (int)s);
                } else {
                    const Vec3 nB = facetNormal((size_t)it->second);
                    if (nB.x == 0 && nB.y == 0 && nB.z == 0) continue;
                    real c = nA.x * nB.x + nA.y * nB.y + nA.z * nB.z;
                    // MITC4 normals can point either side of the facet; take |cos| so the
                    // angle is the geometric one (always in [0, 90 deg]).
                    if (c < 0) c = -c;
                    if (c > real(1)) c = real(1);
                    const real ang = std::acos(c) * real(180) / kPi;
                    if (ang > maxAngleDeg) {
                        maxAngleDeg = ang;
                        worstA = (int)s; worstB = it->second;
                    }
                }
            }
        }
        if (maxAngleDeg > opts.shellCurvatureMaxAngleDeg) {
            S.singular = true;
            S.diagnostic = "curved-shell mesh too coarse: max adjacent-facet angle = "
                           + std::to_string((double)maxAngleDeg) + " deg > tol "
                           + std::to_string((double)opts.shellCurvatureMaxAngleDeg)
                           + " deg (shells [" + std::to_string(worstA) + "," + std::to_string(worstB)
                           + "]). Refine the curved-shell mesh (suggested 22.5 deg ~ 16 facets per 90 deg "
                             "curvature for ~2% hoop accuracy), or set SolveOptions::shellCurvatureMaxAngleDeg=0 "
                             "to disable.";
            return ps;
        }
    }

    // reduced K_ff (free-free block; independent of prescribed values)
    SpMat Kff = reduceFF(S.K, S.fmap, S.nf);

    // ---- R2.1 PERF-01: opt-in supernodal-primary fast path ----------------------------------
    // When useSupernodalPrimary is requested and the supernodal lane is compiled in, build the
    // self-built supernodal Cholesky FIRST and skip the LDLT factor on SPD success. This is the
    // actual architectural fix for "supernodal cannot beat LDLT in public API": now it CAN,
    // because LDLT no longer runs unconditionally. Mechanism detection comes from the supernodal
    // SPD check (Cholesky fails on indefinite / rank-deficient K) and a pivot screen on the
    // L diagonal (sqrt of LDL^T pivots, since K = L L^T). On SPD failure we fall through to the
    // LDLT path so existing mechanism-diagnosis fidelity is preserved.
#if FRAMECORE_SUPERNODAL
    if (opts.useSupernodalPrimary) {
        Kff.makeCompressed();
        const int n = S.nf;
        bool snOk = false;
        try {
            S.snSym = sn::analyze(n, Kff.outerIndexPtr(), Kff.innerIndexPtr(), /*useMetis=*/true);
            S.snFac = sn::factorizeSuperParallel(
                n, Kff.outerIndexPtr(), Kff.innerIndexPtr(), Kff.valuePtr(),
                S.snSym, /*amalgRelax=*/0, /*amalgMaxCol=*/64,
                /*numThreads=*/0, /*blasThreadsRoot=*/0);
            snOk = S.snFac.spd;
        } catch (...) {
            snOk = false;
        }
        if (snOk) {
            // Derive pivotMargin from the supernodal L diagonal (column-major panels).
            // K = L L^T, so L_ii^2 plays the role of the LDLT D pivot. We screen the same
            // way (relative tolerance) and report min/max as the criticality margin.
            real maxAbs = 0, minAbs = std::numeric_limits<real>::infinity();
            int  smallestIdx = -1;
            real smallestVal = std::numeric_limits<real>::infinity();
            for (int J = 0; J < S.snFac.nsn; ++J) {
                const int  nc = S.snFac.ncol[J];
                const int  nr = S.snFac.nrow[J];
                const int  c0 = S.snFac.c0 [J];
                const auto& d = S.snFac.data[J];
                for (int k = 0; k < nc; ++k) {
                    const real Lkk = static_cast<real>(d[(size_t)k * (size_t)nr + (size_t)k]);
                    const real piv = Lkk * Lkk;                 // pivot-equivalent (LL^T form)
                    if (piv > maxAbs) maxAbs = piv;
                    if (piv < smallestVal) { smallestVal = piv; smallestIdx = c0 + k; }
                    if (piv < minAbs) minAbs = piv;
                }
            }
            const real tol = opts.pivotTol * std::max(real(1), maxAbs);
            if (minAbs < tol) {
                S.singular = true;
                // R2.1 audit SOLVER SLV-NEW-1: smallestIdx is the index in the METIS-permuted
                // ordering; un-permute back to the physical reduced-DOF index so the diagnostic
                // names the same DOF the LDLT path would have flagged.
                const int physicalIdx = (smallestIdx >= 0 && smallestIdx < (int)S.snSym.perm.size())
                                        ? S.snSym.perm[smallestIdx] : smallestIdx;
                S.diagnostic = "near-zero supernodal Cholesky pivot (mechanism) at reduced dof #"
                               + std::to_string(physicalIdx);
                return ps;
            }
            S.pivotMargin   = (maxAbs > 0) ? (minAbs / maxAbs) : real(0);
            S.useSnPrimary  = true;
            S.fingerprint   = modelFingerprint(model);
            return ps;   // LDLT factor INTENTIONALLY SKIPPED -- the PERF-01 architectural win
        }
        // SPD failed -> drop the partial supernodal state and fall through to LDLT for
        // authoritative mechanism diagnostics.
        S.snFac = sn::SnSuper{};
        S.snSym = sn::SnSymbolic{};
    }
#endif // FRAMECORE_SUPERNODAL

    // factor + mechanism detection (from the factorization, never from connectivity)
    S.ldlt.compute(Kff);
    if (S.ldlt.info() != Eigen::Success) {
        S.singular = true;
        S.diagnostic = "LDLT factorization failed (info != Success): rank-deficient stiffness / mechanism";
        return ps;
    }
    const VecX D = S.ldlt.vectorD();
    real maxAbs = 0, minAbs = 0; bool firstPivot = true;
    for (int i = 0; i < D.size(); ++i) {
        const real a = std::abs(D(i));
        maxAbs = std::max(maxAbs, a);
        if (firstPivot || a < minAbs) { minAbs = a; firstPivot = false; }
    }
    const real tol = opts.pivotTol * std::max(real(1), maxAbs);
    for (int i = 0; i < D.size(); ++i) {
        if (std::abs(D(i)) < tol) {
            S.singular = true;
            S.diagnostic = "near-zero pivot in LDLT D (mechanism) at reduced dof #" + std::to_string(i);
            return ps;
        }
        if (D(i) < 0) {
            S.singular = true;
            S.diagnostic = "negative pivot in LDLT D (indefinite stiffness / mechanism) at reduced dof #" + std::to_string(i);
            return ps;
        }
    }
    S.pivotMargin = (maxAbs > 0) ? minAbs / maxAbs : 0;   // C4: proximity-to-singular (min/max pivot)
    S.fingerprint = modelFingerprint(model);   // baseline for the solveLoad reuse-validity guard
    return ps;
}

real PreparedSystem::pivotMargin() const { return impl ? impl->pivotMargin : real(0); }
bool PreparedSystem::isSingular()  const { return !impl || impl->singular; }
const char* PreparedSystem::diagnostic() const {
    static const char emptyDiag[] = "";
    return impl ? impl->diagnostic.c_str() : emptyDiag;
}
bool PreparedSystem::usingSupernodalPrimary() const {
#if FRAMECORE_SUPERNODAL
    return impl && impl->useSnPrimary;
#else
    return false;
#endif
}

// ============================================================================
// Phase 2: cheap re-solve. Reuses the factorization; only re-builds the RHS from the
// current model's nodal loads + prescribed values + baked distributed loads.
// ============================================================================
SolveResult solveLoad(const PreparedSystem& prepared, const FrameModel& model) {
    const PreparedSystem::Impl& S = *prepared.impl;
    SolveResult R;
    const int N = S.N;
    R.u.assign((size_t)std::max(0, N), 0.0);
    R.reactions.assign((size_t)std::max(0, N), 0.0);
    if (S.singular) { R.singular = true; R.diagnostic = S.diagnostic; return R; }

    // Reuse-validity guard: the factorization + baked distributed loads are only valid while the
    // structural model is unchanged. Reject a model whose fingerprint differs (geometry / topology /
    // support flags / member UDLs / shell pressures changed since assembleAndFactor) instead of
    // silently returning a stale-load solve. Nodal loads and prescribed VALUES may still vary
    // freely — they are excluded from the fingerprint (the interactive / settlement path).
    if (modelFingerprint(model) != S.fingerprint) {
        R.singular = true;
        R.diagnostic = "solveLoad: model changed since assembleAndFactor (geometry/topology/"
                       "support flags/distributed loads). Re-run assembleAndFactor.";
        return R;
    }

    // global load vector: current nodal loads + baked distributed equivalent loads
    VecX F = VecX::Zero(N);
    for (const auto& nl : model.nodalLoads) {
        const int ni = model.nodeIndex(nl.node);
        if (ni < 0) continue;
        for (int d = 0; d < 6; ++d) F(gdof(ni, d)) += nl.comp[d];
    }
    for (const auto& el : S.elems) el->addEquivalentNodalLoads(F);

    // current prescribed (support displacement) values — may differ between solveLoad calls
    std::vector<real> presc((size_t)N, 0.0);
    for (size_t k = 0; k < model.nodes.size(); ++k)
        for (int d = 0; d < 6; ++d)
            if (model.nodes[k].fixed[d]) presc[(size_t)gdof((int)k, d)] = model.nodes[k].prescribed[d];

    // reduced RHS  F_f - K_fc u_c
    VecX Ff = VecX::Zero(S.nf);
    for (int c = 0; c < N; ++c)
        for (SpMat::InnerIterator it(S.K, c); it; ++it) {
            const int r = it.row();
            if (S.fmap[r] < 0) continue;
            if (S.fmap[c] < 0 && presc[(size_t)c] != 0.0) Ff(S.fmap[r]) -= it.value() * presc[(size_t)c];
        }
    for (int g = 0; g < N; ++g) if (S.fmap[g] >= 0) Ff(S.fmap[g]) += F(g);

    // solve (reuse factorization). R2.1 PERF-01: when assembleAndFactor built the supernodal
    // factor as the primary (useSupernodalPrimary on SPD success), route through sn::solveSuper
    // instead of the LDLT (which was never computed). Same numeric result to factorization
    // round-off; bit-identical drop-in when the flag is off (default).
    VecX uf(S.nf);
#if FRAMECORE_SUPERNODAL
    if (S.useSnPrimary) {
        const int n = S.nf;
        std::vector<double> b((size_t)n), x((size_t)n);
        for (int i = 0; i < n; ++i) b[(size_t)i] = static_cast<double>(Ff(i));
        sn::solveSuper(S.snFac, S.snSym, b.data(), x.data());
        for (int i = 0; i < n; ++i) uf(i) = static_cast<real>(x[(size_t)i]);
        if (!uf.allFinite()) {
            R.singular = true;
            R.diagnostic = "solve produced non-finite displacements (supernodal solve; mechanism)";
            return R;
        }
    } else {
        uf = S.ldlt.solve(Ff);
        if (S.ldlt.info() != Eigen::Success || !uf.allFinite()) {
            R.singular = true;
            R.diagnostic = "solve produced non-finite displacements (mechanism)";
            return R;
        }
    }
#else
    uf = S.ldlt.solve(Ff);
    if (S.ldlt.info() != Eigen::Success || !uf.allFinite()) {
        R.singular = true;
        R.diagnostic = "solve produced non-finite displacements (mechanism)";
        return R;
    }
#endif

    VecX u = VecX::Zero(N);
    for (int g = 0; g < N; ++g) u(g) = (S.fmap[g] >= 0) ? uf(S.fmap[g]) : presc[(size_t)g];
    for (int g = 0; g < N; ++g) R.u[(size_t)g] = u(g);

    const VecX Rv = S.K * u - F;
    for (int g = 0; g < N; ++g) R.reactions[(size_t)g] = Rv(g);

    R.memberForces.resize(model.members.size());
    R.shellForces.resize(model.shells.size());
    // Stamp every row's id, including INACTIVE elements (their forces stay zero but recover()
    // never visits them): leaving the default id 0 in a vector that is parallel to the model
    // would make any consumer keying results by id silently read the wrong element.
    for (size_t e = 0; e < model.members.size(); ++e) R.memberForces[e].member = model.members[e].id;
    for (size_t s = 0; s < model.shells.size(); ++s)  R.shellForces[s].shell   = model.shells[s].id;
    for (const auto& el : S.elems) el->recover(u, R);
    R.pivotMargin = S.pivotMargin;   // C4: report the factorization's criticality margin
    return R;
}

// One-shot linear static solve (back-compatible thin wrapper).
SolveResult solve(const FrameModel& model, const SolveOptions& opts) {
    PreparedSystem ps = assembleAndFactor(model, opts);
    return solveLoad(ps, model);
}

} // namespace frame
