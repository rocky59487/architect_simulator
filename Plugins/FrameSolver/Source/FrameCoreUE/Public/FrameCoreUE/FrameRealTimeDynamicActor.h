// v3.5 Phase 8 — real-time modal-dynamics replay. Reads FFrameModalTimeHistory.Steps (per
// Newmark step displacement snapshots) and animates the deformed structure through them
// at PlaybackSpeed (== 1.0 = real time). Same pattern as DynCollapseReplay (Phase 4) but
// drives a modal time-history instead of a collapse history.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameRealTimeDynamicActor.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Real-Time Dynamic Actor"))
class FRAMECOREUE_API AFrameRealTimeDynamicActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameRealTimeDynamicActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|RealTimeDynamic")
    FFrameModalTimeHistory History;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|RealTimeDynamic")
    TArray<FFrameMemberGeometry> MemberGeometry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|RealTimeDynamic",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float PlaybackSpeed = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|RealTimeDynamic",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float DeflectionScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|RealTimeDynamic")
    bool bLoop = false;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|RealTimeDynamic")
    float CurrentTime = 0.f;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|RealTimeDynamic")
    bool bPlaying = true;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|RealTimeDynamic")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    UFUNCTION(BlueprintCallable, Category="FrameCore|RealTimeDynamic") void Play()  { bPlaying = true; }
    UFUNCTION(BlueprintCallable, Category="FrameCore|RealTimeDynamic") void Pause() { bPlaying = false; }

    UFUNCTION(BlueprintCallable, Category="FrameCore|RealTimeDynamic")
    void SetPlaybackTime(float NewTime);

    UFUNCTION(BlueprintCallable, Category="FrameCore|RealTimeDynamic")
    bool RebuildAtCurrentTime();

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|RealTimeDynamic")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    void BuildOneMemberSection(int32 SectionIdx, const FFrameMemberGeometry& Geom,
                               const TArray<FFrameNodalDisplacement>& Lerped);
};
