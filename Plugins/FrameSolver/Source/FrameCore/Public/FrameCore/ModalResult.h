#pragma once
#include "FrameCore/FrameTypes.h"
#include <vector>
#include <string>

namespace frame {

struct ModalOptions {
    int  numModes = 3;          // number of lowest modes to extract
    // Scale-up path: solve the generalized eigenproblem with a sparse SUBSPACE ITERATION
    // (pure-Eigen, reuses the LDLT) instead of the dense solver. OFF by default — the dense
    // solver is exact and fast for the modest interactive models here; turn it on for large
    // models. It produces the same M-normalized modes, and falls back to the dense path if it
    // fails to converge.
    bool useSparseSolver = false;
};

struct ModeShape {
    real              omega  = 0;   // circular frequency (rad/s)
    real              freqHz = 0;   // = omega / (2*pi)
    std::vector<real> shape;        // 6N mass-normalized eigenvector (constrained DOFs = 0)
};

struct ModalResult {
    bool                   singular = false;
    std::string            diagnostic;
    std::vector<ModeShape> modes;   // ascending frequency
};

}  // namespace frame
