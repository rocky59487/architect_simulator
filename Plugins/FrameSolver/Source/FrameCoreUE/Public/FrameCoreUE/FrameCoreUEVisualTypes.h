#pragma once

// v3.5 Visual surface — shared USTRUCT types consumed by every v3.5 renderer actor.
//
// These structs let a BP designer wire engine results (FFrameSolveResult / FFrameModalResult
// / FFrameDynCollapseResult / FFrameInfluenceLine) to a scene-space renderer without writing
// C++. The engine emits per-NODE displacement; the engine does NOT know node positions in
// world space (those live in the model). Each renderer actor takes:
//
//   1. The engine result USTRUCT.
//   2. A geometry list (one entry per member or shell) supplying world-space position +
//      cross-section + node-index mapping. Cross-section is reused from v3.3
//      FFrameMemberGeometry (extended with EndINodeIdx / EndJNodeIdx). Shell geometry is a
//      new v3.5 sibling type FFrameShellGeometry.
//
// Engine source delta: 0 lines under Plugins/FrameSolver/Source/FrameCore/.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "FrameCoreUEVisualTypes.generated.h"

// ---------------------------------------------------------------------------
// FFrameShellGeometry — per-shell world-space corner positions for v3.5 renderers.
//
// One entry per shell in the source FrameModel. CornerIndices[k] points into
// FFrameSolveResult::Displacements (so a renderer can lerp deformed corners by adding
// Displacement[CornerIndices[k]] * scale). World-space corner positions are kept here
// (not in the engine) so the engine remains pure-numerics consumer-side.
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameShellGeometry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Visual")
    int32 ShellIdx = -1;

    // World-space corner positions, CCW about +local-z. Length-4 enforced at use site.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Visual")
    TArray<FVector> Corners;

    // Per-corner node index into FFrameSolveResult::Displacements (parallel to Corners).
    // Length-4 enforced. -1 entries skip displacement lookup (treated as zero).
    //
    // v3.5 NOTE: reserved field. v3.5's AFrameUtilizationHeatmapActor (the only shell
    // consumer this release) paints by Risk and ignores CornerNodeIndices. A v3.6
    // deformed-shell renderer (`AFrameDeformedShellActor`) will consume this field to
    // lerp corner positions by per-node displacement. Populating now is forward-safe.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Visual")
    TArray<int32> CornerNodeIndices;
};

// ---------------------------------------------------------------------------
// FFrameModelPatch — Phase 7 InteractiveSubsystem patch payload.
//
// Mirrors the engine's S1 ReSolve session deactivate/reactivate operations, plus nodal-load
// increment. Applied via UFrameInteractiveSubsystem::ApplyPatchAndResolve.
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameModelPatch
{
    GENERATED_BODY()

    // Member IDs to deactivate (toggle to inactive). Each id is a user-assigned member id.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Interactive")
    TArray<int32> DeactivateMemberIds;

    // Member IDs to reactivate (toggle back to active).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Interactive")
    TArray<int32> ReactivateMemberIds;

    // Shell IDs to deactivate.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Interactive")
    TArray<int32> DeactivateShellIds;

    // Shell IDs to reactivate.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Interactive")
    TArray<int32> ReactivateShellIds;

    // Reserved for v3.6: incremental nodal-load patch. v3.5 ships activation toggles only.
};
