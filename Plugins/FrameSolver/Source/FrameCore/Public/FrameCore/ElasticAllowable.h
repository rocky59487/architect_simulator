#pragma once
#include "FrameCore/ISectionStrength.h"

namespace frame {

// Elastic / allowable-stress combined-stress screen (spec PFSFv2-to-UE5 §1.4).
//   sigma = N/A +/- M/W ,  tau = |V|/A ,  demand/capacity per mode, argmax = dominant.
// This is a real-time SCREENING layer, NOT RC ultimate strength (no P-M interaction,
// no concrete nonlinearity). Biaxial bending uses the conservative |My|/Wy + |Mz|/Wz
// worst-corner sum (valid for rectangular; circular sections need resultant moment).
struct ElasticAllowable final : ISectionStrength {
    FRAMECORE_API DemandResult checkSection(const MemberEndForces& f, const Section& s, const Capacity& c) const override;
};

} // namespace frame
