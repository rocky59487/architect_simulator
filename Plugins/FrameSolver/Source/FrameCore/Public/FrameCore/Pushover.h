#pragma once
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/SolveOptions.h"
#include <vector>

namespace frame {

// One event in an event-to-event plastic-hinge pushover.
struct PushoverStep {
    real              lambda = 0;     // cumulative reference-load factor at this hinge event
    std::vector<real> u;              // accumulated 6N displacements at this step
    int               hingeMember = -1;
    int               hingeEnd = -1;  // 0 = end i, 1 = end j
};

struct PushoverResult {
    bool                     ok = false;
    std::string              diagnostic;
    real                     collapseLambda = 0;   // load factor at the collapse mechanism
    std::vector<PushoverStep> steps;               // one per hinge formed (last -> mechanism)
};

// First-order rigid-plastic pushover: ramp the reference load, insert a plastic hinge
// (REUSES the member-release machinery) wherever |Mz| first reaches Mp = Wz*cap.bend,
// re-solve the released structure incrementally (no Mp applied as an external load --
// it is carried in the accumulated end moments), and stop when the stiffness becomes
// singular = collapse mechanism (REUSES the pivot-based detector). Monotonic, small
// displacement, single load pattern.
FRAMECORE_API PushoverResult pushover(const FrameModel& model, const SolveOptions& opts, int maxSteps);

} // namespace frame
