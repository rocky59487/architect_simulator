// v3.5 Phase 2 — D/C utilization heat-map renderer. Reads FFrameSolveResult.MemberUtilization
// (peak Risk per member) + ShellUtilization, paints a per-member colour band along the span.
// SaturationDC maps DC value 1.0 -> "red" by default; a designer can shrink it (e.g. 0.5) to
// detect early warning, or grow it (e.g. 2.0) to flag overstressed sections specifically.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"
#include "FrameUtilizationHeatmapActor.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Utilization Heatmap Actor"))
class FRAMECOREUE_API AFrameUtilizationHeatmapActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameUtilizationHeatmapActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Heatmap")
    FFrameSolveResult Solution;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Heatmap")
    TArray<FFrameMemberGeometry> MemberGeometry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Heatmap")
    TArray<FFrameShellGeometry> ShellGeometry;

    // DC value mapped to the "red" end of the ramp. Default 1.0 (== member at allowable).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Heatmap",
              meta=(ClampMin="0.01", UIMin="0.01"))
    float SaturationDC = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Heatmap")
    bool bAutoBuildOnBeginPlay = true;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|Heatmap")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    // Rebuild the procedural mesh. Returns true if at least one member OR shell section was
    // built. Each member emits 1 PMC section coloured by per-end Risk; each shell emits 1
    // section as a 2-triangle quad coloured by Risk.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Heatmap")
    bool BuildHeatmap();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|Heatmap")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    void BuildOneMemberSection(int32 SectionIdx, const FFrameMemberGeometry& Geom,
                               float SatGuard, float RiskValue);
    void BuildOneShellSection (int32 SectionIdx, const FFrameShellGeometry& Geom,
                               float SatGuard, float RiskValue);
};
