#pragma once
#include "FrameCore/FrameTypes.h"
#include <array>

namespace frame {

// Concentrated load/moment applied at a node, in GLOBAL coordinates.
struct NodalLoad {
    NodeId              node = 0;
    std::array<real, 6> comp { { 0, 0, 0, 0, 0, 0 } };   // Fx, Fy, Fz, Mx, My, Mz
};

// Uniformly distributed load over a member, in LOCAL coordinates (force / length).
struct MemberUDL {
    MemberId member = 0;
    Vec3     w_local;
};

// Concentrated load on a member at distance a from end i, in LOCAL coordinates.
struct MemberPointLoad {
    MemberId member = 0;
    Vec3     p_local;
    real     a = 0;
};

} // namespace frame
