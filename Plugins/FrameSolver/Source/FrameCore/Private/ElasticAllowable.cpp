#include "FrameCore/ElasticAllowable.h"
#include "FrameCore/StressKernel.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace frame {

// Elastic / allowable-stress combined-stress screen.  NOT RC ultimate strength.
// Formulas live in StressKernel.h (single source of truth shared with StressField).
DemandResult ElasticAllowable::checkSection(const MemberEndForces& f, const Section& s, const Capacity& c) const {
    const CornerSigmaPair sig = memberCornerSigmaMax(f.N, s.A, f.My, f.Mz,
                                                     s.Wy(), s.Wz(), s.shape);
    const real sM    = sig.sBend;
    const real sComp = sig.sComp;
    const real sTens = sig.sTens;
    const real tau   = memberShearPeak(f.Vy, f.Vz, s.A, s.shape);
    const real sTor  = memberTorsionTau(f.T, s.J, s.cy, s.cz, s.shape);

    auto ratio = [](real demand, real cap) -> real {
        if (cap > 0) return demand / cap;
        return demand > 0 ? std::numeric_limits<real>::infinity() : real(0);
    };
    const real r[5] = { ratio(sComp, c.comp), ratio(sTens, c.tens),
                        ratio(tau, c.shear),  ratio(sM, c.bend), ratio(sTor, c.tors) };
    int k = 0;
    for (int i = 1; i < 5; ++i) if (r[i] > r[k]) k = i;
    static const FailMode M[5] = { FailMode::Crush, FailMode::Tension,
                                   FailMode::Shear, FailMode::Bending, FailMode::Torsion };

    DemandResult d;
    d.risk  = r[k];
    d.mode  = r[k] > 0 ? M[k] : FailMode::None;
    d.sComp = sComp; d.sTens = sTens; d.tau = tau; d.sTor = sTor;
    return d;
}

// Stage 3d: shell surface von Mises screen. Membrane stress from the centre resultants
// (element-constant approximation), bending stress from the centre AND per-corner moments,
// both faces; the worst sample governs. Mirrors checkSection's ratio() semantics, including
// "zero capacity under demand = infinite D/C". Sigma formula lives in StressKernel.h.
ShellDemandResult checkShellSurface(const ShellElementForces& f, real t, const Capacity& c) {
    ShellDemandResult out;
    if (!(t > 0)) return out;   // validate() rejects t <= 0; defensive zero here

    auto ratio = [](real demand, real cap) -> real {
        if (cap > 0) return demand / cap;
        return demand > 0 ? std::numeric_limits<real>::infinity() : real(0);
    };

    for (int kc = -1; kc < 4; ++kc) {             // -1 = centre, 0..3 = corners
        const real Mx  = (kc < 0) ? f.Mxx : f.MxxC[kc];
        const real My  = (kc < 0) ? f.Myy : f.MyyC[kc];
        const real Mxy = (kc < 0) ? f.Mxy : f.MxyC[kc];
        for (int face = 0; face < 2; ++face) {
            const ShellLayer layer = (face == 0) ? ShellLayer::Top : ShellLayer::Bot;
            real sx = 0, sy = 0, txy = 0;
            shellLayerSigma(f.Nxx, f.Nyy, f.Nxy, Mx, My, Mxy, t, layer, sx, sy, txy);
            const PrincipalStress ps = principalStress(sx, sy, txy);
            const real r = ratio(ps.vonMises, c.vm);
            if (r > out.risk) { out.risk = r; out.corner = kc; out.top = (face == 0); }
        }
    }
    return out;
}

// Shell counterpart of worstUtilization (same skip rules: inactive / out-of-range matIdx).
ShellDemandSummary worstShellUtilization(const FrameModel& model, const SolveResult& r) {
    ShellDemandSummary out;
    const size_t nS = std::min(model.shells.size(), r.shellForces.size());
    bool any = false;
    real maxDC = 0;
    for (size_t s = 0; s < nS; ++s) {
        const ShellQuad& sh = model.shells[s];
        if (!sh.active) continue;
        if (sh.matIdx < 0 || sh.matIdx >= (int)model.materials.size()) continue;
        const ShellDemandResult d = checkShellSurface(r.shellForces[s], sh.t, model.materials[(size_t)sh.matIdx].cap);
        if (!any || d.risk > maxDC) {
            maxDC = d.risk;
            out.governingShell = sh.id;
        }
        any = true;
    }
    out.valid = any;
    out.maxDC = any ? maxDC : real(0);
    return out;
}

// C3: worst Demand/Capacity over all ACTIVE members (both ends) + the elastic safety factor.
// Inactive members (element removal) are skipped; members with an out-of-range material/section
// index are skipped (validate() should have caught those, but be defensive). Pure post-process.
DemandSummary worstUtilization(const FrameModel& model, const SolveResult& r) {
    DemandSummary out;
    const ElasticAllowable screen;
    const size_t nM = std::min(model.members.size(), r.memberForces.size());
    bool any = false;
    real maxDC = 0;
    for (size_t e = 0; e < nM; ++e) {
        const Member& mem = model.members[e];
        if (!mem.active) continue;
        if (mem.matIdx < 0 || mem.matIdx >= (int)model.materials.size()) continue;
        if (mem.secIdx < 0 || mem.secIdx >= (int)model.sections.size())  continue;
        const Section&  s = model.sections[mem.secIdx];
        const Capacity& c = model.materials[mem.matIdx].cap;
        const MemberForcePair& mf = r.memberForces[e];
        const DemandResult di = screen.checkSection(mf.endI, s, c);
        const DemandResult dj = screen.checkSection(mf.endJ, s, c);
        const DemandResult& d = (di.risk >= dj.risk) ? di : dj;   // worse of the two ends
        if (!any || d.risk > maxDC) {
            maxDC = d.risk;
            out.governingMember = mem.id;
            out.mode = d.mode;
        }
        any = true;
    }
    out.valid = any;
    out.maxDC = any ? maxDC : real(0);
    out.safetyFactor = any ? ((maxDC > 0) ? real(1) / maxDC : std::numeric_limits<real>::infinity())
                           : real(0);
    return out;
}

} // namespace frame
