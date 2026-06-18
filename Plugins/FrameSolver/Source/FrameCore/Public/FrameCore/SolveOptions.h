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

    // R2.1 audit AC-07 architectural fix (RBD-NEW-1 caveat): opt-in coarse-curved-mesh guard. assembleAndFactor
    // computes the angle between adjacent MITC4 facets' normals (edge-shared shell pairs) and
    // refuses to build the system if any pair exceeds this threshold. 0 (default) disables the
    // guard (v2.0 bit-identical). Set to e.g. 22.5 deg = 16 facets per 90 deg of curvature to
    // enforce the rule of thumb that keeps hoop-membrane error below ~2 % on a cylinder
    // (audit AUDIT_FINDINGS.md: N=8 facets/circle gave 7.6 % hoop error, N=16 gave 1.9 %,
    // N=32 gave 0.48 % under O(1/N^2)). Designers who knowingly accept coarser meshes set
    // a larger angle or leave the flag off; the guard is opt-in protection, not a default
    // policy. Free-form / Gaudi-style meshes can vary curvature locally — the guard reports
    // the worst pair so refinement can target the trouble spot.
    //
    // CAVEAT (audit RBD-NEW-1): the implementation uses |cos(angle)| so the angle is always
    // mapped into [0, 90 deg] regardless of which side of each facet the normal points to.
    // This is geometrically correct for convex / smooth surfaces (the use case the guard
    // was designed for) but it under-estimates dihedral angles on CONCAVE or reflex geometry
    // (a 270 deg fold reads as 90 deg). For concave free-form meshes, lower the threshold
    // to compensate, or pre-orient all facet normals consistently before calling.
    //
    // CAVEAT (audit FINAL-AC07-1): the guard pairs only ACTIVE shells. If a fine mesh has
    // every other shell deactivated (e.g. a partial progressive-collapse trace), the
    // remaining active shells may have no active neighbour and the max-angle stays 0 deg,
    // silently admitting a geometrically coarse model. This is the intended behaviour for
    // progressive-collapse use (where deactivated facets are gone, not just hidden), but
    // a user who deactivates fine-mesh shells expecting the remaining coarse ones to be
    // checked will see no rejection. Pre-filter such cases yourself or run the guard on
    // the full pre-deactivation model.
    real shellCurvatureMaxAngleDeg = 0;

    // R2.1 audit PERF-01 architectural fix: when TRUE, assembleAndFactor builds the self-built
    // supernodal Cholesky (METIS+OpenBLAS) as the PRIMARY factor and SKIPS the SimplicialLDLT
    // factor entirely when the SPD check succeeds. solveLoad then pays only the supernodal
    // factor + back-substitution (measured 8-20x faster than LDLT at 18k-62k DOF on the
    // `perf_sn` driver). When the supernodal SPD check fails (mechanism / indefinite),
    // assembleAndFactor falls back to the LDLT path automatically so mechanism detection
    // remains authoritative.
    //
    // CONSTRAINT: this is a fast-path flag for the bare `solveLoad` workflow. Analyses that
    // internally rely on the LDLT factor (PDeltaAnalysis refactor path, Reanalysis ReSolve
    // Tier-2/3, ModalAnalysis dense subspace, BucklingAnalysis sparse subspace,
    // DynamicCollapse static IC + Ritz basis) will refuse to run on a PreparedSystem built
    // with this flag and return a clear diagnostic. Use the default (LDLT primary) for those
    // workflows. Default `false` keeps every existing 5-leg test bit-identical.
    //
    // FRAMECORE_SUPERNODAL=0 builds (conda env absent): the flag is silently ignored —
    // assembleAndFactor stays on the LDLT path.
    bool useSupernodalPrimary = false;
};

} // namespace frame
