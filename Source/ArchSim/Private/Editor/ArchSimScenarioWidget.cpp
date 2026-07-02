// ArchSim — UArchSimScenarioWidget implementation.
// Sprint S-05, SPIKE-Scenario-u1/u2/u3. Plan ref: docs/logs/S-05/plan_*.md § SPIKE-Scenario.
//
// Architecture decision confirmed: Option A (WITH_EDITOR guard in runtime module).
// If a future audit requires Option B (separate ArchSimEditor module), the ESCALATE trigger
// would be: "Source/ArchSim/Public/Editor/ subdir structure conflicts UE convention OR
// WITH_EDITOR guard is insufficient for packaging."
//
// PlaceKSetMember shared helper (u3 refactor, replaces duplicated PlaceK1Column body):
//   1. Obtain Editor World via GEditor->GetEditorWorldContext().World().
//   2. SpawnActor<AActor> at LocationWorld.
//   3. NewObject<UArchSimMemberData>; set EndIOffsetUE / EndJOffsetUE from params.
//   4. RegisterComponent() + Actor->AddInstanceComponent().
//   5. UArchSimModelRegistry::RegisterMember(Comp) — no-op in edit-mode (Registry null).
//   6. Append to PlacedActors (widget's soft list; GC-safe via TObjectPtr).
//
// PlaceK1Column (u1): delegates to PlaceKSetMember(-50,0,0 / +50,0,0 cm).
// PlaceK2Beam   (u3): delegates to PlaceKSetMember(-100,0,0 / +100,0,0 cm) — 2 m +X.
// PlaceK4Brace  (u3): delegates to PlaceKSetMember(-71,0,-71 / +71,0,+71 cm) — 2 m 45° XZ.
//
// RequestSolveAndVisualize flow (u2, unchanged):
//   1. Prefer PIE world (GEditor->PlayWorld); fall back to Editor world.
//   2. UArchSimModelRegistry::Get(World) — null without live GameInstance → return false.
//   3. Subscribe OnSolveComplete (idempotent; skip if SolveCompleteDelegateHandle.IsValid).
//   4. RequestSolve(empty FFrameModelPatch{}) — fires the 150 ms debounce.
//   5. OnSolveComplete callback: lazy-spawn HeatmapActor → populate → BuildHeatmap().
//
// Tutorial state machine (u3):
//   AdvanceTutorialStep() advances TutorialState linearly; fires OnTutorialStateChanged +
//   OnVoicePromptShouldPlay (both BlueprintImplementableEvent). FreeExplore is terminal.
//
// ResetWidgetState (u3):
//   Unsubscribes delegate, destroys HeatmapActor if valid, resets TutorialState = Welcome,
//   clears PlacedActors list (does NOT destroy the K-set actors — student keeps them).
//
// WHY PIE world preference:
//   GEditor->PlayWorld is the live PIE world while PIE is active. UArchSimModelRegistry is
//   a UGameInstanceSubsystem, which only exists for a live GameInstance (PIE or packaged).
//   GetEditorWorldContext().World() is the Editor world, which has no GI → Registry null.
//   We prefer PlayWorld so we acquire the same world the Registry lives in.
//
// WHY FDelegateHandle + BeginDestroy unsubscribe:
//   DECLARE_MULTICAST_DELEGATE subscribers are stored by function pointer + object pair.
//   If the widget is GC'd without unsubscribing, the next Registry broadcast attempts to
//   call a dead function pointer → undefined behavior. BeginDestroy is the earliest
//   safe UObject-guaranteed teardown hook in UE5.
//
// WHY BuildMemberGeometryFromRegistry is static:
//   It is a pure transformation of Registry state — no widget member is read. Making it
//   static prevents accidental capture of `this` in future refactors.
//
// FROZEN guard: zero lines under Plugins/FrameSolver/Source/FrameCore/ (v4.0.0 FROZEN).

// v0.4.0.1 (AS-28): own header MUST be the first #include (UE5.5+ IWYU first-header rule).
// Prior order placed FrameCoreUE headers first, which made UBT reject the obj rebuild
// silently (Result: Succeeded but obj stale) — that's why the cross-world fix wasn't
// taking effect until include order was corrected.
#include "Editor/ArchSimScenarioWidget.h"

// FrameCoreUEVisualTypes.h included before WITH_EDITOR guard because FFrameMemberGeometry
// is used in BuildMemberGeometryFromRegistry's return type, and the header declaration
// (also outside WITH_EDITOR) needs the complete type to be visible in the same TU.
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"   // FFrameModelPatch (FFrameMemberGeometry is in FrameCoreUETypes.h, pulled in transitively)
#include "FrameCoreUE/FrameCoreUEModelTypes.h"    // FFrameModelDef, FFrameNode, FFrameMember, FFrameSection

#if WITH_EDITOR

#include "Components/ArchSimMemberData.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"   // FFrameSolveResult
#include "FrameCoreUE/FrameUtilizationHeatmapActor.h"  // AFrameUtilizationHeatmapActor
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Editor.h"           // GEditor, GEditor->PlayWorld
#include "UObject/UObjectGlobals.h"

#include "ArchSimGameInstance.h"  // DECLARE_LOG_CATEGORY_EXTERN(LogArchSim, ...)

// Forward-declare the file-scope static helper so OnSolveComplete can call it before
// its definition (C++ requires declaration-before-use for file-scope functions).
// FFrameMemberGeometry and UArchSimModelRegistry are fully visible here (both already
// included above). Definition is below OnSolveComplete (after K-set placement / solve flow).
static TArray<FFrameMemberGeometry> BuildMemberGeometryFromRegistry(UArchSimModelRegistry* Registry);

// ---------------------------------------------------------------------------
// Tutorial state machine helpers (file-scope, no class coupling needed)
// ---------------------------------------------------------------------------

// Map each tutorial state to its prompt FText. Called by GetCurrentPromptText() and
// AdvanceTutorialStep(). Separated here to keep the switch out of the header and avoid
// FText LOCTEXT macro pollution in a header included by UHT.
// WHY LOCTEXT with explicit namespace: ArchSimTutorial — prevents key collision with
// other LOCTEXT namespaces in the same binary.
#define LOCTEXT_NAMESPACE "ArchSimTutorial"
static FText GetPromptForState(EArchSimTutorialState State)
{
    switch (State)
    {
    case EArchSimTutorialState::Welcome:
        return LOCTEXT("WelcomePrompt",
            "Welcome! You are about to test structural ideas. Click Next to begin.");
    case EArchSimTutorialState::PromptPlaceK1:
        return LOCTEXT("PlaceK1Prompt",
            "Place a K1 Column. Click a spot in the world, then press Place Column.");
    case EArchSimTutorialState::PromptPlaceK2:
        return LOCTEXT("PlaceK2Prompt",
            "Place a K2 Beam. Click a spot, then press Place Beam.");
    case EArchSimTutorialState::PromptPlaceK4:
        return LOCTEXT("PlaceK4Prompt",
            "Place a K4 Brace. Click a spot, then press Place Brace.");
    case EArchSimTutorialState::PromptPressTest:
        return LOCTEXT("PressTestPrompt",
            "Press Test Structure to run the structural analysis.");
    case EArchSimTutorialState::FreeExplore:
        return LOCTEXT("FreeExplorePrompt",
            "Great work! Explore freely. Add more elements and keep testing.");
    default:
        return LOCTEXT("UnknownPrompt", "Continue exploring.");
    }
}
#undef LOCTEXT_NAMESPACE

// Linear state transition table (terminal: FreeExplore stays FreeExplore).
static EArchSimTutorialState NextTutorialState(EArchSimTutorialState Current)
{
    switch (Current)
    {
    case EArchSimTutorialState::Welcome:         return EArchSimTutorialState::PromptPlaceK1;
    case EArchSimTutorialState::PromptPlaceK1:   return EArchSimTutorialState::PromptPlaceK2;
    case EArchSimTutorialState::PromptPlaceK2:   return EArchSimTutorialState::PromptPlaceK4;
    case EArchSimTutorialState::PromptPlaceK4:   return EArchSimTutorialState::PromptPressTest;
    case EArchSimTutorialState::PromptPressTest: return EArchSimTutorialState::FreeExplore;
    case EArchSimTutorialState::FreeExplore:     return EArchSimTutorialState::FreeExplore; // terminal
    default:                                     return EArchSimTutorialState::FreeExplore;
    }
}

void UArchSimScenarioWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // u1: No extra init needed.
    // u2: delegate subscription deferred to first RequestSolveAndVisualize() call so we
    // don't subscribe before PIE is started (Registry would be null at widget open time).
}

void UArchSimScenarioWidget::BeginDestroy()
{
    // Unsubscribe from Registry->OnSolveComplete before the widget is GC'd.
    // WHY here and not in NativeDestruct: BeginDestroy fires even when NativeDestruct is not
    // called (e.g. widget in a transient outer that gets collected without explicit cleanup).
    //
    // WHY explicit Remove (not just Reset):
    //   Per Phase 3 reviewer SPIKE-Scenario-u2 Finding #1, only Reset()-ing the local handle
    //   does NOT unsubscribe from the Registry-side multicast — that requires calling
    //   Registry->OnSolveComplete.Remove(Handle). Without that call, AddUObject's implicit
    //   weak-object protection is implementation-detail not API contract, and we could
    //   theoretically be invoked on a dead object before GC propagates.
    //   We use SubscribedRegistry (TWeakObjectPtr) cached during RequestSolveAndVisualize so
    //   we can safely-null-check if PIE / Registry already torn down between subscribe and
    //   this BeginDestroy.
    if (SolveCompleteDelegateHandle.IsValid())
    {
        if (UArchSimModelRegistry* Registry = SubscribedRegistry.Get())
        {
            Registry->OnSolveComplete.Remove(SolveCompleteDelegateHandle);
        }
        SolveCompleteDelegateHandle.Reset();
        SubscribedRegistry.Reset();
    }

    Super::BeginDestroy();
}

// ---------------------------------------------------------------------------
// Shared K-set placement helper (u3 refactor)
// ---------------------------------------------------------------------------

AActor* UArchSimScenarioWidget::PlaceKSetMember(
    FVector LocationWorld,
    FVector EndIOffsetUE,
    FVector EndJOffsetUE,
    const TCHAR* MemberTag)
{
    // -- Step 1: Obtain the world to spawn into ----------------------------------
    // v0.4.0.1 fix (AS-28): MUST prefer GEditor->PlayWorld when PIE is active,
    // mirroring RequestSolveAndVisualize at line ~445. The prior implementation
    // unconditionally used GEditor->GetEditorWorldContext().World() — that always
    // returns the editor world even during PIE (UE5.7 behaviour; the prior comment
    // claiming "During PIE this is the PIE world" was incorrect). Spawning into
    // the editor world while the Registry lives in the PIE GameInstance produced
    // a silent cross-world bug: actors placed, but Registry never received them,
    // so Solver saw "invalid model: no nodes" and HeatmapActor never spawned.
    if (!GEditor)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceKSetMember[%s] — GEditor is null; "
                    "cannot place K-set outside of Editor context."), MemberTag);
        return nullptr;
    }

    UWorld* World = GEditor->PlayWorld.Get()
                    ? GEditor->PlayWorld.Get()
                    : GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceKSetMember[%s] — Editor World is null; "
                    "open a level before placing K-set."), MemberTag);
        return nullptr;
    }

    // -- Step 2: Spawn a plain AActor as the K-set placeholder -------------------
    // No mesh asset is required at u3 scope; the Blueprint child class can assign a
    // StaticMeshComponent asset in the Details panel (AS-05 art asset backlog).
    //
    // AS-36 fix (S-08): base AActor has no default RootComponent. UE known behaviour:
    // SpawnActor<AActor>(AActor::StaticClass(), FTransform(loc,...)) SILENTLY DROPS the
    // Location when no RootComponent exists — GetActorTransform() always returns Identity.
    // This was first discovered in S-01 v0.1.1 (ArchSimSaveLoadTest.cpp L120-134 comment
    // "SpawnActor(...,Location,...) on base AActor drops the location silently because
    // AActor has no default RootComponent -- so we built our own."). PlaceKSetMember was
    // written in S-05 and repeated the same pit.
    //
    // Consequence for SpawnDefaultPortalFrame: ColA and ColB are spawned at different
    // world positions ((-100,0,100) vs (+100,0,100)), but both return Identity transform
    // → RegisterMember computes the SAME world endpoints for both columns → FindOrAddNode
    // deduplicates to the same node pair (e.g. Node 2/3) → LDLT rank-deficient
    // → bSingular → OnSolveComplete singular guard fires → HeatmapActor never spawns.
    // Commandlet PIE SC2b confirmed: "Member[0] I=2 J=3 / Member[1] I=2 J=3".
    //
    // Fix: spawn deferred (AlwaysSpawn at Identity), then graft a USceneComponent root
    // and call SetActorLocation(LocationWorld). This is the identical S-01 pattern from
    // ArchSimSaveLoadTest.cpp L128-134. Affects all K-set placement paths (K1/K2/K4 and
    // any calls via SpawnDefaultPortalFrame), which is the intended scope of this fix.
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* PlacedActor = World->SpawnActor<AActor>(
        AActor::StaticClass(),
        FTransform::Identity,    // AS-36: spawn at Identity first; SetActorLocation below
        SpawnParams);

    if (!PlacedActor)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceKSetMember[%s] — SpawnActor failed "
                    "at location (%.1f, %.1f, %.1f)."),
               MemberTag, LocationWorld.X, LocationWorld.Y, LocationWorld.Z);
        return nullptr;
    }

    // AS-36: Graft a USceneComponent as RootComponent so SetActorLocation is honoured.
    // WHY NewObject with outer=PlacedActor: component lifetime is tied to the actor.
    // WHY RegisterComponent before SetRootComponent: UE requires the component to be
    // registered before it can be set as RootComponent (otherwise
    // SetActorLocation() silently becomes a no-op on the unregistered component).
    // WHY named "Root": stable name avoids anonymous-component-collision in the Details panel.
    {
        USceneComponent* Root = NewObject<USceneComponent>(
            PlacedActor, USceneComponent::StaticClass(), TEXT("Root"));
        check(Root);  // NewObject failure here is catastrophic; always check
        Root->RegisterComponent();
        PlacedActor->SetRootComponent(Root);
        PlacedActor->SetActorLocation(LocationWorld);

        UE_LOG(LogArchSim, Verbose,
               TEXT("UArchSimScenarioWidget::PlaceKSetMember[%s] — "
                    "AS-36: USceneComponent root attached; SetActorLocation (%.1f,%.1f,%.1f) applied."),
               MemberTag, LocationWorld.X, LocationWorld.Y, LocationWorld.Z);
    }

    // -- Step 3: Attach UArchSimMemberData with caller-supplied offsets ----------
    // WHY NewObject with outer=PlacedActor: the component's lifetime is tied to the
    // actor. GC will collect the component when the actor is destroyed.
    UArchSimMemberData* MemberComp =
        NewObject<UArchSimMemberData>(PlacedActor,
                                      UArchSimMemberData::StaticClass(),
                                      TEXT("ArchSimMemberData"));
    if (!MemberComp)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceKSetMember[%s] — NewObject<UArchSimMemberData> "
                    "failed; actor will be spawned without registry link."), MemberTag);
        return PlacedActor; // Still return the actor — non-fatal for skeleton
    }

    // Apply per-archetype geometry offsets (overwrite UArchSimMemberData ctor defaults).
    MemberComp->EndIOffsetUE = EndIOffsetUE;
    MemberComp->EndJOffsetUE = EndJOffsetUE;

    // RegisterComponent wires the component into the actor's lifecycle (BeginPlay,
    // Tick, EndPlay). AddInstanceComponent makes it visible in the Details panel
    // and serializable via SaveGame. Order matters: Register first, then Add.
    MemberComp->RegisterComponent();
    PlacedActor->AddInstanceComponent(MemberComp);

    // -- Step 4: Register with UArchSimModelRegistry if available ----------------
    // Registry is a GameInstanceSubsystem — null in pure edit-mode without PIE.
    // PlaceKSetMember is still useful (spawns + attaches component) even without Registry.
    // BeginPlay on MemberData will also attempt auto-registration, so the defer is safe.
    UArchSimModelRegistry* Registry = UArchSimModelRegistry::Get(World);
    if (Registry)
    {
        int32 AssignedIdx = Registry->RegisterMember(MemberComp);
        if (AssignedIdx < 0)
        {
            UE_LOG(LogArchSim, Warning,
                   TEXT("UArchSimScenarioWidget::PlaceKSetMember[%s] — RegisterMember returned -1 "
                        "(validation failure: check actor origin / member axis). "
                        "Actor spawned but not registered."), MemberTag);
        }
        else
        {
            UE_LOG(LogArchSim, Display,
                   TEXT("UArchSimScenarioWidget::PlaceKSetMember[%s] — registered at "
                        "MemberIdx=%d, loc=(%.1f,%.1f,%.1f)."),
                   MemberTag, AssignedIdx, LocationWorld.X, LocationWorld.Y, LocationWorld.Z);
        }
    }
    else
    {
        // WHY this is expected in edit-mode: UArchSimModelRegistry is a
        // UGameInstanceSubsystem which only exists during PIE / packaged builds.
        // In pure edit-mode (no PIE) Get(World) returns nullptr. BeginPlay on
        // UArchSimMemberData will attempt registration when PIE starts.
        UE_LOG(LogArchSim, Display,
               TEXT("UArchSimScenarioWidget::PlaceKSetMember[%s] — Registry not available "
                    "(edit-mode, no PIE). Actor placed; registration deferred to BeginPlay."),
               MemberTag);
    }

    // -- Step 5: Track actor in PlacedActors list --------------------------------
    // WHY append: widget holds a soft ownership list so ResetWidgetState() can clear it
    // without searching the world. GC keeps entries valid as TObjectPtr<AActor>.
    PlacedActors.Add(PlacedActor);

    return PlacedActor;
}

// ---------------------------------------------------------------------------
// K1 column (u1, refactored to delegate to PlaceKSetMember)
// ---------------------------------------------------------------------------

AActor* UArchSimScenarioWidget::PlaceK1Column(FVector LocationWorld)
{
    // Default K1 geometry: 1 m horizontal beam along +X.
    // EndI(-50,0,0) cm / EndJ(+50,0,0) cm = 1 m (100 cm) total length.
    // WHY 1 m: ZPD Level 1 baseline element; matches UArchSimMemberData ctor defaults.
    return PlaceKSetMember(LocationWorld,
                           FVector(-50.f, 0.f, 0.f),
                           FVector(+50.f, 0.f, 0.f),
                           TEXT("K1"));
}

// ---------------------------------------------------------------------------
// K2 horizontal beam (u3 new)
// ---------------------------------------------------------------------------

AActor* UArchSimScenarioWidget::PlaceK2Beam(FVector LocationWorld)
{
    // K2 geometry: 2 m horizontal beam along +X.
    // EndI(-100,0,0) cm / EndJ(+100,0,0) cm = 2 m (200 cm) total length.
    // WHY 2 m: visually distinct from K1; teaches students a longer span deflects more.
    return PlaceKSetMember(LocationWorld,
                           FVector(-100.f, 0.f, 0.f),
                           FVector(+100.f, 0.f, 0.f),
                           TEXT("K2"));
}

// ---------------------------------------------------------------------------
// K4 diagonal brace (u3 new)
// ---------------------------------------------------------------------------

AActor* UArchSimScenarioWidget::PlaceK4Brace(FVector LocationWorld)
{
    // K4 geometry: 2 m diagonal brace at 45° in the XZ plane.
    // EndI(-71,0,-71) cm / EndJ(+71,0,+71) cm.
    // Full member length = 2 * sqrt(71^2 + 71^2) = 2 * 71 * sqrt(2) ≈ 200.8 cm ≈ 2 m.
    // WHY XZ plane (not XY): Z is vertical in UE; a brace rising in XZ teaches students
    // the classic cross-brace portal that resists lateral sway — visually intuitive.
    // WHY 71 (not 70.71): integer cm approximation of 100/sqrt(2); introduces <0.1% error.
    return PlaceKSetMember(LocationWorld,
                           FVector(-71.f, 0.f, -71.f),
                           FVector(+71.f, 0.f, +71.f),
                           TEXT("K4"));
}

// ---------------------------------------------------------------------------
// AS-30: boundary support + default portal frame fixture
// ---------------------------------------------------------------------------

int32 UArchSimScenarioWidget::PlaceFixedSupport(FVector LocationWorld)
{
    // -- Acquire world (PIE-preferred, matching v0.4.0.1 cross-world fix) --------
    // WHY PlayWorld first: UArchSimModelRegistry is a GameInstanceSubsystem; it only
    // exists when a live GameInstance is running. GEditor->GetEditorWorldContext().World()
    // returns the Editor world which has no GI → Registry null.
    if (!GEditor)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceFixedSupport — GEditor is null; "
                    "cannot place support outside Editor context."));
        return -1;
    }

    UWorld* World = GEditor->PlayWorld.Get()
                    ? GEditor->PlayWorld.Get()
                    : GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceFixedSupport — World is null; "
                    "open a level before placing supports."));
        return -1;
    }

    // -- Acquire Registry (null without live GameInstance / PIE) -----------------
    UArchSimModelRegistry* Registry = UArchSimModelRegistry::Get(World);
    if (!Registry)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::PlaceFixedSupport — Registry not available "
                    "(no PIE). Enter PIE first."));
        return -1;
    }

    // -- Convert world cm → FrameCore mm -----------------------------------------
    // WHY ×10: 1 UE unit = 1 cm; FrameCore engine stores positions in mm.
    // This mirrors ArchSimModelRegistry.cpp kCmToMm=10 (used in RegisterMember).
    const FVector PosMm = LocationWorld * 10.0;

    // -- Delegate to Registry::RegisterFixedSupport (single dedup authority) ------
    // WHY not reimplement FindOrAddNode here: Registry owns node dedup; the widget
    // is the BP-facing surface. Keeping the dedup in Registry ensures a column placed
    // at (-100,0,0) and a support at (-100,0,0) share the same node automatically.
    const int32 NodeIdx = Registry->RegisterFixedSupport(PosMm);

    if (NodeIdx >= 0)
    {
        UE_LOG(LogArchSim, Display,
               TEXT("UArchSimScenarioWidget::PlaceFixedSupport — NodeIdx=%d at "
                    "world (%.1f,%.1f,%.1f) cm = mm (%.1f,%.1f,%.1f)."),
               NodeIdx,
               LocationWorld.X, LocationWorld.Y, LocationWorld.Z,
               PosMm.X, PosMm.Y, PosMm.Z);
    }

    return NodeIdx;
}

bool UArchSimScenarioWidget::SpawnDefaultPortalFrame()
{
    // -- Step 1: verify we can reach the Registry (bail early before any spawn) --
    // WHY early check before spawning: partial spawns (actors in world but model
    // incomplete) are harder to diagnose than a clean early-exit log.
    if (!GEditor)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::SpawnDefaultPortalFrame — GEditor null; "
                    "cannot spawn outside Editor context."));
        return false;
    }

    UWorld* World = GEditor->PlayWorld.Get()
                    ? GEditor->PlayWorld.Get()
                    : GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::SpawnDefaultPortalFrame — World null; "
                    "open a level before calling SpawnDefaultPortalFrame."));
        return false;
    }

    UArchSimModelRegistry* Registry = UArchSimModelRegistry::Get(World);
    if (!Registry)
    {
        // Expected in transient widget / headless test: Registry is a
        // GameInstanceSubsystem — null without a live PIE GameInstance.
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::SpawnDefaultPortalFrame — Registry null "
                    "(no PIE). Enter PIE first, then call SpawnDefaultPortalFrame."));
        return false;
    }

    // -- Step 2: place 2 fully-fixed base supports --------------------------------
    // WHY fixed at both bases: portal frame with fixed-fixed base + rigid joints
    // at top is 3x statically indeterminate (3D frame with axial + bending
    // constraints); reduced K matrix is well-conditioned for LDLT and definitely
    // not a mechanism.
    // DOF accounting for the 3-member, 4-node system:
    //   4 nodes × 6 DOF = 24 total nodal DOF.
    //   2 fixed nodes × 6 = 12 removed; leaves 12 free DOF on the 2 top corner
    //   nodes (6 each). Reduced K matrix dimension is therefore 12 × 12.
    //   (Disassembled member-level count is 3 members × 12 end-DOF = 36, but
    //   shared corner nodes collapse it to the 12-free assembly above.)
    // WHY (-100,0,0) and (+100,0,0): 2 m base width matches the 2 m column height
    // for a square portal frame. Easy for students to recognise as a building frame.
    const int32 SupportA = PlaceFixedSupport(FVector(-100.f, 0.f, 0.f));
    const int32 SupportB = PlaceFixedSupport(FVector(+100.f, 0.f, 0.f));

    if (SupportA < 0 || SupportB < 0)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::SpawnDefaultPortalFrame — "
                    "fixed support registration failed (A=%d B=%d); aborting."),
               SupportA, SupportB);
        return false;
    }

    // -- Step 3: place 2 K1 columns ----------------------------------------------
    // Column A: (-100,0,0) to (-100,0,200) cm = 2 m vertical left column.
    // Column B: (+100,0,0) to (+100,0,200) cm = 2 m vertical right column.
    // WHY PlaceKSetMember with explicit offsets: columns are vertical (+Z in UE),
    // but K1 default offsets are horizontal (±50 cm X). We override with vertical
    // offsets so the column rises from base to top-corner.
    // WHY actor origin at base: the offset pair (0,0,-100)/(0,0,+100) centres the
    // actor at mid-column (same-origin convenience), but we spawn the actor AT
    // the base and use offsets from there → EndI=(0,0,0) EndJ=(0,0,200) in world.
    // We use PlaceKSetMember with LocationWorld at the base and offset from 0→200 Z.
    // EndIOffsetUE=(0,0,0) would produce zero-length axis rejection in RegisterMember,
    // so we use a symmetric pair around the column midpoint:
    //   EndI=(0,0,-100) cm EndJ=(0,0,+100) cm centred at column mid-height (0,0,100).
    // The actor is spawned at the column midpoint; base and top resolve to
    //   base  = (±100,0,0) + (0,0,-100)  = (±100,0,-100)+actor_loc=(±100,0,100-100)
    //         = (±100,0,0) ← matches support node via FindOrAddNode 1 mm tolerance ✓
    //   top   = (±100,0,0) + (0,0,+100)  = (±100,0,200) ← beam connection node ✓
    AActor* ColA = PlaceKSetMember(
        FVector(-100.f, 0.f, 100.f),   // actor origin at column mid-height
        FVector(  0.f,  0.f, -100.f),  // EndI offset → world (-100,0,0) = base support A
        FVector(  0.f,  0.f, +100.f),  // EndJ offset → world (-100,0,200) = top-left corner
        TEXT("K1-ColA"));

    AActor* ColB = PlaceKSetMember(
        FVector(+100.f, 0.f, 100.f),   // actor origin at column mid-height
        FVector(  0.f,  0.f, -100.f),  // EndI offset → world (+100,0,0) = base support B
        FVector(  0.f,  0.f, +100.f),  // EndJ offset → world (+100,0,200) = top-right corner
        TEXT("K1-ColB"));

    if (!ColA || !ColB)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::SpawnDefaultPortalFrame — "
                    "column spawn failed (ColA=%s ColB=%s); aborting."),
               ColA ? TEXT("ok") : TEXT("null"),
               ColB ? TEXT("ok") : TEXT("null"));
        return false;
    }

    // -- Step 4: place 1 K2 beam -------------------------------------------------
    // Beam: (-100,0,200) to (+100,0,200) cm = 2 m horizontal top beam.
    // Actor origin at beam midpoint (0,0,200) cm; EndI=(-100,0,0) EndJ=(+100,0,0).
    // WHY K2 offsets ±100 X: K2 default is already ±100 X for a 2 m span; the top
    // nodes snap to the column tops (-100,0,200) and (+100,0,200) via FindOrAddNode.
    AActor* Beam = PlaceKSetMember(
        FVector(  0.f, 0.f, 200.f),    // actor origin at beam midpoint / top of frame
        FVector(-100.f, 0.f, 0.f),     // EndI offset → world (-100,0,200) = top-left corner
        FVector(+100.f, 0.f, 0.f),     // EndJ offset → world (+100,0,200) = top-right corner
        TEXT("K2-Beam"));

    if (!Beam)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::SpawnDefaultPortalFrame — "
                    "beam spawn failed; frame is incomplete."));
        return false;
    }

    UE_LOG(LogArchSim, Display,
           TEXT("UArchSimScenarioWidget::SpawnDefaultPortalFrame — "
                "portal frame spawned: SupportA=%d SupportB=%d ColA=%s ColB=%s Beam=%s. "
                "4 nodes (2 fixed base + 2 top corners), 3 members. "
                "Call RequestSolveAndVisualize() to solve and view heatmap."),
           SupportA, SupportB,
           *ColA->GetName(), *ColB->GetName(), *Beam->GetName());

    return true;
}

// ---------------------------------------------------------------------------
// Tutorial state machine (u3)
// ---------------------------------------------------------------------------

void UArchSimScenarioWidget::AdvanceTutorialStep()
{
    // FreeExplore is terminal — no-op to prevent wrapping or surprising the student.
    if (TutorialState == EArchSimTutorialState::FreeExplore)
    {
        return;
    }

    TutorialState = NextTutorialState(TutorialState);
    const FText Prompt = GetPromptForState(TutorialState);

    // Fire the BP overlay event — BP child class implements this to update the UMG widget.
    // WHY non-modal: per PhET implicit scaffolding principle, the overlay must be
    // dismissible / non-blocking at any time. BP child decides when to show it.
    OnTutorialStateChanged(TutorialState, Prompt);

    // Fire the voice TTS hook — text-only; no C++ SDK invocation. BP decides whether
    // to speak, display, or ignore. toString() of FText for the voice string.
    OnVoicePromptShouldPlay(Prompt.ToString());

    UE_LOG(LogArchSim, Display,
           TEXT("UArchSimScenarioWidget::AdvanceTutorialStep — state → %d, prompt: \"%s\""),
           static_cast<int32>(TutorialState), *Prompt.ToString());
}

FText UArchSimScenarioWidget::GetCurrentPromptText() const
{
    // Delegate to the file-scope helper so the switch stays in one place.
    return GetPromptForState(TutorialState);
}

// ---------------------------------------------------------------------------
// Reload / reset smoke (u3)
// ---------------------------------------------------------------------------

void UArchSimScenarioWidget::ResetWidgetState()
{
    UE_LOG(LogArchSim, Display,
           TEXT("UArchSimScenarioWidget::ResetWidgetState — resetting widget state."));

    // -- 1. Unsubscribe OnSolveComplete delegate (same path as BeginDestroy) -----
    // WHY do this here instead of relying on BeginDestroy: the student may reopen the
    // widget without GC-ing it (EditorUtilityWidgets are persistent until Editor close).
    // If we don't unsubscribe before re-subscribing on the new PIE session, we'd get
    // double callbacks on the same widget instance.
    if (SolveCompleteDelegateHandle.IsValid())
    {
        if (UArchSimModelRegistry* Registry = SubscribedRegistry.Get())
        {
            Registry->OnSolveComplete.Remove(SolveCompleteDelegateHandle);
        }
        SolveCompleteDelegateHandle.Reset();
        SubscribedRegistry.Reset();
    }

    // -- 2. Destroy the heatmap actor if it is still valid in the PIE world ------
    // WHY IsValid(): the PIE world may have been torn down already, in which case
    // the actor is pending kill and DestroyActor is already a no-op.
    if (IsValid(HeatmapActor))
    {
        HeatmapActor->Destroy();
        HeatmapActor = nullptr;
    }

    // -- 3. Reset tutorial state to Welcome ---------------------------------------
    // After reset the student sees the welcome prompt again. BP overlay should update
    // via the explicit OnTutorialStateChanged call below.
    TutorialState = EArchSimTutorialState::Welcome;
    const FText WelcomePrompt = GetPromptForState(EArchSimTutorialState::Welcome);
    OnTutorialStateChanged(EArchSimTutorialState::Welcome, WelcomePrompt);
    OnVoicePromptShouldPlay(WelcomePrompt.ToString());

    // -- 4. Clear PlacedActors list (soft ref list only; actors stay in world) ---
    // WHY NOT destroy: the student's placed K-set actors are their design work — silently
    // destroying them would be surprising and irreversible. The widget's job is to track
    // them for reference counting, not to own their lifetime. The Registry remains the
    // authoritative source of truth about registered members.
    PlacedActors.Empty();

    UE_LOG(LogArchSim, Display,
           TEXT("UArchSimScenarioWidget::ResetWidgetState — done. "
                "TutorialState=Welcome, HeatmapActor=null, PlacedActors cleared."));
}

bool UArchSimScenarioWidget::RequestSolveAndVisualize()
{
    // -- Step 1: Acquire the best available world ----------------------------------
    // Prefer GEditor->PlayWorld (the live PIE game world) because UArchSimModelRegistry
    // is a UGameInstanceSubsystem — it only exists when a GameInstance is live.
    // GEditor->GetEditorWorldContext().World() is the Editor (non-PIE) world; its
    // GameInstance is null → Registry::Get returns nullptr there.
    if (!GEditor)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::RequestSolveAndVisualize — GEditor null; "
                    "not in Editor context."));
        return false;
    }

    // WHY .Get(): GEditor->PlayWorld is TObjectPtr<UWorld>; GetEditorWorldContext().World()
    // returns UWorld*. The ternary operator cannot implicitly unify these two types in MSVC,
    // so we explicitly extract the raw pointer from TObjectPtr via .Get().
    UWorld* World = GEditor->PlayWorld.Get()
                    ? GEditor->PlayWorld.Get()
                    : GEditor->GetEditorWorldContext().World();

    // -- Step 2: Acquire Registry (null without live PIE GameInstance) ------------
    UArchSimModelRegistry* Registry = UArchSimModelRegistry::Get(World);
    if (!Registry)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::RequestSolveAndVisualize — "
                    "UArchSimModelRegistry is null. Enter PIE first, then press "
                    "Test Structure."));
        return false;
    }

    // -- Step 3: Subscribe OnSolveComplete (idempotent) ---------------------------
    // WHY idempotent guard: the designer may press "Test Structure" multiple times per
    // PIE session. We bind only once; the same delegate handle fires on every solve.
    // If the handle is already valid we skip re-subscription.
    // WHY also store SubscribedRegistry: BeginDestroy needs Registry pointer to call
    // .Remove(Handle) for proper unsubscribe (Reset alone is not enough — see
    // BeginDestroy comment + Phase 3 reviewer SPIKE-Scenario-u2 Finding #1).
    if (!SolveCompleteDelegateHandle.IsValid())
    {
        SolveCompleteDelegateHandle = Registry->OnSolveComplete.AddUObject(
            this, &UArchSimScenarioWidget::OnSolveComplete);
        SubscribedRegistry = Registry;
    }

    // -- Step 4: Request a solve (150 ms debounce in Registry) --------------------
    // An empty FFrameModelPatch{} signals "no structural change, just re-solve the
    // current model". The debounce timer in UArchSimModelRegistry coalesces multiple
    // rapid calls. The result arrives asynchronously via the OnSolveComplete delegate.
    Registry->RequestSolve(FFrameModelPatch{});

    UE_LOG(LogArchSim, Display,
           TEXT("UArchSimScenarioWidget::RequestSolveAndVisualize — "
                "Solve requested; awaiting OnSolveComplete delegate."));
    return true;
}

void UArchSimScenarioWidget::OnSolveComplete(const FFrameSolveResult& Result)
{
    // Guard: if the solve produced a singular (degenerate) system, heatmap would be
    // meaningless — skip spawn and log a diagnostic for the student.
    if (Result.bSingular)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::OnSolveComplete — Solve is singular "
                    "(mechanism or insufficient constraints). "
                    "Add supports before testing the structure."));
        return;
    }

    // Find the PIE world to spawn / reuse the heatmap actor.
    // WHY PlayWorld here (not EditorWorldContext): the heatmap actor must live in the same
    // world as the placed K1 actors so it appears in the PIE viewport. BeginPlay on the
    // heatmap actor requires a live game world; Editor world BeginPlay is not called.
    UWorld* PIEWorld = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    if (!PIEWorld)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::OnSolveComplete — PIE world is gone; "
                    "discarding solve result."));
        return;
    }

    // Acquire Registry again (defensive; could be torn down between request and callback).
    UArchSimModelRegistry* Registry = UArchSimModelRegistry::Get(PIEWorld);
    if (!Registry)
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("UArchSimScenarioWidget::OnSolveComplete — Registry gone before "
                    "heatmap update; skipping."));
        return;
    }

    // -- Lazy spawn the heatmap actor (one per widget lifetime) -------------------
    // WHY lazy: SpawnActor costs; we only need one actor regardless of solve count.
    // WHY IsValid() check: if PIE restarted the old actor pointer is stale (GC'd).
    if (!IsValid(HeatmapActor))
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        HeatmapActor = PIEWorld->SpawnActor<AFrameUtilizationHeatmapActor>(
            AFrameUtilizationHeatmapActor::StaticClass(),
            FTransform::Identity,
            Params);

        if (!HeatmapActor)
        {
            UE_LOG(LogArchSim, Error,
                   TEXT("UArchSimScenarioWidget::OnSolveComplete — Failed to spawn "
                        "AFrameUtilizationHeatmapActor in PIE world."));
            return;
        }

        UE_LOG(LogArchSim, Display,
               TEXT("UArchSimScenarioWidget::OnSolveComplete — HeatmapActor spawned in PIE world."));
    }

    // -- Populate the heatmap actor -----------------------------------------------
    HeatmapActor->Solution       = Result;
    HeatmapActor->MemberGeometry = BuildMemberGeometryFromRegistry(Registry);
    // ShellGeometry stays empty at u2 scope (no shells placed via K1 widget).

    // Rebuild the procedural mesh. Returns false if zero members/shells.
    const bool bBuilt = HeatmapActor->BuildHeatmap();
    UE_LOG(LogArchSim, Display,
           TEXT("UArchSimScenarioWidget::OnSolveComplete — BuildHeatmap returned %s; "
                "%d member geometries provided."),
           bBuilt ? TEXT("true") : TEXT("false"),
           HeatmapActor->MemberGeometry.Num());
}

// File-scope helper (not a class member). WHY not a class member:
// exposing TArray<FFrameMemberGeometry> in the header declaration forces all TUs that
// include ArchSimScenarioWidget.h (including UHT-generated unity TUs) to have
// FFrameMemberGeometry visible, which requires including FrameCoreUEVisualTypes.h in every
// such TU. Keeping this as a file-scope static avoids that header-creep path and
// is safe because the function is only called from OnSolveComplete in this TU.
static TArray<FFrameMemberGeometry> BuildMemberGeometryFromRegistry(
    UArchSimModelRegistry* Registry)
{
    TArray<FFrameMemberGeometry> Out;
    if (!Registry)
    {
        return Out;
    }

    const FFrameModelDef& Model = Registry->GetCurrentModel();
    const int32 MemberCount     = Model.Members.Num();
    if (MemberCount == 0)
    {
        return Out;
    }

    Out.Reserve(MemberCount);

    for (int32 i = 0; i < MemberCount; ++i)
    {
        const FFrameMember& Mem = Model.Members[i];
        if (!Mem.bActive)
        {
            // Skip deactivated members — heatmap actor won't have utilization data for them.
            continue;
        }

        // Resolve node world positions (FrameCore stores in mm; FFrameNode::Pos is in mm).
        // Convert mm → UE cm: multiply by 0.1.
        // WHY 0.1: FrameCore mm * 0.1 = UE cm (1 UE unit = 1 cm; 1 mm = 0.1 cm).
        const bool bValidI = Model.Nodes.IsValidIndex(Mem.I);
        const bool bValidJ = Model.Nodes.IsValidIndex(Mem.J);
        if (!bValidI || !bValidJ)
        {
            // Node index out of range — model may be partially rebuilt; skip.
            UE_LOG(LogArchSim, Warning,
                   TEXT("BuildMemberGeometryFromRegistry — member[%d] node index out of range "
                        "(I=%d J=%d NodeCount=%d); skipping."),
                   i, Mem.I, Mem.J, Model.Nodes.Num());
            continue;
        }

        const FVector StartMm = Model.Nodes[Mem.I].Pos;
        const FVector EndMm   = Model.Nodes[Mem.J].Pos;
        constexpr float kMmToCm = 0.1f;

        FFrameMemberGeometry Geom;
        Geom.MemberIdx    = i;
        Geom.Start        = StartMm * kMmToCm;
        Geom.End          = EndMm   * kMmToCm;
        // Width / Depth: use section data if available; fall back to a 10 cm default so the
        // heatmap renders something even without section metadata.
        // WHY 10 cm default: matches FFrameMemberGeometry member defaults (Width=10, Depth=10).
        Geom.Width        = 10.f;
        Geom.Depth        = 10.f;
        if (Model.Sections.IsValidIndex(Mem.SecIdx))
        {
            // Cy / Cz are half-breadth in local y / z respectively (mm). Convert to cm.
            // These are "neutral axis to extreme fiber" distances, so full dimension = 2 * C.
            // For a rectangular section Cy = b/2, Cz = d/2.
            const FFrameSection& Sec = Model.Sections[Mem.SecIdx];
            if (Sec.Cy > 0.f) { Geom.Width = Sec.Cy * 2.f * kMmToCm; }
            if (Sec.Cz > 0.f) { Geom.Depth = Sec.Cz * 2.f * kMmToCm; }
        }
        // Node index mapping for potential future deformed-shape renderers.
        Geom.EndINodeIdx  = Mem.I;
        Geom.EndJNodeIdx  = Mem.J;

        Out.Add(Geom);
    }

    return Out;
}

#endif // WITH_EDITOR
