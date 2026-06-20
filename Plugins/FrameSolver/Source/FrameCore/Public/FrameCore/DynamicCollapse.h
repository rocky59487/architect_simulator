#pragma once
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveOptions.h"
#include "FrameCore/Collapse.h"        // CollapseOutcome, CollapseHingeEvent, FailMode (via ISectionStrength)
#include "FrameCore/Connectivity.h"    // FragmentCluster (now carries vel/angVel)
#include <functional>
#include <string>
#include <vector>

namespace frame {

// S2 N4 dynamic-collapse driver options. Engine units N-mm-tonne-s (so dt is in seconds).
struct DynCollapseOptions {
    real dt              = 1e-3;   // time step (s). Honest guide: dt <= T1/100 for accuracy.
    real maxTime         = 10.0;   // integration horizon (s)
    int  basisSize       = 30;     // Ritz / modal basis size. >= nf means "full basis" (exact, small models).
    bool useRitzVectors  = true;   // true = load-dependent Ritz (Wilson); false = pure eigenmodes (audit path).
    real rayleighAlpha   = 0.0;    // C = alpha*M + beta*K -> modal damping c_i = alpha + beta*omega_i^2 (diagonal).
    real rayleighBeta    = 0.0;
    real removeThreshold = 1.0;    // ElasticAllowable D/C screen (same screening grade as the static driver).
    int  screenEvery     = 5;      // do a D/C screen every N steps (recover cost control).
    real quietKineticRatio = 1e-6; // Stable when kinetic energy < ratio * peak history KE, held for one T1.
    int  maxEvents       = 64;     // event budget; exhausting it terminates MaxSteps.
    std::vector<MemberId> initialRemovals;        // t=0 scenario (GSA sudden removal)
    std::vector<int>      initialShellRemovals;
    int  frameStride     = 10;     // store a (u,v) frame every N steps (UE replay)
    SolveOptions solve;            // pivotTol / enableReleases / useTimoshenko passthrough

    // P1-3 (v2.7 live streaming): if set, called inside storeFrame() with the just-stored
    // frame, so a transport (e.g. the v2 dispatcher) can push each frame to the client BEFORE
    // runDynamicCollapse() returns. Default empty = old behaviour: frames still accumulate into
    // H.frames and the caller walks them after return; existing standalone fixtures and the v1
    // CLI are unaffected because they do not set the callback.
    std::function<void(const struct DynCollapseFrame&)> onFrameEmitted;
    // P1-3 (v2.7 live cancel): polled every frameStride steps. Returning true terminates the
    // integrator immediately; H.outcome stays Invalid, H.diagnostic is set to "cancelled by
    // caller", and H.frames keeps whatever was captured before cancel. Default empty = never
    // cancelled.
    std::function<bool()> isCancelled;
};

// One topology-changing event during the run. The lists are what was APPLIED at this event;
// the detached fragments carry their handoff velocity/angular-velocity (see Connectivity.h).
struct DynCollapseEvent {
    real t = 0;
    FailMode mode = FailMode::None;
    std::vector<MemberId> removedMembers;
    std::vector<int>      removedShells;
    std::vector<CollapseHingeEvent> formedHinges;   // reserved for the S2.1 ductile-dynamic extension
    std::vector<FragmentCluster> detached;          // each carries vel/angVel for the physics handoff

    real truncationResidual = 0;   // ||u' - Phi' q'|| / ||u'||  (basis-inheritance projection error)
    real energyBefore = 0;         // 1/2 vMv + 1/2 uKu - F.u of the pre-event configuration
    real energyAfter  = 0;         // retained-structure modal energy + sum of fragment rigid-body KE
};

// A 6N displacement + velocity snapshot for UE replay.
struct DynCollapseFrame {
    real t = 0;
    std::vector<real> u;   // 6N global displacement
    std::vector<real> v;   // 6N global velocity
};

struct DynCollapseHistory {
    CollapseOutcome outcome = CollapseOutcome::Invalid;
    std::string diagnostic;
    std::vector<DynCollapseEvent> events;
    std::vector<DynCollapseFrame> frames;
};

// S2 dynamic progressive-collapse driver: modal-space Newmark time integration over a sequence
// of brittle removal events, with cross-event state inheritance and momentum-preserving debris
// handoff. The caller's model is NEVER mutated (internal working copy; loads are whatever is
// baked in -- call addSelfWeight() / compose the combination first, same contract as the static
// runProgressiveCollapse). Per event the configuration is re-factored FRESH (assembleAndFactor):
// the post-removal connectivity cleanup pins debris nodes (= a support change beyond the
// ReSolve same-topology regime), and rebuilding the modal/Ritz basis needs the new K'_ff factor.
//
// Honest boundaries: linear elastic in modal space between events; failure criterion = screening
// D/C (NOT a code check); events trigger on a whole step (O(dt)); the debris handoff is one-way
// (a fragment does not feed back after leaving); truncation error is reported per event via
// truncationResidual (a full basis is exact; a truncated basis can miss high-frequency content);
// the fragment own-axis angular momentum uses the slender-rod closed form (the FE section polar
// term is dropped -- negligible for slender members). Plastic-hinge dynamics are reserved for S2.1.
FRAMECORE_API DynCollapseHistory runDynamicCollapse(const FrameModel& model,
                                                    const DynCollapseOptions& opts = {});

} // namespace frame
