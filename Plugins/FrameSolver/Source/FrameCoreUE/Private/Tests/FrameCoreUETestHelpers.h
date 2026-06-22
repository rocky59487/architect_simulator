// V321-05: shared test-only forward declaration for FrameCoreUE::ToBlueprint.
//
// The actual implementation lives in Private/FrameCoreUETypes.cpp and is module-private
// (no public header exposes the engine POD -> USTRUCT marshal helper, since the
// UBlueprintFunctionLibrary is the only consumer outside FrameCoreUE that should ever
// reach for it). Test files need the prototype to cross-check the marshal against the
// engine POD directly without using the BP library.
//
// Prior to v3.2.2 each of the 6 test TUs declared this prototype inside a per-file
// `namespace FrameCoreUE { ... }` block, which meant any signature change to ToBlueprint
// required editing 6 files in lock-step. This header consolidates the declaration so the
// signature is single-sourced; the cost is one extra `#include` per test file.
//
// IMPORTANT: this header is for test TUs only. Do NOT expose it from
// `Public/FrameCoreUE/`. The marshal helper stays an implementation detail of the
// module; test access is the only legitimate consumer outside `FrameCoreUETypes.cpp`.

#pragma once

#include "FrameCoreUE/FrameCoreUETypes.h"

namespace frame { struct StressField; }

namespace FrameCoreUE
{
    FFrameStressField ToBlueprint(const frame::StressField& field);
}
