#pragma once
#include "FrameCore/FrameTypes.h"

namespace frame {

// POD options threaded into solve(). MUST stay engine-agnostic: only real / bool /
// int / plain enums -- never UE or Eigen types -- so FrameCore stays pure.
struct SolveOptions {
    real pivotTol       = 1e-12;   // mechanism-detection pivot tolerance (relative to max|D|)
    bool enableReleases = false;   // honor Member.release[12] via per-element static condensation
    bool useTimoshenko  = false;   // include shear flexibility (needs Section.Asy/Asz > 0);
                                   // false keeps the Euler-Bernoulli element bit-for-bit.
};

} // namespace frame
