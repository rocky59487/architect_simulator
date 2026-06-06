#include "FrameCore/EquivalentModeling.h"
#include <algorithm>
#include <cmath>

namespace frame { namespace equiv {

real effectiveFlangeWidthACI(real bw, real hf, real swClear, real lnClear) {
    const real overhang = std::min({ real(8) * hf, real(0.5) * swClear, lnClear / real(8) });
    return bw + real(2) * std::max(real(0), overhang);
}

Section compositeTeeSection(real bw, real h, real be, real hf) {
    Section s;
    const real Af = be * hf;
    const real hw = h - hf;                 // web depth below the flange
    const real Aw = bw * hw;
    const real A  = Af + Aw;

    // centroid measured from the TOP fibre
    const real yf = hf * real(0.5);
    const real yw = hf + hw * real(0.5);
    const real ybar = (Af * yf + Aw * yw) / A;

    // strong-axis I about the composite centroid (parallel-axis theorem)
    const real If = be * hf * hf * hf / real(12) + Af * (ybar - yf) * (ybar - yf);
    const real Iw = bw * hw * hw * hw / real(12) + Aw * (yw - ybar) * (yw - ybar);

    s.A  = A;
    s.Iz = If + Iw;
    s.cz = std::max(ybar, h - ybar);        // extreme fibre (top vs bottom)
    // weak-axis I about the vertical centroidal axis (flange & web symmetric in width)
    s.Iy = be * be * be * hf / real(12) + hw * bw * bw * bw / real(12);
    s.cy = real(0.5) * std::max(be, bw);

    // approximate torsion constant: sum of the rectangular St-Venant constants
    auto rectJ = [](real a_, real t_) -> real {
        const real a = std::max(a_, t_), t = std::min(a_, t_), r = t / a;
        return a * t * t * t * (real(1) / real(3) - real(0.21) * r * (real(1) - (r * r * r * r) / real(12)));
    };
    s.J = rectJ(be, hf) + rectJ(bw, hw);

    // shear is carried mostly by the web (vertical) / flange (horizontal)
    s.Asy = (real(5) / real(6)) * Aw;
    s.Asz = (real(5) / real(6)) * Af;
    s.shape = Section::Shape::Rectangular;
    return s;
}

real equivalentBraceArea(real K, real E, real L, real H) {
    const real Ld = std::sqrt(L * L + H * H);
    return K * Ld * Ld * Ld / (E * L * L);
}

}} // namespace frame::equiv
