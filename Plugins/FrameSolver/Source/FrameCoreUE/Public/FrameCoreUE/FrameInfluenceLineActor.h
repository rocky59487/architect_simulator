// v3.5 Phase 6 — influence line renderer. Reads FFrameInfluenceLine.ReactionAtPosition[k]
// and paints a colour band along the path showing the reaction value at each load position.
// Negative = blue, zero = white, positive = red (symmetric ramp).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameInfluenceLineActor.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Influence Line Actor"))
class FRAMECOREUE_API AFrameInfluenceLineActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameInfluenceLineActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|InfluenceLine")
    FFrameInfluenceLine Line;

    // PathGeometry has one entry per LoadNode; world-space position is in Start. Length must
    // match Line.LoadNodes.Num() to draw correctly (mismatch silently truncates to the
    // shorter array length).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|InfluenceLine")
    TArray<FFrameMemberGeometry> PathGeometry;

    // Visual height scaling of the influence ribbon (mm of influence value -> mm of ribbon
    // height). Default 100x because influence values are typically O(1).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|InfluenceLine",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float HeightScale = 100.f;

    // v3.6 U-10: engine reactionInfluenceLine assumes a unit -Z load convention.
    // Designers may use a unit +Z convention; this toggle flips the value sign at
    // paint time so positive influence is the side they expect. Default false
    // matches the engine convention.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|InfluenceLine")
    bool bFlipPolarity = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|InfluenceLine")
    bool bAutoBuildOnBeginPlay = true;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|InfluenceLine")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    UFUNCTION(BlueprintCallable, Category="FrameCore|InfluenceLine")
    bool BuildMesh();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|InfluenceLine")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;
};
