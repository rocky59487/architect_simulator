#include "FrameCore/Section.h"
#include <algorithm>
#include <cmath>

namespace frame {

// b = width (along local z), d = depth (along local y).
Section Section::Rectangular(real b, real d) {
    Section s;
    // R2 audit MI-02 guard: a non-positive side silently produces zero/negative section
    // properties (zero EI, zero Wz/Wy) and the engine would otherwise let `validate()`
    // catch it later as "non-positive section property" — but the path producing that
    // section is the factory itself. Reject up front so the caller sees the bad input
    // at construction time, not at solve time. A zero-section `Section` is returned so
    // the assignment `m.sections.push_back(Section::Rectangular(0, ...))` still runs (no
    // throw) but `validate()` then blocks the model with "non-positive section property"
    // -- consistent with the existing post-construction guard, just without the silent
    // negative/zero values flowing through the rest of the pipeline.
    if (!(b > 0) || !(d > 0) || !std::isfinite(b) || !std::isfinite(d)) {
        return s;   // s.A == 0 etc.; validate() will block the solve.
    }
    s.A  = b * d;
    s.Iz = b * d * d * d / 12.0;   // about local z (depth d in local y) -> governs Mz
    s.Iy = d * b * b * b / 12.0;   // about local y (width b in local z) -> governs My
    s.cz = d / 2.0;                // -> Wz = Iz/cz = b*d^2/6  (textbook section modulus)
    s.cy = b / 2.0;                // -> Wy = Iy/cy = d*b^2/6
    // St-Venant torsion constant for a solid rectangle (a = long side, t = short side).
    const real a = std::max(b, d);
    const real t = std::min(b, d);
    const real r = t / a;
    s.J = a * t * t * t * (1.0 / 3.0 - 0.21 * r * (1.0 - (r * r * r * r) / 12.0));
    // Timoshenko shear coefficient for a rectangle: k = 5/6 -> A_s = 5/6 * A.
    s.Asy = s.Asz = (5.0 / 6.0) * s.A;
    // Plastic section moduli (full-section plastic moment Mp = fy * Z).
    s.Zz = b * d * d / 4.0;
    s.Zy = d * b * b / 4.0;
    s.shape = Shape::Rectangular;
    return s;
}

// Solid circular section of radius r.
Section Section::Circular(real r) {
    Section s;
    // R2 audit MI-02 guard (see Rectangular): non-positive r yields zero properties; let
    // validate() catch it explicitly rather than silently produce a degenerate section.
    if (!(r > 0) || !std::isfinite(r)) {
        return s;
    }
    const real r2 = r * r;
    s.A  = kPi * r2;
    s.Iy = s.Iz = kPi * r2 * r2 / 4.0;   // I = pi r^4 / 4 (about any centroidal axis)
    s.J  = kPi * r2 * r2 / 2.0;          // J = pi r^4 / 2 (polar = 2I)
    s.cy = s.cz = r;                    // extreme fibre at radius r -> Wy = Wz = pi r^3 / 4
    // Timoshenko shear coefficient for a solid circle ~ 0.9 -> A_s = 0.9 * A.
    s.Asy = s.Asz = 0.9 * s.A;
    // Plastic section modulus of a solid circle: Z = 4 r^3 / 3 (both axes).
    s.Zy = s.Zz = 4.0 * r * r2 / 3.0;
    s.shape = Shape::Circular;
    return s;
}

} // namespace frame
