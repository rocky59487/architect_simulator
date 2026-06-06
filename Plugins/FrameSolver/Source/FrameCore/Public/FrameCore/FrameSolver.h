#pragma once
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/SolveOptions.h"

namespace frame {

// Linear-elastic 3D Euler-Bernoulli direct stiffness solver.
// Assembles K, applies BCs by free-DOF reduction, factors with SimplicialLDLT,
// detects mechanisms from the factorization (never from connectivity), and
// recovers reactions + local member end forces.
// `opts` is a POD config (tolerances, feature flags); defaulted for back-compat.
FRAMECORE_API SolveResult solve(const FrameModel& model, const SolveOptions& opts = {});

} // namespace frame
