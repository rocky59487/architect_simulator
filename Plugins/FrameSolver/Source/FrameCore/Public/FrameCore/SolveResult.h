#pragma once
#include "FrameCore/FrameTypes.h"
#include "FrameCore/ISectionStrength.h"
#include <vector>
#include <string>

namespace frame {

struct MemberForcePair {
    MemberId        member = 0;
    MemberEndForces endI;   // local end forces at node i
    MemberEndForces endJ;   // local end forces at node j
};

// Stress resultants for a shell facet, in the facet's LOCAL frame. The scalar fields are at
// the element CENTRE (the average for a linearly-varying field). Bending/twist moments and
// transverse shears are per-unit-width (N*mm/mm = N); membrane forces are per-unit-width
// (N/mm). Per-CORNER bending moments are also provided for design PEAK recovery (see below).
struct ShellElementForces {
    int  shell = 0;                       // ShellQuad::id
    real Mxx = 0, Myy = 0, Mxy = 0;       // bending + twisting moments / width (centre)
    real Qx  = 0, Qy  = 0;                // transverse shears / width (centre)
    real Nxx = 0, Nyy = 0, Nxy = 0;       // membrane forces / width (centre)
    // Per-CORNER bending moments (corner order matches ShellQuad::n). These are LINEAR
    // quantities, so they combine()/envelope() correctly. The design PEAK moment is a
    // post-process: the max over corners of |Mxx|/|Myy|/|Mxy| — do NOT read the centre value
    // as a peak. For a constant-moment field every corner equals the centre.
    // NOTE: with the opt-in DKQ plate (SolveOptions::useDKQPlate) these hold Gauss-point values,
    // not corner extrapolations — for a DKQ design peak use the centre value, not MxxC[k].
    real MxxC[4] = { 0, 0, 0, 0 };
    real MyyC[4] = { 0, 0, 0, 0 };
    real MxyC[4] = { 0, 0, 0, 0 };
};

struct SolveResult {
    bool                            singular = false;   // mechanism / instability detected
    std::string                     diagnostic;
    std::vector<real>               u;                  // 6N global DOF displacements
    std::vector<real>               reactions;          // 6N global (nonzero at constrained DOF)
    std::vector<MemberForcePair>    memberForces;
    std::vector<ShellElementForces> shellForces;        // parallel to model.shells

    // Criticality / proximity-to-singular indicator (C4): min|LDLT pivot| / max|LDLT pivot| of the
    // reduced stiffness K_ff, in (0,1]. The solver flags a mechanism when this falls below pivotTol,
    // so a value COLLAPSING toward pivotTol is the early warning. NOT a normalized 0..1 health score
    // and NOT the condition number: well-conditioned frames are already <<1 (axial pivots dominate
    // bending pivots). Read it RELATIVELY (vs the same structure) or as distance above pivotTol.
    // It is a dimensionless ratio (scale-invariant). 0 when singular / not factored.
    real pivotMargin = 0;

    // R2 audit LC-06 guard: previously these unconditionally indexed into u/reactions and
    // would silently invoke UB when the SolveResult is singular (empty u/reactions on some
    // singular paths) or when nodeIndex/dof are out of range. Return 0 in those cases —
    // safer default for any caller that forgot a `if (!r.singular)` check (callers who DO
    // check still see the same value because the vectors are full on a successful solve).
    real disp(int nodeIndex, int dof) const {
        if (nodeIndex < 0 || dof < 0 || dof >= DOF_PER_NODE) return 0;
        const size_t i = static_cast<size_t>(gdof(nodeIndex, dof));
        return (i < u.size()) ? u[i] : real(0);
    }
    real reaction(int nodeIndex, int dof) const {
        if (nodeIndex < 0 || dof < 0 || dof >= DOF_PER_NODE) return 0;
        const size_t i = static_cast<size_t>(gdof(nodeIndex, dof));
        return (i < reactions.size()) ? reactions[i] : real(0);
    }
};

} // namespace frame
