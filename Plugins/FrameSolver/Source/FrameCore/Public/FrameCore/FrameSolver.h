#pragma once
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/SolveOptions.h"
#include <memory>

namespace frame {

// Opaque prepared system (PIMPL). Holds the assembled + factorized stiffness so that many
// load variations can be solved WITHOUT re-factorizing — the key enabler for interactive
// (UE5) re-solves and multi-load-case work. Eigen lives entirely in the .cpp-defined Impl,
// so this public type stays Eigen-free (POD boundary). Movable, non-copyable.
struct FRAMECORE_API PreparedSystem {
    PreparedSystem();
    ~PreparedSystem();
    PreparedSystem(PreparedSystem&&) noexcept;
    PreparedSystem& operator=(PreparedSystem&&) noexcept;
    PreparedSystem(const PreparedSystem&) = delete;
    PreparedSystem& operator=(const PreparedSystem&) = delete;
    struct Impl;
    std::unique_ptr<Impl> impl;

    // Criticality margin = min/max |LDLT pivot| of K_ff (see SolveResult::pivotMargin). Available
    // straight after assembleAndFactor without a solve. 0 if singular / not yet factored.
    real pivotMargin() const;
};

// Phase 1 (once per fixed model): build K, apply the support pattern, factorize (LDLᵀ),
// detect mechanisms from the factorization, and bake the distributed (UDL/pressure)
// equivalent loads + element geometry. This is the expensive step.
FRAMECORE_API PreparedSystem assembleAndFactor(const FrameModel& model, const SolveOptions& opts = {});

// Phase 2 (cheap, many times): reuse the factorization to solve for the CURRENT model's
// NODAL loads and PRESCRIBED (support-displacement) VALUES. Only forward/back-substitution.
// VALID ONLY while geometry, topology, support FLAGS and distributed loads are unchanged
// from assembleAndFactor — changing any of those needs a fresh assembleAndFactor. Prescribed
// VALUES and nodal loads may vary freely (the interactive / load-case path: e.g. a player
// dragging a support settlement re-solves in microseconds).
FRAMECORE_API SolveResult solveLoad(const PreparedSystem& prepared, const FrameModel& model);

// One-shot linear static solve = assembleAndFactor + solveLoad (back-compatible; bit-for-bit
// identical to the previous monolithic solver).
FRAMECORE_API SolveResult solve(const FrameModel& model, const SolveOptions& opts = {});

} // namespace frame
