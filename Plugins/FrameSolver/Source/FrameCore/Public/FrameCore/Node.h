#pragma once
#include "FrameCore/FrameTypes.h"
#include <array>

namespace frame {

// A structural joint with 6 DOF. fixed[d]=true constrains DOF d to prescribed[d].
struct Node {
    NodeId id = 0;
    Vec3   pos;
    std::array<bool, 6> fixed       { { false, false, false, false, false, false } };
    std::array<real, 6> prescribed  { { 0, 0, 0, 0, 0, 0 } };

    Node() = default;
    Node(NodeId i, real x, real y, real z) : id(i), pos{ x, y, z } {}

    void fixAll() { fixed = { true, true, true, true, true, true }; }     // encastre
    void pinTranslations() { fixed = { true, true, true, false, false, false }; }
};

} // namespace frame
