#include "FrameCore/FiberSection.h"
#include "FrameCore/Section.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace frame {

namespace {

// ACI equivalent-stress-block factor.
real beta1(real fc) {
    if (fc <= 28.0) return 0.85;
    return std::max(real(0.65), real(0.85 - 0.05 * (fc - 28.0) / 7.0));
}

// (Pn, Mn) of a rectangular RC section bending about an axis of depth dd (width bb),
// with steel AsC at depth `cover` (compression side) and AsT at dd-cover (tension side),
// at neutral-axis depth c from the extreme compression fibre. Compression positive.
// Whitney concrete block (0.85 f'c over depth a = beta1*c) + elastic-perfectly-plastic
// steel (with displaced-concrete correction for compression bars inside the block).
void section_PM(const RCSectionParams& P, real bb, real dd, real AsC, real AsT, real c,
                real& Pn, real& Mn) {
    const real a  = std::min(beta1(P.fc) * c, dd);
    const real Cc = 0.85 * P.fc * bb * a;             // concrete block resultant, at a/2

    auto steel = [&](real As, real y, real& F, real& M) {
        F = 0; M = 0;
        if (As <= 0 || c <= 0) return;
        const real eps = P.ecu * (c - y) / c;          // compression positive
        real sig = std::clamp(P.Es * eps, -P.fy, P.fy);
        if (eps > 0 && y < a) sig -= 0.85 * P.fc;       // subtract displaced concrete
        F = sig * As;
        M = F * (dd / 2.0 - y);                         // about the centroid
    };
    real Ft, Mt, Fb, Mb;
    steel(AsC, P.cover,      Ft, Mt);
    steel(AsT, dd - P.cover, Fb, Mb);

    Pn = Cc + Ft + Fb;
    Mn = Cc * (dd / 2.0 - a / 2.0) + Mt + Mb;
}

// Moment capacity Mn at axial Pdemand for bending about an axis of depth dd: sweep the
// neutral-axis depth to trace the interaction curve, interpolate Mn at Pdemand. Fills
// P0 (pure compression) and Pt (pure tension) from closed form.
real momentCapacityAtP(const RCSectionParams& P, real bb, real dd, real AsC, real AsT,
                       real Pdemand, real& P0, real& Pt) {
    const real Ast = AsC + AsT;
    P0 = 0.85 * P.fc * (bb * dd - Ast) + P.fy * Ast;
    Pt = -P.fy * Ast;

    const int  n    = std::max(20, P.nSweep);
    const real cMin = dd * 0.01, cMax = dd * 6.0;
    real prevP = 0, prevM = 0; bool have = false;
    for (int i = 0; i <= n; ++i) {
        const real c = cMin + (cMax - cMin) * (real(i) / real(n));
        real Pn, Mn; section_PM(P, bb, dd, AsC, AsT, c, Pn, Mn);
        if (have && (prevP - Pdemand) * (Pn - Pdemand) <= 0 && Pn != prevP) {
            const real t = (Pdemand - prevP) / (Pn - prevP);
            return std::abs(prevM + t * (Mn - prevM));
        }
        prevP = Pn; prevM = Mn; have = true;
    }
    return 0.0;   // Pdemand outside the swept range -> axial governs (handled by caller)
}

} // namespace

DemandResult FiberSection::checkSection(const MemberEndForces& f, const Section& s, const Capacity& /*c*/) const {
    DemandResult d;
    const real bb = 2.0 * s.cy;   // width  (local z extent)
    const real dd = 2.0 * s.cz;   // depth  (local y extent) = strong-axis bending (Mz)
    if (bb <= 0 || dd <= 0) { return d; }

    const real Pd  = f.N;         // compression-positive (matches RC convention)
    const real big = std::numeric_limits<real>::infinity();

    real P0z, Ptz, P0y, Pty;
    const real Mnz = momentCapacityAtP(P, bb, dd, P.AsTop, P.AsBot, Pd, P0z, Ptz);  // about z (depth dd)
    const real Mny = momentCapacityAtP(P, dd, bb, P.AsTop, P.AsBot, Pd, P0y, Pty);  // about y (depth bb)
    const real P0 = P0z, Pt = Ptz;

    real risk = 0; FailMode mode = FailMode::None;
    if (Pd >= P0) {
        risk = (P0 > 0) ? Pd / P0 : big;  mode = FailMode::Crush;
    } else if (Pd <= Pt) {
        risk = (Pt < 0) ? Pd / Pt : big;  mode = FailMode::Tension;
    } else {
        // conservative linear biaxial moment contour: Mz/Mnz + My/Mny <= 1
        const real rz = (Mnz > 0) ? std::abs(f.Mz) / Mnz : big;
        const real ry = (Mny > 0) ? std::abs(f.My) / Mny : big;
        risk = rz + ry;  mode = FailMode::Bending;
    }

    // one-way concrete shear screen (ACI Vc = 0.17 sqrt(f'c) b d_eff), folded in
    const real dEff = std::max(dd - P.cover, real(1));
    const real Vc   = 0.17 * std::sqrt(P.fc) * bb * dEff;
    const real Vu   = std::sqrt(f.Vy * f.Vy + f.Vz * f.Vz);
    const real rv   = (Vc > 0) ? Vu / Vc : big;
    if (rv > risk) { risk = rv; mode = FailMode::Shear; }

    d.risk  = risk;
    d.mode  = (risk > 0) ? mode : FailMode::None;
    d.sComp = (P0 > 0) ? Pd / P0 : 0;
    d.tau   = Vu / (bb * dEff);
    return d;
}

} // namespace frame
