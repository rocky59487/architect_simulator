// v3.5 Phase 3 — modal shape animator. Reads FFrameModalResult.Modes[ModeIndex].Shape,
// animates with sinusoidal amplitude * cos(2*pi*FreqHz * t * TimeScale). Mode shape is the
// engine's mass-normalised eigenvector; Amplitude is BP-tuned and is unrelated to actual
// modal energy (visual only — documented in class doc).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameModalShapeActor.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Modal Shape Actor"))
class FRAMECOREUE_API AFrameModalShapeActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameModalShapeActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Modal")
    FFrameModalResult Modes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Modal")
    TArray<FFrameMemberGeometry> MemberGeometry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Modal",
              meta=(ClampMin="0", UIMin="0"))
    int32 ModeIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Modal",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float Amplitude = 100.f;

    // 1.0 = real time, smaller = slow motion. Default 1.0. Audit D-04: clamp >= 0 so
    // we match the forward-only contract DynCollapseReplay / RealTimeDynamic use for
    // their PlaybackSpeed; a negative TimeScale would reverse the cos(omega * t)
    // animation, which is mathematically valid but BP-surprising.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Modal",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float TimeScale = 1.f;

    // Read-only animation phase in seconds (drives the cos term).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Modal")
    float CurrentPhase = 0.f;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|Modal")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    // Rebuild mesh at the current phase. Returns true if a member section was built.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Modal")
    bool BuildAtPhase(float PhaseSec);

    // Tick override: advance CurrentPhase by DeltaTime * TimeScale, then rebuild.
    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|Modal")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    void BuildOneMemberSection(int32 SectionIdx,
                               const FFrameMemberGeometry& Geom,
                               float DispScale);

    FVector LookupShapeDisplacement(int32 NodeIdx) const;
};
