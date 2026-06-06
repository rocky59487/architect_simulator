#pragma once
#include "FrameCore/FrameTypes.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include <array>

namespace frame {

// Common member-end release patterns. Local DOF order per end is [Ux Uy Uz Rx Ry Rz].
enum class ReleasePreset { Rigid, TrussPin, HingeI, HingeJ };

// Build a release[12] mask for a preset:
//   Rigid    = no releases (moment-resisting frame member).
//   TrussPin = all 6 rotations (both ends) released -> a 2-force / axial member.
//   HingeI/J = the two bending rotations (Ry,Rz) at end i / j -> a moment hinge.
FRAMECORE_API std::array<bool, 12> makeRelease(ReleasePreset p);

// A prismatic 3D beam-column connecting node i -> node j.
struct Member {
    MemberId        id  = 0;
    NodeId          i   = 0;
    NodeId          j   = 0;
    const Material* mat = nullptr;
    const Section*  sec = nullptr;

    // Reference vector defining the local x-y plane. Default global +Z; the
    // solver falls back to +Y automatically when refVec is parallel to the axis.
    Vec3 refVec { 0, 0, 1 };

    // Per-DOF end release (truss / hinge mode). MVP keeps all false; this is the
    // hook for the truss idealization (release rotational DOFs) added later.
    std::array<bool, 12> release { {} };

    Member() = default;
    Member(MemberId id_, NodeId i_, NodeId j_, const Material* m, const Section* s)
        : id(id_), i(i_), j(j_), mat(m), sec(s) {}
};

} // namespace frame
