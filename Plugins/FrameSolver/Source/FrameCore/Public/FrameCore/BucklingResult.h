#pragma once
#include "FrameCore/FrameTypes.h"
#include <vector>
#include <string>

namespace frame {

struct BucklingResult {
    bool              singular = false;
    std::string       diagnostic;
    real              criticalFactor = 0;   // lambda_cr: P_cr = lambda_cr * applied reference load.
                                            // When BucklingOptions::shellBucklingKnockdown>0 this
                                            // holds the KNOCKED-DOWN design value (alpha*lambda_cr);
                                            // otherwise == reportedCriticalFactor.
    real              reportedCriticalFactor = 0;  // R2.1 AC-06: the raw eigenvalue from the
                                            // eigensolve, BEFORE any shell-buckling knockdown.
                                            // Always populated; equals criticalFactor when no
                                            // knockdown is applied.
    real              knockdownFactor = 1;  // R2.1 AC-06: the alpha actually used; 1 means
                                            // raw eigenvalue (default), <1 means design value.
    std::vector<real> mode;                 // 6N buckling mode shape (constrained DOFs = 0)
};

}  // namespace frame
