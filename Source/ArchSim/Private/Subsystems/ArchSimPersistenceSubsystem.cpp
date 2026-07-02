// ArchSim - UArchSimPersistenceSubsystem implementation.
// AS-08-u1 (Sprint S-08).
//
// See header for full design rationale.

#include "Subsystems/ArchSimPersistenceSubsystem.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "Components/ArchSimMemberData.h"

// SPUD public API
#include "SpudSubsystem.h"       // USpudSubsystem (GameInstanceSubsystem)

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"            // TActorIterator

DEFINE_LOG_CATEGORY_STATIC(LogArchSimPersistence, Log, All);

// Stable name used to register this subsystem with SPUD's global-object store.
// Must match across save/load cycles; changing it is a breaking format change.
// (SpudSubsystem::AddPersistentGlobalObjectWithName keyed by FString identity.)
static const FString kSpudGlobalName = TEXT("ArchSimPersistenceSidecar");

// Slot name prefix for manually-named user slots.
static const FString kSlotPrefix = TEXT("ArchSimSlot_");

// ---- UGameInstanceSubsystem lifecycle ----------------------------------------

void UArchSimPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Register ourselves as a SPUD named global object so our UPROPERTY(SaveGame)
    // TArrays are included in every save/load cycle without needing a level actor.
    //
    // WHY AddPersistentGlobalObjectWithName (not AddPersistentGlobalObject):
    //   UGameInstanceSubsystem instances can have non-deterministic FNames across
    //   sessions (the subsystem manager may produce suffixes like _0, _1).
    //   Named registration uses our stable kSpudGlobalName as the identity key so
    //   SPUD always finds the same record in GOBS regardless of runtime FName.
    //   (SpudSubsystem::AddPersistentGlobalObjectWithName → NamedGlobalObjects.Add;
    //   SpudSubsystem.cpp:1017-1019.)
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("Initialize: no GameInstance; SPUD registration skipped."));
        return;
    }

    USpudSubsystem* Spud = GI->GetSubsystem<USpudSubsystem>();
    if (!Spud)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("Initialize: USpudSubsystem not found; SPUD registration skipped."));
        return;
    }

    // AddPersistentGlobalObjectWithName is idempotent for the same Name key
    // (NamedGlobalObjects is a TMap; Add overwrites the same key).
    Spud->AddPersistentGlobalObjectWithName(this, kSpudGlobalName);

    // Subscribe to PostLoadGame so we can replay the sidecar after a full load.
    // WHY PostLoadGame (not PostLevelRestore): PostLoadGame fires after both
    // global-object restore AND the level restore are complete (SpudSubsystem.cpp:
    // 981 LoadComplete), so level actors spawned by SPUD are fully available.
    Spud->PostLoadGame.AddDynamic(
        this, &UArchSimPersistenceSubsystem::OnPostLoadGame);

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("Initialize: registered with SPUD as '%s'."), *kSpudGlobalName);
}

void UArchSimPersistenceSubsystem::Deinitialize()
{
    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        if (USpudSubsystem* Spud = GI->GetSubsystem<USpudSubsystem>())
        {
            Spud->RemovePersistentGlobalObject(this);

            // Unbind delegate before we go away.
            Spud->PostLoadGame.RemoveDynamic(
                this, &UArchSimPersistenceSubsystem::OnPostLoadGame);
        }
    }

    Super::Deinitialize();
}

// ---- Save -------------------------------------------------------------------

void UArchSimPersistenceSubsystem::SnapshotCurrentModel()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;
    UArchSimModelRegistry* Registry = GI->GetSubsystem<UArchSimModelRegistry>();
    if (!Registry)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("SnapshotCurrentModel: UArchSimModelRegistry not found."));
        return;
    }

    const FFrameModelDef& Model = Registry->GetCurrentModel();

    // Rebuild MemberRecords from the currently registered components.
    // We walk IndexToComponent (private TMap) indirectly via GetCurrentModel().
    // Components are the authoritative source for geometry fields that FrameCore
    // does not store (EndIOffsetUE / EndJOffsetUE in UE cm local-space).
    //
    // WHY iterate Members[] (not components directly): ensures we capture only
    // members that FrameCore actually accepted (registered successfully), and
    // preserves their serialised order (stable for replay).
    MemberRecords.Reset();
    for (int32 Idx = 0; Idx < Model.Members.Num(); ++Idx)
    {
        const FFrameMember& M = Model.Members[Idx];
        if (!M.bActive) continue;   // skip deactivated members

        // Retrieve the component for this member index via Registry read-accessor.
        // Registry::IndexToComponent is private; we use GetRegisteredCount/GetCurrentModel
        // to verify existence, then search actors in the current world.
        // WHY: there is no public accessor for the component map by design (it is
        // internal to the registry). We need to find the actor another way.
        //
        // Fallback: walk all actors in the world and find the one whose MemberData
        // component reports the same MemberIdx. O(N×M) but only runs at Save time
        // (not per-frame). MVP cap is <=500 members/level so cost is acceptable.
        //
        // Future opt: add a Registry::GetComponentForIndex(int32) accessor if
        // this becomes a profiling hotspot.
        UWorld* World = GI->GetWorld();
        if (!World) break;

        bool bFoundComponent = false;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor) continue;

            TArray<UActorComponent*> Comps;
            Actor->GetComponents(UArchSimMemberData::StaticClass(), Comps);
            for (UActorComponent* C : Comps)
            {
                UArchSimMemberData* MD = Cast<UArchSimMemberData>(C);
                if (!MD || MD->MemberIdx != Idx) continue;

                FArchSimMemberRecord Rec;
                Rec.WorldTransform  = Actor->GetActorTransform();
                Rec.EndIOffsetUE    = MD->EndIOffsetUE;
                Rec.EndJOffsetUE    = MD->EndJOffsetUE;
                Rec.StructureGroupId= MD->StructureGroupId;
                Rec.MaterialId      = MD->MaterialId;
                Rec.SectionId       = MD->SectionId;
                MemberRecords.Add(Rec);
                bFoundComponent = true;
                break;
            }
            if (bFoundComponent) break;
        }

        if (!bFoundComponent)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("SnapshotCurrentModel: no component found for MemberIdx=%d; "
                        "this member will not be saved."), Idx);
        }
    }

    // Rebuild SupportPositions: walk all nodes with Fixed[0]==true (all-fixed node).
    // WHY Fixed[0] check (not length check): a free node also has Fixed.Num()==6;
    // the distinguishing marker is Fixed[0..5] all true (idempotent from RegisterFixedSupport).
    SupportPositions.Reset();
    for (const FFrameNode& Node : Model.Nodes)
    {
        bool bAllFixed = (Node.Fixed.Num() == 6);
        for (bool F : Node.Fixed) { if (!F) { bAllFixed = false; break; } }
        if (bAllFixed)
        {
            // Store in FrameCore mm (RegisterFixedSupport takes mm).
            SupportPositions.Add(Node.Pos);
        }
    }

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("SnapshotCurrentModel: captured %d members + %d supports."),
           MemberRecords.Num(), SupportPositions.Num());
}

bool UArchSimPersistenceSubsystem::SaveToSlot(const FString& SlotName)
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return false;

    USpudSubsystem* Spud = GI->GetSubsystem<USpudSubsystem>();
    if (!Spud)
    {
        UE_LOG(LogArchSimPersistence, Warning, TEXT("SaveToSlot: USpudSubsystem not found."));
        return false;
    }

    if (!Spud->IsIdle())
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("SaveToSlot: SPUD not in RunningIdle; save refused."));
        return false;
    }

    // Snapshot BEFORE handing off to SPUD so the sidecar arrays are current.
    // SPUD calls StoreGlobalObject (SpudState.cpp:405) which scans our
    // UPROPERTY(SaveGame) fields via VisitPersistentProperties; by the time
    // FinishSaveGame runs those fields must already hold the latest state.
    SnapshotCurrentModel();

    // Validate: do not save an empty model (a 0-member save is technically valid
    // but almost certainly a user error; warn + allow to let SPUD proceed).
    if (MemberRecords.Num() == 0 && SupportPositions.Num() == 0)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("SaveToSlot: sidecar is empty (no members, no supports). "
                    "Saving an empty slot."));
    }

    // Delegate to SPUD. SaveGame is async; caller can bind PostSaveGame delegate
    // if it needs completion notification.
    // bTakeScreenshot = false (no render thread in commandlet / headless).
    Spud->SaveGame(SlotName, FText::FromString(SlotName),
                   /*bTakeScreenshot=*/false);

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("SaveToSlot: issued SaveGame for slot '%s' "
                "(%d members, %d supports)."),
           *SlotName, MemberRecords.Num(), SupportPositions.Num());
    return true;
}

// ---- Load -------------------------------------------------------------------

bool UArchSimPersistenceSubsystem::LoadFromSlot(const FString& SlotName)
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return false;

    USpudSubsystem* Spud = GI->GetSubsystem<USpudSubsystem>();
    if (!Spud)
    {
        UE_LOG(LogArchSimPersistence, Warning, TEXT("LoadFromSlot: USpudSubsystem not found."));
        return false;
    }

    if (!Spud->IsIdle())
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("LoadFromSlot: SPUD not in RunningIdle; load refused."));
        return false;
    }

    // SPUD's LoadGame triggers a map reload. The actual replay happens in
    // OnPostLoadGame once SPUD finishes restoring global objects + the level.
    Spud->LoadGame(SlotName);

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("LoadFromSlot: issued LoadGame for slot '%s'. "
                "Replay will fire in OnPostLoadGame."), *SlotName);
    return true;
}

// ---- Reset ------------------------------------------------------------------

void UArchSimPersistenceSubsystem::ResetRegistry()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;
    UArchSimModelRegistry* Registry = GI->GetSubsystem<UArchSimModelRegistry>();
    if (!Registry) return;
    Registry->Reset();
}

// ---- PostLoadGame delegate --------------------------------------------------

void UArchSimPersistenceSubsystem::OnPostLoadGame(const FString& SlotName, bool bSuccess)
{
    if (!bSuccess)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("OnPostLoadGame: load of slot '%s' failed; replay skipped."),
               *SlotName);
        return;
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UWorld* World = GI->GetWorld();
    if (!World)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("OnPostLoadGame: no World available; replay skipped."));
        return;
    }

    // SPUD already restored our UPROPERTY(SaveGame) arrays via StoreGlobalObject
    // path (SpudSubsystem.cpp:963-972 RestoreGlobalObject before OpenLevel, then
    // SpudState.cpp:942-951 RestoreGlobalObject). At this point MemberRecords and
    // SupportPositions hold the values from the save file. The subsystem instance
    // (owned by UGameInstance) survives OpenLevel, so those restored arrays are
    // still the same objects when this PostLoadGame callback fires.
    ReplayLoadedSidecar(World);
}

void UArchSimPersistenceSubsystem::ReplayLoadedSidecar(UWorld* World)
{
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return;

    UArchSimModelRegistry* Registry = GI->GetSubsystem<UArchSimModelRegistry>();
    if (!Registry)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("ReplayLoadedSidecar: Registry not available."));
        return;
    }

    // Clear stale model from pre-load state. Registry::Reset() also clears the
    // timer so no debounce fires on stale model.
    Registry->Reset();

    // 1. Replay fixed supports (must come before members so support nodes dedup
    //    correctly against any member endpoint nodes that happen to coincide).
    //    WHY supports-first: in a typical portal frame K1 column feet sit exactly
    //    on the support node. Registering supports first ensures the Fixed flag is
    //    set before the member's FindOrAddNode sees the same position.
    for (const FVector& SupportPosMm : SupportPositions)
    {
        const int32 NodeIdx = Registry->RegisterFixedSupport(SupportPosMm);
        if (NodeIdx < 0)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayLoadedSidecar: RegisterFixedSupport failed for "
                        "(%.1f,%.1f,%.1f) mm."),
                   SupportPosMm.X, SupportPosMm.Y, SupportPosMm.Z);
        }
    }

    // 2. Replay member placements.
    // For each MemberRecord: spawn a bare AActor, graft a SceneComponent root
    // (mirrors PlaceKSetMember pattern from AS-36 fix), add a UArchSimMemberData
    // instance component, configure geometry, call RegisterMember.
    //
    // WHY spawn a new AActor (not search existing actors): after a SPUD LoadGame
    // the map is fully reloaded (OpenLevel call in SpudSubsystem.cpp:977). The
    // original K-set actors are gone. There is no SPUD respawn path because the
    // actors did not implement ISpudObject + SpudGuid (SPUD respawn requires both;
    // SpudState.cpp:306-343 GetSpawnedActorData). So we create fresh actors here.
    //
    // The newly created actors are NOT registered with SPUD (no ISpudObject, no
    // SpudGuid) — their geometry is managed entirely by the sidecar replay on the
    // next save/load cycle. This is by design: the sidecar IS the source of truth.
    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    int32 ReplayedCount = 0;
    for (const FArchSimMemberRecord& Rec : MemberRecords)
    {
        // Spawn at Identity; we will set the transform after root is established.
        AActor* Actor = World->SpawnActor<AActor>(
            AActor::StaticClass(), FTransform::Identity, SP);
        if (!Actor)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayLoadedSidecar: SpawnActor failed for record %d; "
                        "member skipped."), ReplayedCount);
            continue;
        }

        // Graft a root SceneComponent so SetActorTransform takes effect.
        // WHY: bare AActor has no RootComponent; SetActorLocation silently drops
        // on rootless actors (AS-36 lesson; AS-36-u1 fix in PlaceKSetMember).
        USceneComponent* Root = NewObject<USceneComponent>(
            Actor, USceneComponent::StaticClass(), TEXT("ReplayRoot"));
        if (!Root)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayLoadedSidecar: NewObject<USceneComponent> failed "
                        "for record %d; member skipped."), ReplayedCount);
            Actor->Destroy();
            continue;
        }
        Actor->SetRootComponent(Root);
        Root->RegisterComponent();
        Actor->SetActorTransform(Rec.WorldTransform);

        // Add the ArchSimMemberData instance component with geometry from the record.
        UArchSimMemberData* MD = NewObject<UArchSimMemberData>(
            Actor, UArchSimMemberData::StaticClass(), NAME_None);
        if (!MD)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayLoadedSidecar: NewObject<UArchSimMemberData> failed "
                        "for record %d; member skipped."), ReplayedCount);
            Actor->Destroy();
            continue;
        }
        MD->EndIOffsetUE    = Rec.EndIOffsetUE;
        MD->EndJOffsetUE    = Rec.EndJOffsetUE;
        MD->StructureGroupId= Rec.StructureGroupId;
        MD->MaterialId      = Rec.MaterialId;
        MD->SectionId       = Rec.SectionId;

        Actor->AddInstanceComponent(MD);
        MD->RegisterComponent();

        // Register with the engine model. RegisterMember reads the actor's
        // world transform + component offsets; the actor is already positioned.
        const int32 NewIdx = Registry->RegisterMember(MD);
        if (NewIdx < 0)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayLoadedSidecar: RegisterMember failed for record %d "
                        "(zero-length or validation error); member skipped."),
                   ReplayedCount);
            // Actor remains in the world but unregistered. It will not affect
            // the structural model. A future cleanup task can sweep orphans.
        }

        ++ReplayedCount;
    }

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("ReplayLoadedSidecar: replayed %d/%d members + %d supports. "
                "Registry now has %d registered."),
           ReplayedCount, MemberRecords.Num(),
           SupportPositions.Num(),
           Registry->GetRegisteredCount());

    // 3. Kick a solve so the heatmap updates immediately after load.
    //    WHY RequestSolve with empty patch: a fresh session hasn't had a patch
    //    yet; the debounce timer will fire ExecuteSolve which calls
    //    FlushAndStartSession on the rebuilt model.
    //    WHY delay (timer approach not used here): RegisterMember already arms the
    //    debounce timer internally via each member's BeginPlay → RequestSolve chain.
    //    But in a replay scenario BeginPlay may not fire (we did not go through the
    //    normal actor-spawn + BeginPlay path). So explicitly kick a solve.
    //
    //    NOTE: the FFrameModelPatch{} empty patch still causes ExecuteSolve to
    //    call FlushAndStartSession and run a full baseline solve.
    FFrameModelPatch EmptyPatch{};
    Registry->RequestSolve(EmptyPatch);
}
