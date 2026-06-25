// ArchSim - UArchSimMemberData : structural-engineering tag on a placed building component.
// A1-01 (Sprint S-01). Owns the link between a UE Actor and the FrameCore engine model
// row; persists across SaveGame so a loaded world reattaches to the same Members[] slot.
//
// Sister class: UArchSimModelRegistry (A1-02). See docs/IMPLEMENTATION_PLAN.md Sprint S-01.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArchSimMemberData.generated.h"

UCLASS(ClassGroup=(ArchSim), meta=(BlueprintSpawnableComponent),
       DisplayName="ArchSim Member Data")
class ARCHSIM_API UArchSimMemberData : public UActorComponent
{
    GENERATED_BODY()

public:
    UArchSimMemberData();

    // Internal Members[] index in UArchSimModelRegistry::CurrentModel; -1 = unregistered.
    // Persists across SaveGame round-trip; on Load the registry re-binds by this id.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Registry")
    int32 MemberIdx = -1;

    // Optional grouping id (members of the same prefab share a StructureGroupId).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Registry")
    int32 StructureGroupId = -1;

    // Peak D/C ratio from the latest Solve. Written by the registry's
    // DistributeSolveResult; consumed by heatmap / HUD.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Result")
    float CachedUtilization = 0.f;

    // FrameCore material library index in FFrameModelDef::Materials. 0 = registry's
    // default material (S275 steel) auto-inserted on first register.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArchSim|Member")
    int32 MaterialId = 0;

    // FrameCore section library index in FFrameModelDef::Sections. 0 = registry's
    // default section (200mm x 200mm rectangular) auto-inserted on first register.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArchSim|Member")
    int32 SectionId = 0;

    // Local-space offset (UE cm) of end-I from the owning actor's transform centre.
    // The registry transforms these by the actor's world transform to get world-space
    // endpoints, then multiplies by 10 to convert UE cm to FrameCore mm.
    // Default (-50,0,0)..(+50,0,0) cm = a 1 m horizontal beam along +X local.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArchSim|Member|Geometry")
    FVector EndIOffsetUE = FVector(-50.f, 0.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArchSim|Member|Geometry")
    FVector EndJOffsetUE = FVector(+50.f, 0.f, 0.f);

    // True once the registry has assigned MemberIdx and added the row to CurrentModel.
    // Reset to false on DeactivateMember.
    UPROPERTY(BlueprintReadOnly, Category="ArchSim|Registry")
    bool bRegistered = false;

    UFUNCTION(BlueprintPure, Category="ArchSim|Result")
    float GetCachedUtilization() const { return CachedUtilization; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
