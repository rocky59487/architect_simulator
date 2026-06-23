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
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "ProceduralMeshComponent.h"

namespace frame { struct StressField; }

namespace FrameCoreUE
{
    FFrameStressField ToBlueprint(const frame::StressField& field);
}

// v3.5.1 TEST-DUP-01: shared spawn-world + tip-center helpers used by all v3.5
// renderer-actor tests. Inline, anonymous-namespace-safe (each TU sees a separate
// inline definition but the helper is stateless so they collapse to the same
// behaviour). Keeps the per-test boilerplate minimal.
namespace FrameCoreUETestHelpers
{
    inline UWorld* GetSpawnWorld()
    {
        if (!GEngine) return nullptr;
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            if (Ctx.World()) return Ctx.World();
        }
        return nullptr;
    }

    // Average of the 4 corner positions of ring 10 (the tip ring of an 11-ring PMC
    // extrusion). Returns the deformed tip center. Caller must guarantee the section
    // has at least 11*4 = 44 vertices.
    inline FVector TipCenter(const FProcMeshSection* Sec)
    {
        const int32 BaseTip = 10 * 4;
        FVector C = FVector::ZeroVector;
        for (int32 c = 0; c < 4; ++c) { C += Sec->ProcVertexBuffer[BaseTip + c].Position; }
        return C * 0.25f;
    }
}
