// v3.5 Phase 1 — deformed-shape renderer. Reads FFrameSolveResult.Displacements +
// TArray<FFrameMemberGeometry> and emits a UProceduralMeshComponent box-extrusion mesh
// along the displaced member axes. DeflectionScale visually amplifies the deformation
// (default 100x) because serviceable structural displacements are typically sub-millimetre
// and would otherwise be invisible in viewport.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameDeformedShapeActor.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Deformed Shape Actor"))
class FRAMECOREUE_API AFrameDeformedShapeActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameDeformedShapeActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Deformed")
    FFrameSolveResult Solution;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Deformed")
    TArray<FFrameMemberGeometry> MemberGeometry;

    // Visual amplification factor applied to nodal displacements. Default 100x because
    // serviceable structural displacements are typically sub-millimetre — without
    // amplification the deformed shape is visually indistinguishable from undeformed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Deformed",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float DeflectionScale = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Deformed")
    bool bAutoBuildOnBeginPlay = true;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|Deformed")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    // Rebuild the procedural mesh from current Solution + MemberGeometry. Returns true if
    // at least one member section was built (false on empty inputs).
    UFUNCTION(BlueprintCallable, Category="FrameCore|Deformed")
    bool BuildMesh();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|Deformed")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    // Emit one PMC section per member, sampled along the deformed axis using a cubic-Hermite
    // interpolation between the two end displacements (if EndINodeIdx / EndJNodeIdx are set;
    // otherwise straight lerp).
    void BuildOneMemberSection(int32 SectionIdx, const FFrameMemberGeometry& Geom);

    // Look up displacement for a NodeIdx; returns zero vector if the idx is OOB or -1.
    FVector GetNodalDisplacement(int32 NodeIdx) const;
};
