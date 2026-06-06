#include "FrameCore/StaticCondensation.h"
#include "FrameEigen.h"
#include "IElement.h"
#include "BeamColumnElement.h"

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <memory>
#include <limits>

namespace frame {

// Assemble the full UNCONSTRAINED global K and load F through the same element path
// the solver uses (IElement -> BeamColumnElement), then Schur-eliminate the internal
// DOFs. See StaticCondensation.h for the contract and the math.
CondensationResult condenseStatic(const FrameModel& model,
                                  const std::vector<int>& boundaryGlobalDofs,
                                  const SolveOptions& opts) {
    CondensationResult r;
    r.boundaryCount = static_cast<int>(boundaryGlobalDofs.size());

    std::string why;
    if (!model.validate(why)) { r.ok = false; r.why = "invalid model: " + why; return r; }

    const int N = model.dofCount();
    if (N <= 0) { r.ok = false; r.why = "empty model (no DOF)"; return r; }

    // ---- validate the boundary DOF set: in range, no duplicates ----
    std::vector<char> isB(static_cast<size_t>(N), 0);
    for (const int g : boundaryGlobalDofs) {
        if (g < 0 || g >= N) { r.ok = false; r.why = "boundary DOF index out of range: " + std::to_string(g); return r; }
        if (isB[(size_t)g]) { r.ok = false; r.why = "duplicate boundary DOF index: " + std::to_string(g); return r; }
        isB[(size_t)g] = 1;
    }

    // ---- build elements + prepare (same seam as solve: respects releases/Timoshenko) ----
    std::vector<std::unique_ptr<IElement>> elems;
    elems.reserve(model.members.size());
    for (size_t e = 0; e < model.members.size(); ++e)
        elems.push_back(std::make_unique<BeamColumnElement>((int)e));
    for (auto& el : elems) {
        std::string ewhy;
        if (!el->prepare(model, opts, ewhy)) { r.ok = false; r.why = "element prepare failed: " + ewhy; return r; }
    }

    // ---- full unconstrained K (sparse) ----
    std::vector<Triplet> trips;
    trips.reserve(elems.size() * 144);
    for (auto& el : elems) el->assemble(trips);
    SpMat K(N, N);
    K.setFromTriplets(trips.begin(), trips.end());
    K.makeCompressed();

    // ---- full load F: nodal + element equivalent nodal ----
    VecX F = VecX::Zero(N);
    for (const auto& nl : model.nodalLoads) {
        const int ni = model.nodeIndex(nl.node);
        if (ni < 0) continue;
        for (int d = 0; d < 6; ++d) F(gdof(ni, d)) += nl.comp[d];
    }
    for (auto& el : elems) el->addEquivalentNodalLoads(F);

    // ---- partition into boundary (caller ordering preserved for S) / internal ----
    const std::vector<int>& bdofs = boundaryGlobalDofs;
    std::vector<int> idofs;
    idofs.reserve(static_cast<size_t>(N) - bdofs.size());
    for (int g = 0; g < N; ++g) if (!isB[(size_t)g]) idofs.push_back(g);
    const int nb = static_cast<int>(bdofs.size());
    const int ni = static_cast<int>(idofs.size());

    std::vector<int> bpos(static_cast<size_t>(N), -1), ipos(static_cast<size_t>(N), -1);
    for (int k = 0; k < nb; ++k) bpos[(size_t)bdofs[k]] = k;
    for (int k = 0; k < ni; ++k) ipos[(size_t)idofs[k]] = k;

    // ---- extract sub-blocks by scanning K's non-zeros once ----
    std::vector<Triplet> iiTrips;
    MatX Kib = MatX::Zero(ni, nb);   // internal x boundary
    MatX Kbi = MatX::Zero(nb, ni);   // boundary x internal
    MatX Kbb = MatX::Zero(nb, nb);
    for (int c = 0; c < N; ++c) {
        for (SpMat::InnerIterator it(K, c); it; ++it) {
            const int row = it.row();
            const real v  = it.value();
            const bool rB = isB[(size_t)row] != 0;
            const bool cB = isB[(size_t)c]   != 0;
            if (!rB && !cB)      iiTrips.emplace_back(ipos[(size_t)row], ipos[(size_t)c], v);
            else if (!rB && cB)  Kib(ipos[(size_t)row], bpos[(size_t)c]) += v;
            else if (rB && !cB)  Kbi(bpos[(size_t)row], ipos[(size_t)c]) += v;
            else                 Kbb(bpos[(size_t)row], bpos[(size_t)c]) += v;
        }
    }

    auto writeOut = [&](const MatX& Sdense, const VecX& fEff) {
        r.S.assign(static_cast<size_t>(nb) * nb, 0.0);
        for (int a = 0; a < nb; ++a)
            for (int b = 0; b < nb; ++b)
                r.S[static_cast<size_t>(a) * nb + b] = Sdense(a, b);
        r.fEff.assign(static_cast<size_t>(nb), 0.0);
        for (int a = 0; a < nb; ++a) r.fEff[(size_t)a] = fEff(a);
    };

    // ---- degenerate: no internal DOFs -> S = K_bb, fEff = f_b ----
    if (ni == 0) {
        VecX fb(nb);
        for (int k = 0; k < nb; ++k) fb(k) = F(bdofs[k]);
        writeOut(Kbb, fb);
        r.conditionNumber = 1.0;
        r.ok = true;
        r.why = "ok (no internal DOFs to condense)";
        return r;
    }

    // ---- factor K_ii + mechanism / conditioning guards ----
    SpMat Kii(ni, ni);
    Kii.setFromTriplets(iiTrips.begin(), iiTrips.end());
    Kii.makeCompressed();

    LDLTSolver ldlt;
    ldlt.compute(Kii);
    if (ldlt.info() != Eigen::Success) {
        r.ok = false;
        r.why = "internal block K_ii factorization failed (rank-deficient substructure / internal mechanism)";
        return r;
    }

    const VecX D = ldlt.vectorD();
    real maxAbs = 0, minAbs = std::numeric_limits<real>::max();
    for (int k = 0; k < D.size(); ++k) {
        const real a = std::abs(D(k));
        maxAbs = std::max(maxAbs, a);
        minAbs = std::min(minAbs, a);
    }
    const real kappaD = (minAbs > 0) ? (maxAbs / minAbs) : std::numeric_limits<real>::infinity();
    r.conditionNumber = kappaD;

    if (!(kappaD <= 1e10)) {   // catches inf/NaN too
        r.ok = false;
        r.why = "ill-conditioned internal block: kappa_D=" + std::to_string(kappaD) + " > 1e10 (hard reject)";
        return r;
    }

    // adaptive pivot tolerance (PFSF §3.2): looser when the block is already ill-conditioned.
    const real eps = opts.pivotTol * std::max(real(1), std::sqrt(kappaD / 1e4));
    const real thr = eps * std::max(real(1), maxAbs);
    for (int k = 0; k < D.size(); ++k) {
        if (std::abs(D(k)) < thr) {
            r.ok = false;
            r.why = "near-zero pivot in K_ii (internal mechanism) at internal dof #" + std::to_string(k);
            return r;
        }
        if (D(k) < 0) {
            r.ok = false;
            r.why = "negative pivot in K_ii (indefinite internal block) at internal dof #" + std::to_string(k);
            return r;
        }
    }

    // ---- Schur complement + condensed load (multi-RHS solves via the K_ii factor) ----
    const MatX X = ldlt.solve(Kib);                 // K_ii^-1 K_ib   (ni x nb)
    if (ldlt.info() != Eigen::Success) { r.ok = false; r.why = "K_ii solve (K_ib) failed"; return r; }
    const MatX Sdense = Kbb - Kbi * X;              // nb x nb

    VecX fb(nb), fi(ni);
    for (int k = 0; k < nb; ++k) fb(k) = F(bdofs[k]);
    for (int k = 0; k < ni; ++k) fi(k) = F(idofs[k]);
    const VecX y = ldlt.solve(fi);                  // K_ii^-1 f_i
    if (ldlt.info() != Eigen::Success) { r.ok = false; r.why = "K_ii solve (f_i) failed"; return r; }
    const VecX fEff = fb - Kbi * y;

    if (!Sdense.allFinite() || !fEff.allFinite()) {
        r.ok = false; r.why = "condensation produced non-finite entries"; return r;
    }

    writeOut(Sdense, fEff);
    r.ok = true;
    r.why = "ok";
    return r;
}

} // namespace frame
