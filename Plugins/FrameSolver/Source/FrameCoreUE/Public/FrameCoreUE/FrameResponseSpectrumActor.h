// v3.5 Phase 8 — response spectrum animator. Reads FFrameResponseSpectrumResult.PeakDisplacements
// (already mode-combined per SRSS / CQC), pulses the deformed structure with a slow sinusoidal
// envelope so designers can see the spectrum-combined response shape interactively. Engine
// gives one steady-state peak per node; this actor animates the visual amplitude only
// (educational — the engine spectrum result is itself a stationary peak).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameResponseSpectrumActor.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Response Spectrum Actor"))
class FRAMECOREUE_API AFrameResponseSpectrumActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameResponseSpectrumActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ResponseSpectrum")
    FFrameResponseSpectrumResult Response;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ResponseSpectrum")
    TArray<FFrameMemberGeometry> MemberGeometry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ResponseSpectrum",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float Amplitude = 100.f;

    // Slow envelope frequency in Hz. Default 0.5 Hz = 2-second period (visible pulse).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ResponseSpectrum",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float EnvelopeHz = 0.5f;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ResponseSpectrum")
    float CurrentPhase = 0.f;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|ResponseSpectrum")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    UFUNCTION(BlueprintCallable, Category="FrameCore|ResponseSpectrum")
    bool BuildAtPhase(float PhaseSec);

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|ResponseSpectrum")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    FVector LookupPeakDisplacement(int32 NodeIdx) const;
    void BuildOneMemberSection(int32 SectionIdx, const FFrameMemberGeometry& Geom, float Scale);
};
