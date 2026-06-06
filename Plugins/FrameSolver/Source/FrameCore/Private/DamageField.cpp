#include "FrameCore/DamageField.h"
#include <algorithm>

namespace frame { namespace damage {

real damageOf(real H, const DamageLaw& law) {
    const real k0 = law.kappa0, kf = law.kappaf;
    if (kf <= k0)  return (H > k0) ? 1.0 : 0.0;   // degenerate law -> step at the threshold
    if (H <= k0)   return 0.0;                     // undamaged below onset
    if (H >= kf)   return 1.0;                     // fully damaged at/after final history
    // linear-softening damage: stress drops linearly from E*k0 (at k0) to 0 (at kf).
    const real d = 1.0 - k0 * (kf - H) / (H * (kf - k0));
    return std::min<real>(1.0, std::max<real>(0.0, d));
}

real DamageState::update(real psi, const DamageLaw& law) {
    if (psi > H) H = psi;        // irreversible: H only ever grows (Kuhn-Tucker)
    return damageOf(H, law);
}

}} // namespace frame::damage
