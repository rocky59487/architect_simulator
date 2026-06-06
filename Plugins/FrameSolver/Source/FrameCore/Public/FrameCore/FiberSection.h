#pragma once
#include "FrameCore/ISectionStrength.h"

namespace frame {

// Reinforced-concrete section parameters for the fiber-section precision layer.
// POD (engine-agnostic). MVP scope: rectangular section, two symmetric rebar layers
// (top/bottom), ACI-style equivalent rectangular (Whitney) concrete block + elastic-
// perfectly-plastic steel, plane sections, extreme-fibre concrete strain ecu.
// Deferred: confined concrete, slenderness/P-delta, strain hardening, fibre biaxial
// (biaxial uses a conservative linear moment-contour here).
struct RCSectionParams {
    real fc    = 28.0;       // f'c (MPa)
    real fy    = 420.0;      // steel yield (MPa)
    real Es    = 200000.0;   // steel modulus (MPa)
    real ecu   = 0.003;      // ultimate concrete strain
    real cover = 40.0;       // mm to each rebar-layer centroid
    real AsTop = 0.0;        // mm^2 (one face, "+y" side)
    real AsBot = 0.0;        // mm^2 (opposite face)
    int  nSweep = 240;       // neutral-axis sweep resolution for the P-M curve
};

// RC ultimate strength via fiber integration + P-M interaction. checkSection reads the
// rectangular geometry (b = 2*cy, d = 2*cz) from the Section, computes the axial-moment
// interaction capacity, and returns D/C = demand/capacity. Same ISectionStrength seam
// as ElasticAllowable, so the solver/converter are agnostic to which one is injected.
class FiberSection final : public ISectionStrength {
public:
    explicit FiberSection(const RCSectionParams& params) : P(params) {}
    FRAMECORE_API DemandResult checkSection(const MemberEndForces& f, const Section& s, const Capacity& c) const override;
private:
    RCSectionParams P;
};

} // namespace frame
