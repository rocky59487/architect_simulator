// v3.6 Phase 3 C6 — along-span BMD / SFD renderer. Reads FFrameStressField
// (computed via UFrameCoreStressFieldLibrary or any v2 dispatcher inspect.stress_field
// roundtrip) and emits a sign-aware ribbon mesh per member where the ribbon height
// at each sample is proportional to the selected force component
// (N / Vy / Vz / T / My / Mz). Positive values extrude on one side of the member
// axis, negative on the other -- classic textbook BMD / SFD visualisation.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameInternalForceFieldActor.generated.h"

class UProceduralMeshComponent;

UENUM(BlueprintType)
enum class EFrameForceComponent : uint8
{
    AxialN    UMETA(DisplayName="N (axial)"),
    ShearVy   UMETA(DisplayName="Vy (shear y)"),
    ShearVz   UMETA(DisplayName="Vz (shear z)"),
    TorsionT  UMETA(DisplayName="T (torsion)"),
    MomentMy  UMETA(DisplayName="My (moment about y)"),
    MomentMz  UMETA(DisplayName="Mz (moment about z)"),
};

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Internal Force Field Actor"))
class FRAMECOREUE_API AFrameInternalForceFieldActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameInternalForceFieldActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ForceField")
    FFrameStressField Field;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ForceField")
    TArray<FFrameMemberGeometry> MemberGeometry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ForceField")
    EFrameForceComponent Component = EFrameForceComponent::MomentMz;

    // mm of force value -> mm of ribbon extrusion. Default 100x because typical
    // small-fixture forces are O(N) / O(N*mm) and would not be visible otherwise.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ForceField",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float HeightScale = 100.f;

    // When true (default), positive values extrude on +RefY/+RefZ side and negative
    // on the opposite side; sign is preserved. When false, ribbon uses |value|.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ForceField")
    bool bDualSidedSigned = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ForceField")
    bool bAutoBuildOnBeginPlay = true;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|ForceField")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    UFUNCTION(BlueprintCallable, Category="FrameCore|ForceField")
    bool BuildMesh();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|ForceField")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    void BuildOneMemberSection(int32 SectionIdx,
                               const FFrameMemberStressTrace& Trace,
                               const FFrameMemberGeometry& Geom);

    // Pulls the selected component value from a stress-field sample.
    static float ExtractComponent(const FFrameStressFieldSample& S,
                                  EFrameForceComponent Comp);
};
