#pragma once
#include "FrameCore/ISectionStrength.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveResult.h"

namespace frame {

// Elastic / allowable-stress combined-stress screen (spec PFSFv2-to-UE5 §1.4).
//   sigma = N/A +/- M/W ,  tau = |V|/A ,  demand/capacity per mode, argmax = dominant.
// This is a real-time SCREENING layer, NOT RC ultimate strength (no P-M interaction,
// no concrete nonlinearity). Biaxial bending uses the conservative |My|/Wy + |Mz|/Wz
// worst-corner sum (valid for rectangular; circular sections need resultant moment).
struct ElasticAllowable final : ISectionStrength {
    FRAMECORE_API DemandResult checkSection(const MemberEndForces& f, const Section& s, const Capacity& c) const override;
};

// Structural utilization aggregate (C3 / collapse foundation). The worst Demand/Capacity over all
// ACTIVE members (both ends), and the elastic SAFETY FACTOR = 1/maxDC: the linear load multiplier
// that brings the most-utilized member to its first allowable-stress limit ("how far from first
// failure"). This is the same ElasticAllowable SCREEN aggregated per structure — NOT an RC
// ultimate-strength margin. Kept as a free post-process (the solver stays capacity-free), matching
// combine()/envelope(): a pure function of (model, result).
struct DemandSummary {
    real     maxDC          = 0;            // worst Demand/Capacity (0 if no screenable active member)
    real     safetyFactor   = 0;            // 1/maxDC; +infinity when maxDC == 0; 0 when invalid
    MemberId governingMember = 0;           // id of the member carrying maxDC
    FailMode mode           = FailMode::None;
    bool     valid          = false;        // false when no active member could be screened
};
FRAMECORE_API DemandSummary worstUtilization(const FrameModel& model, const SolveResult& r);

} // namespace frame
