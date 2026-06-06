#pragma once
#include "FrameCore/FrameTypes.h"

namespace frame {

// Cross-section properties in LOCAL axes.
//   Iz : 2nd moment about local z  (bending in the x-y plane, deflection along local y)
//   Iy : 2nd moment about local y  (bending in the x-z plane, deflection along local z)
//   cy/cz : extreme-fibre distances -> section moduli Wy = Iy/cy, Wz = Iz/cz.
struct Section {
    // Section shape governs the biaxial-bending combination used by ElasticAllowable:
    //   Rectangular -> conservative corner sum |My|/Wy + |Mz|/Wz (worst corner).
    //   Circular    -> resultant sqrt(My^2 + Mz^2)/W (a round section has no worst
    //                  corner, so the corner sum would be needlessly conservative).
    enum class Shape { Rectangular, Circular };

    real A  = 0;
    real Iy = 0;
    real Iz = 0;
    real J  = 0;
    real cy = 0;
    real cz = 0;

    // Timoshenko effective shear areas (Asy resists local-y shear & pairs with the Iz
    // bending block; Asz resists local-z shear & pairs with Iy). 0 means "not provided"
    // -> the solver uses the Euler-Bernoulli element (no shear flexibility) regardless
    // of SolveOptions.useTimoshenko. Set by the section factories.
    real Asy = 0;
    real Asz = 0;

    Shape shape = Shape::Rectangular;

    real Wy() const { return cy > 0 ? Iy / cy : 0; }   // = d*b^2/6 for a rectangle
    real Wz() const { return cz > 0 ? Iz / cz : 0; }   // = b*d^2/6 for a rectangle (textbook W)

    // b = width (along local z), d = depth (along local y).
    static FRAMECORE_API Section Rectangular(real b, real d);
    // Solid circular section of radius r (symmetric: Iy == Iz, Wy == Wz).
    static FRAMECORE_API Section Circular(real r);
};

} // namespace frame
