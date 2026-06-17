#pragma once
#include "FrameCore/FrameTypes.h"

namespace frame {

// POD options threaded into solve(). MUST stay engine-agnostic: only real / bool /
// int / plain enums -- never UE or Eigen types -- so FrameCore stays pure.
//
// REUSE SEMANTICS: these options are BAKED into the factorization by assembleAndFactor and are
// NOT part of modelFingerprint. A PreparedSystem built with one SolveOptions must not be reused
// as if it were built with another -- changing any option (useTimoshenko / useIncompatibleMembrane
// / useDKQPlate / enableReleases) requires a fresh assembleAndFactor.
struct SolveOptions {
    real pivotTol       = 1e-12;   // mechanism-detection pivot tolerance (relative to max|D|)
    bool enableReleases = false;   // honor Member.release[12] via per-element static condensation
    bool useTimoshenko  = false;   // include shear flexibility (needs Section.Asy/Asz > 0);
                                   // false keeps the Euler-Bernoulli element bit-for-bit.

    // S8-8a: opt-in QM6 incompatible-mode membrane (Wilson Q6 1973 + Taylor 1976 correction).
    // false keeps the original bilinear Q4 shell membrane BIT-FOR-BIT, so the OpenSees
    // ShellMITC4 plate gate (~1e-10) is preserved. true adds two bubble modes (1-xi^2,
    // 1-eta^2) on the in-plane u,v and condenses them out per element, defeating in-plane
    // membrane locking. The drilling (Rz / Hughes-Brezzi) block is untouched either way.
    // Constant-stress patch is EXACT on affine (parallelogram) meshes; on a general quad it is a
    // WEAK (Irons-Razzaque) patch -- converges under refinement, not bit-exact.
    bool useIncompatibleMembrane = false;

    // S8-8b: opt-in DKQ discrete-Kirchhoff THIN-plate bending (Batoz & Tahar 1982), replacing
    // the MITC4 assumed-shear bending block. false keeps the MITC4 Reissner-Mindlin plate
    // BIT-FOR-BIT (OpenSees ShellMITC4 gate). true = no transverse-shear DOF -> thin plate
    // only (t/L < ~1/20); mid/thick plates MUST keep this false (MITC4). Membrane + drilling
    // are shared -- this flag ONLY swaps the bending block. Recovered Qx=Qy=0 (Kirchhoff).
    bool useDKQPlate = false;

    // Opt-in SHELL geometric stiffness (stress stiffening) for MITC4 shells, so a model containing
    // shells gets a meaningful linear buckling factor (and shell P-Delta). false keeps shells a
    // no-op in assembleGeometric -> buckling/P-Delta stay BIT-FOR-BIT today's beam-column-only
    // behavior. Unlike the flags above this one does NOT change K_e / the factorization -- it only
    // feeds assembleGeometric from the prior linear solve's membrane field -- so it lives here purely
    // for shell opt-in consistency, and forcing a fresh assembleAndFactor on change is harmless.
    // Transverse-displacement (w) stress stiffening only: the standard thin-shell buckling term;
    // in-plane (u,v) second-order terms are intentionally excluded (documented limitation).
    bool shellGeometricStiffness = false;

    // Opt-in WARPING CORRECTION for warped (non-coplanar) MITC4 quads. false keeps today's behavior
    // BIT-FOR-BIT (corner coords projected onto the P0-origin facet plane, normal = diagonal cross
    // product). true projects the corners onto the BEST-FIT plane through the centroid (Newell average
    // normal), which reduces the projection error of a warped facet. MITC4 stays a flat facet -- this
    // only reduces the per-element projection error; the O(1/N^2) faceting of a curved surface is
    // unchanged, and a strongly warped quad still needs mesh refinement (documented limitation).
    bool useWarpingCorrection = false;
    // Relaxes validate()'s HARD rejection of non-coplanar shell quads. 1e-6 (default) = strict (today's
    // gate). Raise (e.g. 1e-2) to admit gently-warped free-surface meshes; pairs with useWarpingCorrection
    // to keep the projection error bounded. validate() rejects warp above warpTolerance * maxEdge.
    real warpTolerance = 1.0e-6;
};

} // namespace frame
