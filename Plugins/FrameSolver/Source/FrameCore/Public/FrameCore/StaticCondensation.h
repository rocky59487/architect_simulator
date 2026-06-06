#pragma once
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameTypes.h"
#include "FrameCore/SolveOptions.h"
#include <vector>
#include <string>

namespace frame {

// Schur static condensation (substructuring / superelement reduction).
// Partition the assembled UNCONSTRAINED global system into boundary DOFs (b, the
// caller-supplied set) and internal DOFs (i, the complement), then eliminate the
// internal DOFs exactly:
//     S    = K_bb - K_bi * K_ii^-1 * K_ib            (condensed boundary stiffness)
//     fEff = f_b  - K_bi * K_ii^-1 * f_i             (condensed boundary load)
// Solving  S * u_b = fEff  with the boundary supports applied reproduces the boundary
// displacements of the full solve bit-for-bit (this is the M8 #10 oracle).
//
// CONTRACT: condensation operates on the UNCONSTRAINED stiffness (it ignores Node
// supports). The internal set therefore must be genuinely free — never put a DOF you
// intend to constrain into the internal set, or K_ii becomes singular. The caller
// applies supports/loads to the returned boundary system.
//
// IMPORTANT: exact for STATICS only. For dynamics / modal substructuring it degrades
// to the Guyan approximation (drops internal-DOF inertia) — use CMS (Craig-Bampton),
// not this. (spec PFSFv2-to-UE5 §0 boundary ii, §2-3.)
//
// Robustness (PFSF §3.1-3.2): the internal block K_ii is factored with SimplicialLDLT;
// its diagonal condition number kappa_D = max|D| / min|D| drives an adaptive pivot
// tolerance eps = pivotTol * max(1, sqrt(kappa_D / 1e4)). A near-zero/negative pivot
// flags an internal mechanism; kappa_D > 1e10 is a HARD REJECT (ill-conditioned
// substructure). Either way ok=false with a diagnostic, never NaNs.
struct CondensationResult {
    bool              ok  = false;
    std::string       why = "static condensation not implemented (deferred)";
    int               boundaryCount = 0;
    std::vector<real> S;               // row-major boundaryCount x boundaryCount (when ok)
    std::vector<real> fEff;            // length boundaryCount (when ok)
    real              conditionNumber = 0;  // kappa_D estimate of the internal block K_ii
};

FRAMECORE_API CondensationResult condenseStatic(const FrameModel& model,
                                                const std::vector<int>& boundaryGlobalDofs,
                                                const SolveOptions& opts = {});

} // namespace frame
