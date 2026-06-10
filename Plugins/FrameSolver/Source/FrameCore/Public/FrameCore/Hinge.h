#pragma once
#include "FrameCore/FrameTypes.h"

namespace frame {

// A formed plastic hinge (collapse stage 4a): explicit MODEL STATE, not a solve option.
// The hinge releases ONE bending rotation at a member end and keeps transmitting the signed
// residual plastic moment Mp -- the textbook hinge-by-hinge (event-to-event / sequential
// linear analysis) device. The element honours hinges UNCONDITIONALLY (not gated by
// SolveOptions.enableReleases): a formed hinge silently ignored would be a wrong stiffness.
//
// Mechanics split across two channels (both required):
//   1) ELEMENT side (automatic, inside BeamColumnElement::prepare): the hinge dof joins the
//      release mask and Mp feeds the fixed-end-force condensation, producing the carry-over
//      moment and shear couple on the member side.
//   2) NODE side (the caller's job -- the collapse driver does this; hand-built tests must
//      too): the joint must still receive the hinge moment from the released member, which
//      the condensed element can no longer deliver. Add a NodalLoad with global moment
//      components  -Mp * e_axis , where e_axis is the member's LOCAL y axis (dof 4/10) or
//      LOCAL z axis (dof 5/11) from memberLocalAxes(), at the hinge-end node.
//
// Sign convention: Mp is the member-end moment in the LOCAL END-FORCE convention (the value
// MemberEndForces .My/.Mz reads at formation -- create a hinge by copying that signed value).
// Recover() reads 0 at the released dof afterwards (condensation contract); the hinge's
// moment lives here, in Mp.
//
// Honest boundary: no unloading/reversal (a hinge never closes), uniaxial Mp (no My-Mz or
// N-M interaction), hinge length zero. This is sequential linear analysis, NOT true
// elastoplasticity.
struct PlasticHinge {
    MemberId member = 0;   // Member::id carrying the hinge
    int      dof    = 0;   // released LOCAL rotation: 4 = Ry@i, 5 = Rz@i, 10 = Ry@j, 11 = Rz@j
    real     Mp     = 0;   // SIGNED residual plastic moment (local end-force convention)
};

} // namespace frame
