// v3.6 Phase 5 C8 — 贅餘度 (redundancy) sample renderer. For each watched member,
// the actor asks the interactive subsystem to deactivate it, resolve, measure the
// resulting MaxDC jump on the remaining structure, then reactivate. The redundancy
// of a member is HIGH when removing it causes a LARGE DC jump (the structure
// depends on it). Paints each watched member by its computed jump.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameRedundancyFieldActor.generated.h"

class UProceduralMeshComponent;
class UFrameInteractiveSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFrameRedundancyComputedDelegate);

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Redundancy Field Actor"))
class FRAMECOREUE_API AFrameRedundancyFieldActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameRedundancyFieldActor();

    // BP-set reference to the subsystem holding an active session. The actor uses
    // the subsystem's ReSolveSession to deactivate/reactivate members.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Redundancy")
    TObjectPtr<UFrameInteractiveSubsystem> Subsystem;

    // Member ids the actor will probe. Typically the user's "critical members" set.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Redundancy")
    TArray<int32> WatchedMemberIds;

    // Geometry for the watched members. MemberIdx pairs to WatchedMemberIds by
    // *index in the WatchedMemberIds array*, not by engine member id.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Redundancy")
    TArray<FFrameMemberGeometry> MemberGeometry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Redundancy",
              meta=(ClampMin="0.001", UIMin="0.001"))
    float SaturationJump = 0.5f;

    // Last computed jumps (parallel to WatchedMemberIds; filled by ComputeRedundancy).
    // A "jump" is (post-removal MaxDC) - (baseline MaxDC). +Inf encodes a mechanism
    // (the structure becomes singular when the member is removed).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Redundancy")
    TArray<float> LastJumps;

    // Multicast fired when ComputeRedundancy finishes (sync — the compute runs on
    // the calling thread; the delegate is for BP graph chaining).
    UPROPERTY(BlueprintAssignable, Category="FrameCore|Redundancy")
    FFrameRedundancyComputedDelegate OnRedundancyComputed;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|Redundancy")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    // Run the redundancy sweep. Returns the number of members successfully probed.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Redundancy")
    int32 ComputeRedundancy();

    UFUNCTION(BlueprintCallable, Category="FrameCore|Redundancy")
    bool BuildMesh();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|Redundancy")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    void BuildOneMemberSection(int32 SectionIdx, const FFrameMemberGeometry& Geom,
                               float JumpValue);
};
