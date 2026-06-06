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

struct SolveResult {
    bool                         singular = false;   // mechanism / instability detected
    std::string                  diagnostic;
    std::vector<real>            u;                  // 6N global DOF displacements
    std::vector<real>            reactions;          // 6N global (nonzero at constrained DOF)
    std::vector<MemberForcePair> memberForces;

    real disp(int nodeIndex, int dof) const { return u[static_cast<size_t>(gdof(nodeIndex, dof))]; }
    real reaction(int nodeIndex, int dof) const { return reactions[static_cast<size_t>(gdof(nodeIndex, dof))]; }
};

} // namespace frame
