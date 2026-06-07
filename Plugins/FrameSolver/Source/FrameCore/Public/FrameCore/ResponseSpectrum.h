#pragma once
#include "FrameCore/FrameSolver.h"
#include "FrameCore/ModalResult.h"
#include <vector>
#include <string>

namespace frame {

// Design response SPECTRUM: spectral pseudo-acceleration Sa (mm/s^2) vs period T (s),
// linearly interpolated. This is an INPUT (a code curve — ASCE 7 / Eurocode 8 / Taiwan CNS —
// is supplied by the caller); the engine does the modal combination, not the code curve.
struct Spectrum {
    std::vector<real> T;    // periods (s), ascending
    std::vector<real> Sa;   // spectral acceleration (mm/s^2)
    real at(real period) const {
        if (T.empty()) return 0;
        if (period <= T.front()) return Sa.front();
        if (period >= T.back())  return Sa.back();
        for (size_t i = 1; i < T.size(); ++i)
            if (period <= T[i]) {
                const real t = (period - T[i - 1]) / (T[i] - T[i - 1]);
                return Sa[i - 1] + t * (Sa[i] - Sa[i - 1]);
            }
        return Sa.back();
    }
};

enum class SpectrumCombo { SRSS, CQC };

struct ResponseSpectrumResult {
    bool              singular = false;
    std::string       diagnostic;
    std::vector<real> u;                 // combined peak displacement (6N)
    real              baseShear = 0;     // combined base shear in the excitation direction
    std::vector<real> effMass;           // per-mode effective (participating) mass
    real              totalMass = 0;     // total directional mass r^T M r — NOTE: the influence
                                         // vector r is set on ALL nodes (incl. constrained ones),
                                         // so totalMass includes support-node tributary mass.
                                         // Sum(effMass)/totalMass can therefore read <1 from BOTH
                                         // modal truncation AND constrained mass (multi-support).
};
// COVERAGE NOTE: the CQC branch's correlation formula is unit-tested for correctness in
// isolation, but the solveResponseSpectrum CQC *path* (combo = CQC) currently has NO numerical
// oracle in the gate (all F24 fixtures use SRSS). Adding a closely-spaced-mode CQC-vs-SRSS
// oracle is a known test-coverage gap. SRSS is fully gated.

// Modal RESPONSE-SPECTRUM analysis. For excitation along global DOF `excDof` (0=Ux,1=Uy,2=Uz),
// computes per-mode participation factors, peak modal responses scaled by Sa(T_n), and combines
// them by SRSS or CQC (damping `zeta`). Reuses the prepared system (for the mass matrix) and a
// prior ModalResult (the more modes, the more complete the participating mass).
FRAMECORE_API ResponseSpectrumResult solveResponseSpectrum(
    const PreparedSystem& prepared, const ModalResult& modes, const Spectrum& spectrum,
    int excDof, SpectrumCombo combo = SpectrumCombo::SRSS, real zeta = 0.05);

}  // namespace frame
