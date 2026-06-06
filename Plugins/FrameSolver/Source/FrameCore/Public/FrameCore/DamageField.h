#pragma once
#include "FrameCore/FrameTypes.h"
#include <vector>

namespace frame { namespace damage {

// #11 history-field irreversible damage — seam ② (a new analysis state layered on the
// static solver). A scalar continuum-damage model with an irreversible history variable:
//
//   H_{n+1} = max(H_n, psi_{n+1})          (Kuhn-Tucker loading/unloading: H never drops)
//
// where psi is a driving demand measure (e.g. the section D/C ratio). The damage scalar
// d(H) follows linear softening — derived from a bilinear law whose stress drops linearly
// from sigma0 = E*kappa0 at H=kappa0 to 0 at H=kappaf:
//
//   d(H) = 0                                     H <= kappa0   (undamaged)
//   d(H) = 1 - kappa0*(kappaf - H) /
//              ( H * (kappaf - kappa0) )         kappa0 < H < kappaf
//   d(H) = 1                                     H >= kappaf   (fully damaged)
//
// d is monotone non-decreasing in H, so as H only grows the damage only grows: unloading
// (a smaller psi) leaves H — and therefore d and the secant stiffness (1-d) — unchanged.
// No healing, ever. (spec: monotonic + unloading-no-heal.)
struct DamageLaw {
    real kappa0 = 1.0;   // damage onset threshold (D/C = 1 by default)
    real kappaf = 5.0;   // history at which d -> 1 (full degradation). Must be > kappa0.
};

// Closed-form damage d(H) in [0,1] for the given law (no state).
FRAMECORE_API real damageOf(real H, const DamageLaw& law);

// Per-element irreversible state. Hold one per member/element across pseudo-time steps.
struct DamageState {
    real H = 0;   // history = running max of the driving measure psi

    // Advance with a new demand psi (H = max(H, psi)) and return the updated damage d.
    FRAMECORE_API real update(real psi, const DamageLaw& law);

    // Current damage d(H) and secant factor (1-d) WITHOUT advancing the state.
    real damage(const DamageLaw& law) const { return damageOf(H, law); }
    real secant(const DamageLaw& law) const { return 1.0 - damageOf(H, law); }
};

}} // namespace frame::damage
