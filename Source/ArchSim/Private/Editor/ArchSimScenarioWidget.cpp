// ArchSim — UArchSimScenarioWidget implementation.
// Sprint S-05, SPIKE-Scenario-u1. Plan ref: docs/logs/S-05/plan_*.md § SPIKE-Scenario-u1.
//
// Architecture decision confirmed: Option A (WITH_EDITOR guard in runtime module).
// If a future audit requires Option B (separate ArchSimEditor module), the ESCALATE trigger
// would be: "Source/ArchSim/Public/Editor/ subdir structure conflicts UE convention OR
// WITH_EDITOR guard is insufficient for packaging."
//
// PlaceK1Column flow:
//   1. Obtain Editor World via GEditor->GetEditorWorldContext().World() so the Actor lands
//      in the correct level under Edit-mode.
//   2. SpawnActor<AActor> at LocationWorld (default rotation).
//   3. NewObject<UArchSimMemberData> with the spawned Actor as Outer; default EndIOffsetUE/
//      EndJOffsetUE are (-50,0,0)/(+50,0,0) cm = 1 m horizontal beam along +X local.
//   4. RegisterComponent() + Actor->AddInstanceComponent() so the component appears in the
//      Details panel and persists via SaveGame.
//   5. UArchSimModelRegistry::RegisterMember(Comp) — succeeds only when a live GameInstance
//      is present (PIE); no-ops / logs warning in edit-mode (Registry is null without GI).
//
// WHY SpawnActor + NewObject instead of SpawnActorDeferred?
//   SpawnActorDeferred requires FinishSpawning which needs Blueprint context we don't have
//   here. For a pure-C++ runtime component add, NewObject + RegisterComponent is the
//   canonical UE5 pattern (ref: UE docs "Creating Components at Runtime").
//
// WHY GEditor->GetEditorWorldContext()?
//   In an EditorUtilityWidget the canonical world accessor is GEditor->GetEditorWorldContext()
//   because GetWorld() on UUserWidget returns the Game world only during PIE. This gives us
//   the correct Editor level context in both PIE and non-PIE sessions.
//
// FROZEN guard: this file touches zero lines under Plugins/FrameSolver/Source/FrameCore/
// per v4.0.0 FROZEN contract. UArchSimMemberData and UArchSimModelRegistry are game-body
// (Source/ArchSim/) — not frozen.

#include "Editor/ArchSimScenarioWidget.h"

#if WITH_EDITOR

#include "Components/ArchSimMemberData.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Editor.h"           // GEditor
#include "UObject/UObjectGlobals.h"

// Log category already declared in ArchSimGameInstance.h / .cpp
DECLARE_LOG_CATEGORY_EXTERN(LogArchSim, Log, All);

void UArchSimScenarioWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // No extra init needed for u1 skeleton. u2/u3 may wire solve-request delegates here.
}

AActor* UArchSimScenarioWidget::PlaceK1Column(FVector LocationWorld)
{
    // -- Step 1: Obtain the Editor world -----------------------------------------
    // GEditor->GetEditorWorldContext() returns the active Edit-mode level context.
    // During PIE this is the PIE world, not the EditorWorld transient — that is fine
    // because we want the Actor in the level the player is actually editing/testing.
    if (!GEditor)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceK1Column — GEditor is null; "
                    "cannot place K1 outside of Editor context."));
        return nullptr;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceK1Column — Editor World is null; "
                    "open a level before placing K1."));
        return nullptr;
    }

    // -- Step 2: Spawn a plain AActor as the K1 placeholder ----------------------
    // No mesh asset is required at u1 scope; the Blueprint child class can assign a
    // StaticMeshComponent asset in the Details panel (AS-05 art asset backlog).
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* PlacedActor = World->SpawnActor<AActor>(
        AActor::StaticClass(),
        FTransform(FRotator::ZeroRotator, LocationWorld, FVector::OneVector),
        SpawnParams);

    if (!PlacedActor)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceK1Column — SpawnActor failed "
                    "at location (%.1f, %.1f, %.1f)."),
               LocationWorld.X, LocationWorld.Y, LocationWorld.Z);
        return nullptr;
    }

    // -- Step 3: Attach UArchSimMemberData with default 1 m beam geometry --------
    // Default EndIOffsetUE(-50,0,0) / EndJOffsetUE(+50,0,0) cm = 1 m along +X local
    // (matches UArchSimMemberData ctor defaults — no extra assignment needed).
    UArchSimMemberData* MemberComp =
        NewObject<UArchSimMemberData>(PlacedActor,
                                      UArchSimMemberData::StaticClass(),
                                      TEXT("ArchSimMemberData"));
    if (!MemberComp)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceK1Column — NewObject<UArchSimMemberData> "
                    "failed; actor will be spawned without registry link."));
        return PlacedActor; // Still return the actor — non-fatal for skeleton
    }

    // RegisterComponent wires the component into the actor's lifecycle (BeginPlay,
    // Tick, EndPlay). AddInstanceComponent makes it visible in the Details panel
    // and serializable via SaveGame. Order matters: Register first, then Add.
    MemberComp->RegisterComponent();
    PlacedActor->AddInstanceComponent(MemberComp);

    // -- Step 4: Register with UArchSimModelRegistry if available ----------------
    // Registry is a GameInstanceSubsystem — null in pure edit-mode without PIE.
    // PlaceK1Column is still useful (spawns + attaches component) even without Registry.
    // BeginPlay on MemberData will also attempt auto-registration, so the defer is safe.
    UArchSimModelRegistry* Registry = UArchSimModelRegistry::Get(World);
    if (Registry)
    {
        int32 AssignedIdx = Registry->RegisterMember(MemberComp);
        if (AssignedIdx < 0)
        {
            UE_LOG(LogArchSim, Warning,
                   TEXT("UArchSimScenarioWidget::PlaceK1Column — RegisterMember returned -1 "
                        "(validation failure: check actor origin / member axis). "
                        "Actor spawned but not registered."));
        }
        else
        {
            UE_LOG(LogArchSim, Display,
                   TEXT("UArchSimScenarioWidget::PlaceK1Column — K1 registered at "
                        "MemberIdx=%d, loc=(%.1f,%.1f,%.1f)."),
                   AssignedIdx, LocationWorld.X, LocationWorld.Y, LocationWorld.Z);
        }
    }
    else
    {
        // WHY this is expected in edit-mode: UArchSimModelRegistry is a
        // UGameInstanceSubsystem which only exists during PIE / packaged builds.
        // In pure edit-mode (no PIE) Get(World) returns nullptr. BeginPlay on
        // UArchSimMemberData will attempt registration when PIE starts.
        UE_LOG(LogArchSim, Display,
               TEXT("UArchSimScenarioWidget::PlaceK1Column — Registry not available "
                    "(edit-mode, no PIE). Actor placed; registration deferred to BeginPlay."));
    }

    return PlacedActor;
}

#endif // WITH_EDITOR
