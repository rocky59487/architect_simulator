// v3.6 Phase 4 C7 — along-span D/C utilization field. Distinct from v3.5
// AFrameUtilizationHeatmapActor (which paints per-member peak Risk). C7 paints
// SAMPLE-by-SAMPLE D/C from the stress field, derived from
// max( SigmaCompMax / Cap.Comp, SigmaTensMax / Cap.Tens, TauShear / Cap.Shear ).
// Same elastic-allowable screen as the engine D/C, applied per sample along span.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"   // FFrameCapacity
#include "FrameUtilizationFieldActor.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Utilization Field Actor (along-span)"))
class FRAMECOREUE_API AFrameUtilizationFieldActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameUtilizationFieldActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|UtilField")
    FFrameStressField Field;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|UtilField")
    TArray<FFrameMemberGeometry> MemberGeometry;

    // Per-member-index capacity table (Cap.Comp, Cap.Tens, Cap.Shear in MPa). When
    // a member's MemberIdx is OOB or the entry is zero, the D/C for that segment is
    // taken as 0 (cannot evaluate without capacity).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|UtilField")
    TArray<FFrameCapacity> Capacities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|UtilField",
              meta=(ClampMin="0.01", UIMin="0.01"))
    float SaturationDC = 1.0f;

    // When true, only paints segments with DC > 1.0 (exceedance only). Default false
    // paints the full ramp.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|UtilField")
    bool bShowExceedanceOnly = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|UtilField")
    bool bAutoBuildOnBeginPlay = true;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|UtilField")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    UFUNCTION(BlueprintCallable, Category="FrameCore|UtilField")
    bool BuildMesh();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|UtilField")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    void BuildOneMemberSection(int32 SectionIdx,
                               const FFrameMemberStressTrace& Trace,
                               const FFrameMemberGeometry& Geom);

    float SampleDC(const FFrameStressFieldSample& S, const FFrameCapacity& Cap) const;
};
