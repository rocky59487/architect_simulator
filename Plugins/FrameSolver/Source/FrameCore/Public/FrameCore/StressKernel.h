#pragma once
#include "FrameCore/FrameTypes.h"
#include "FrameCore/Section.h"
#include <cmath>
#include <algorithm>

// Single source of truth for fiber / surface stress formulas. Both the elastic D/C
// screen (ElasticAllowable) and the visualization stress-field post-process
// (StressField) call into this header so the two cannot drift. All formulas mirror
// the LOCAL-coordinate conventions used by MemberEndForces (N compression-positive,
// Vy/Vz/T/My/Mz signed) and ShellElementForces (per-width Nxx/Nyy/Nxy + Mxx/Myy/Mxy).
//
// Conventions (DO NOT change without also re-running F1..F66 + F70):
//   * Member axial stress is COMPRESSION-POSITIVE everywhere (matches N's sign rule).
//     sComp = compressive demand magnitude (>= 0), sTens = tensile demand magnitude (>= 0).
//   * Shell layer Top = +bending side (face along +local-z normal,
//     where bend-induced sigma adds with a +1 sign on Mxx/Myy/Mxy).
//     Shell layer Bot = -bending side.
//   * principalStress returns (s1, s2, vonMises, theta_rad), s1 >= s2.

namespace frame {

enum class ShellLayer : int { Top = 0, Bot = 1 };

// Member fiber id along the 4 face mid-lines of the cross-section. Visualisation
// expectation:
//   TopY    (local +y face mid, z=0)
//   BotY    (local -y face mid, z=0)
//   PlusZ   (local +z face mid, y=0)
//   MinusZ  (local -z face mid, y=0)
// For a rectangular section the WORST corner fiber is reconstructed by
// memberCornerSigmaMax (sum-of-abs), which is what the elastic D/C screen reads.
enum class MemberFiber : int { TopY = 0, BotY = 1, PlusZ = 2, MinusZ = 3 };

// ---- Member fiber sigma (compression-positive) -----------------------------
// Face mid-line fiber sigma from axial + bending (Euler-Bernoulli).
//   sigma(fiber) = N/A + Mz * y_fiber / Iz + My * z_fiber / Iy   (compression-positive)
// where (y_fiber, z_fiber) ranges over the 4 face mid-points (+/-cy, 0) and
// (0, +/-cz). Mz contributes only at TopY/BotY; My contributes only at PlusZ/MinusZ.
// Useful for visualising the contiguous fiber stress field along a member
// (matches how a colour-band paint over the 4 long faces would be sampled).
[[nodiscard]] inline real memberFiberSigma(real N, real A,
                                           real My, real Mz,
                                           real Iy, real Iz,
                                           real cy, real cz,
                                           MemberFiber fiber)
{
    const real safeA  = (A  > 0) ? A  : real(1e-12);
    const real safeIy = (Iy > 0) ? Iy : real(1e-12);
    const real safeIz = (Iz > 0) ? Iz : real(1e-12);
    const real sN = N / safeA;
    switch (fiber) {
        case MemberFiber::TopY:   return sN + Mz * ( cy) / safeIz;
        case MemberFiber::BotY:   return sN + Mz * (-cy) / safeIz;
        case MemberFiber::PlusZ:  return sN + My * ( cz) / safeIy;
        case MemberFiber::MinusZ: return sN + My * (-cz) / safeIy;
    }
    return sN;
}

// ---- Member corner-worst sigma magnitudes ----------------------------------
// Returns the compressive and tensile demand magnitudes (both >= 0) at the
// worst section corner. This is exactly what ElasticAllowable::checkSection
// reports as sComp / sTens. Rectangular sum-of-abs vs circular resultant is
// the section-shape switch documented on Section::Shape.
struct CornerSigmaPair {
    real sComp;   // compressive demand magnitude  (>= 0)
    real sTens;   // tensile  demand magnitude    (>= 0)
    real sBend;   // bending-only worst fiber magnitude (>= 0)  [sM]
    real sAxial;  // N / A                         (signed; compression positive) [sN]
};

[[nodiscard]] inline CornerSigmaPair memberCornerSigmaMax(real N, real A,
                                                          real My, real Mz,
                                                          real Wy, real Wz,
                                                          Section::Shape shape)
{
    const real safeA  = (A  > 0) ? A  : real(1e-12);
    const real safeWy = (Wy > 0) ? Wy : real(1e-12);
    const real safeWz = (Wz > 0) ? Wz : real(1e-12);

    const real sN = N / safeA;
    const real sM = (shape == Section::Shape::Circular)
                    ? std::sqrt(My * My + Mz * Mz) / safeWz
                    : std::abs(My) / safeWy + std::abs(Mz) / safeWz;

    CornerSigmaPair p;
    p.sComp  = std::max(sM + sN, real(0));
    p.sTens  = std::max(sM - sN, real(0));
    p.sBend  = sM;
    p.sAxial = sN;
    return p;
}

// ---- Member shear / torsion (peak shear screen) ----------------------------
// Peak transverse shear over the cross-section: V/A is the AVERAGE; the peak
// at the neutral axis is k*V/A with k = 1.5 for a rectangle and 4/3 for a
// circle. Returns absolute magnitude (>= 0).
[[nodiscard]] inline real memberShearPeak(real Vy, real Vz, real A, Section::Shape shape)
{
    const real safeA = (A > 0) ? A : real(1e-12);
    const real k = (shape == Section::Shape::Circular) ? real(4.0 / 3.0) : real(1.5);
    return k * std::sqrt(Vy * Vy + Vz * Vz) / safeA;
}

// Torsional shear stress magnitude T*c/J. c is the extreme-fibre distance:
// for a circle c = cy = r; for a rectangle the diagonal corner hypot(cy,cz)
// is used as a conservative St-Venant heuristic (warping deferred).
[[nodiscard]] inline real memberTorsionTau(real T, real J, real cy, real cz, Section::Shape shape)
{
    const real safeJ = (J > 0) ? J : real(1e-12);
    const real cTor  = (shape == Section::Shape::Circular) ? cy : std::hypot(cy, cz);
    return std::abs(T) * cTor / safeJ;
}

// ---- Shell layer stress -----------------------------------------------------
// Project per-width membrane + bending resultants onto a top / bottom fiber.
// Membrane:   sigma_membrane_x = Nxx / t   (etc.)
// Bending:    sigma_bend_x      = +/-6*Mxx / t^2   (top: +1, bot: -1)
// All inputs are per-unit-width (as stored in ShellElementForces).
inline void shellLayerSigma(real Nxx, real Nyy, real Nxy,
                            real Mxx, real Myy, real Mxy,
                            real t,
                            ShellLayer layer,
                            real& sigxx, real& sigyy, real& tauxy)
{
    const real safeT = (t > 0) ? t : real(1e-12);
    const real mx  = Nxx / safeT;
    const real my  = Nyy / safeT;
    const real mxy = Nxy / safeT;
    const real bend = real(6.0) / (safeT * safeT);
    const real s = (layer == ShellLayer::Top) ? real(1) : real(-1);
    sigxx = mx  + s * bend * Mxx;
    sigyy = my  + s * bend * Myy;
    tauxy = mxy + s * bend * Mxy;
}

// ---- Principal stress + von Mises ------------------------------------------
// In-plane principal stresses with s1 >= s2; theta is the angle (rad) of the
// s1 axis from local +x. vonMises = sqrt(max(0, sx^2 - sx*sy + sy^2 + 3*txy^2)).
// The max(0, .) clamp mirrors ElasticAllowable::checkShellSurface (guards
// against tiny negative round-off under the sqrt).
struct PrincipalStress {
    real s1;
    real s2;
    real vonMises;
    real theta;   // radians, in (-pi/2, +pi/2] (atan2 half-angle of the principal direction)
};

[[nodiscard]] inline PrincipalStress principalStress(real sx, real sy, real txy)
{
    const real avg = real(0.5) * (sx + sy);
    const real dev = real(0.5) * (sx - sy);
    const real r   = std::sqrt(dev * dev + txy * txy);
    PrincipalStress p;
    p.s1 = avg + r;
    p.s2 = avg - r;
    p.vonMises = std::sqrt(std::max(real(0),
                                    sx * sx - sx * sy + sy * sy + real(3) * txy * txy));
    p.theta = real(0.5) * std::atan2(real(2) * txy, sx - sy);
    return p;
}

} // namespace frame
