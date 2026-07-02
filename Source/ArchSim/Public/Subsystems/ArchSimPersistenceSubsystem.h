// ArchSim - UArchSimPersistenceSubsystem : SPUD-wired persistence orchestrator.
// AS-08-u1 (Sprint S-08). Closes the persistence chain begun in S-01/A1-07.
//
// Design (Placement-Replay Sidecar):
//   SPUD cannot save runtime-spawned bare AActor members directly because:
//     (a) UArchSimMemberData is an AddInstanceComponent component; SPUD's property
//         scan walks the Actor object's UPROPERTY(SaveGame) fields, not dynamically-
//         added instance components (SpudState.cpp:755 RestoreObjectProperties scans
//         Actor->GetClass() properties, not the instance-component list).
//     (b) EndIOffsetUE/EndJOffsetUE on UArchSimMemberData have no SaveGame flag
//         (ArchSimMemberData.h:50-54), so geometry is not recovered from SaveGame
//         archives on any path.
//     (c) Runtime actors require a SPUD SpudGuid property (FGuid) to be tracked
//         across save/load (SpudState.cpp:314 GetSpawnedActorData; SpudPropertyUtil.cpp
//         :492 WriteActorRefPropertyData); our K-set actors are bare AActor with no
//         such property.
//   Support nodes have no actor representation at all (pure FFrameModelDef state).
//
//   Chosen path:
//     This subsystem registers itself as a SPUD global object
//     (SpudSubsystem::AddPersistentGlobalObjectWithName). Its UPROPERTY(SaveGame)
//     TArrays capture member placement records + support positions at SaveGame time.
//     On LoadGame completion the arrays are replayed: Registry is cleared, supports
//     are re-registered, member actors are respawned via a replay helper.
//
//   MemberIdx semantics after load:
//     Indices are re-assigned monotonically from 0 (replay order preserves logical
//     member order). CachedUtilization is re-computed after the post-replay Solve;
//     the stored SaveGame value is a display fallback shown before the first Solve.
//
// Slot convention:
//   Manual saves: "ArchSimSlot_<N>" (N = 1-based, callers use SaveToSlot(1..N)).
//   Auto-save:    SPUD built-in AutoSave slot (slot name "__AutoSave__").
//   Quick-save:   SPUD built-in QuickSave slot (slot name "__QuickSave__").
//   Only one user-slot at a time is expected in the current MVP; multi-slot UI
//   is deferred to a later task.
//
// SPUD integration notes:
//   * This subsystem must be registered with SPUD after SpudSubsystem is available.
//     We do that in UArchSimGameInstance::Init via GetSubsystem<USpudSubsystem>.
//   * SPUD global-object restore fires BEFORE PostLoadMap level restore
//     (SpudSubsystem.cpp:963-972 shows GlobalObjects restored before OpenLevel).
//     Our own PostLoadGame delegate fires AFTER the full load completes (SpudSubsystem
//     .cpp:981 LoadComplete broadcasts PostLoadGame). We subscribe PostLoadGame to
//     trigger the replay on the now-fully-loaded level.
//
// PIE timing caveat (for AS-08-u2):
//   SPUD's NewGame call is deferred 0.2 s in PIE mode (SpudSubsystem.cpp Initialize).
//   SaveGame/LoadGame require RunningIdle state. PIE smoke must wait for SPUD
//   NewGame to complete before calling Save or Load.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ArchSimPersistenceSubsystem.generated.h"

// Per-member placement record stored in the sidecar for save/load replay.
// All geometry in UE centimetres (world-space for world-transform fields,
// local-space for EndI/J offset fields — mirrors UArchSimMemberData layout).
USTRUCT(BlueprintType)
struct ARCHSIM_API FArchSimMemberRecord
{
    GENERATED_BODY()

    // World-space actor transform at placement time.
    // Used to reposition the replayed AActor via SetActorTransform.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    FTransform WorldTransform = FTransform::Identity;

    // Local-space end-I offset in UE cm (mirrors UArchSimMemberData::EndIOffsetUE).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    FVector EndIOffsetUE = FVector(-50.f, 0.f, 0.f);

    // Local-space end-J offset in UE cm (mirrors UArchSimMemberData::EndJOffsetUE).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    FVector EndJOffsetUE = FVector(+50.f, 0.f, 0.f);

    // Optional grouping id; -1 = ungrouped (mirrors UArchSimMemberData::StructureGroupId).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    int32 StructureGroupId = -1;

    // MaterialId index (mirrors UArchSimMemberData::MaterialId; 0 = default S275).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    int32 MaterialId = 0;

    // SectionId index (mirrors UArchSimMemberData::SectionId; 0 = default 200×200 rect).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    int32 SectionId = 0;
};

UCLASS()
class ARCHSIM_API UArchSimPersistenceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // UGameInstanceSubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ---- Save surface (BP-callable) -----------------------------------------

    // Persist the current model state to the named slot.
    // Slot name "ArchSimSlot_1" is the default MVP slot. Pass any non-empty name
    // to use an alternate slot. Returns false + logs if SPUD is not in RunningIdle
    // state or no model is present.
    UFUNCTION(BlueprintCallable, Category="ArchSim|Persistence")
    bool SaveToSlot(const FString& SlotName = TEXT("ArchSimSlot_1"));

    // ---- Load surface (BP-callable) -----------------------------------------

    // Restore the model from the named slot. The SPUD state machine triggers a
    // map reload; the replay fires asynchronously in PostLoadGame. Returns false
    // + logs if SPUD is not in RunningIdle state or the slot does not exist.
    UFUNCTION(BlueprintCallable, Category="ArchSim|Persistence")
    bool LoadFromSlot(const FString& SlotName = TEXT("ArchSimSlot_1"));

    // ---- Snapshot API (called by widget / gameplay code before Save) ---------

    // Capture the current world's member placements + support positions into
    // the SaveGame arrays. Must be called before SaveToSlot so the sidecar is
    // up-to-date. In production, SaveToSlot calls this internally.
    UFUNCTION(BlueprintCallable, Category="ArchSim|Persistence")
    void SnapshotCurrentModel();

    // ---- Reset API (called before replay on load) ----------------------------

    // Clear the Registry's CurrentModel, IndexToComponent, and all session state.
    // Call before replaying a loaded sidecar to avoid stale model data.
    // WHY: Registry has no public Reset; we add one in ArchSimModelRegistry (see
    // ArchSimModelRegistry.h/cpp addendum). This is the minimal production change.
    UFUNCTION(BlueprintCallable, Category="ArchSim|Persistence")
    void ResetRegistry();

    // ---- read-only accessors (tests) ----------------------------------------
    [[nodiscard]] int32 GetMemberRecordCount() const { return MemberRecords.Num(); }
    [[nodiscard]] int32 GetSupportCount() const { return SupportPositions.Num(); }

private:
    // ---- UPROPERTY(SaveGame) sidecar arrays (scanned by SPUD global-object path) --

    // One record per placed structural member, in registration order.
    // SPUD scans UPROPERTY(SaveGame) on this UObject (SpudPropertyUtil.cpp:24-31
    // ShouldPropertyBeIncluded: CPF_SaveGame flag + not Deprecated).
    UPROPERTY(SaveGame)
    TArray<FArchSimMemberRecord> MemberRecords;

    // Fixed support node positions in FrameCore mm.
    // WHY stored in mm (not UE cm): RegisterFixedSupport takes mm; avoids a
    // double-conversion round-trip that could accumulate floating-point drift
    // across save-load cycles.
    UPROPERTY(SaveGame)
    TArray<FVector> SupportPositions;

    // ---- internal helpers ----------------------------------------------------

    // Bound to USpudSubsystem::PostLoadGame. Fires after the full load is complete
    // (SpudSubsystem.cpp:981 LoadComplete). Triggers replay on the loaded level.
    UFUNCTION()
    void OnPostLoadGame(const FString& SlotName, bool bSuccess);

    // Replay MemberRecords + SupportPositions into the live Registry.
    // WHY deferred to a separate method: called from OnPostLoadGame (delegate)
    // so the level actors are fully available (BeginPlay has run).
    void ReplayLoadedSidecar(UWorld* World);
};
