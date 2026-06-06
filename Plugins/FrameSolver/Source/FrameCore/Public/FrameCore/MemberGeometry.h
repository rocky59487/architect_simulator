#pragma once
#include "FrameCore/FrameTypes.h"

namespace frame {

// Local member axes as unit vectors expressed in GLOBAL coordinates:
//   outX = unit(pj - pi);  refVec defines the local x-y plane (auto-fallback to
//   global +Y when refVec is parallel to the axis);  (outX,outY,outZ) is right-handed.
// Eigen-free signature on purpose: this is the single shared source of the member
// local frame, consumed by BOTH the solver (ElementStiffness/transform) and any
// visualization, so the two can never drift.  v_local = [outX;outY;outZ] . v_global.
FRAMECORE_API void memberLocalAxes(const Vec3& pi, const Vec3& pj, const Vec3& refVec,
                                   Vec3& outX, Vec3& outY, Vec3& outZ);

} // namespace frame
