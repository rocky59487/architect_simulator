// ArchSim — UArchSimScenarioWidget implementation.
// Sprint S-05, SPIKE-Scenario-u1/u2. Plan ref: docs/logs/S-05/plan_*.md § SPIKE-Scenario.
//
// Architecture decision confirmed: Option A (WITH_EDITOR guard in runtime module).
// If a future audit requires Option B (separate ArchSimEditor module), the ESCALATE trigger
// would be: "Source/ArchSim/Public/Editor/ subdir structure conflicts UE convention OR
// WITH_EDITOR guard is insufficient for packaging."
//
// PlaceK1Column flow (u1, unchanged):
//   1. Obtain Editor World via GEditor->GetEditorWorldContext().World().
//   2. SpawnActor<AActor> at LocationWorld.
//   3. NewObject<UArchSimMemberData>; default offsets = 1 m horizontal beam along +X.
//   4. RegisterComponent() + Actor->AddInstanceComponent().
//   5. UArchSimModelRegistry::RegisterMember(Comp) — no-op in edit-mode (Registry null).
//
// RequestSolveAndVisualize flow (u2):
//   1. Prefer PIE world (GEditor->PlayWorld); fall back to Editor world.
//   2. UArchSimModelRegistry::Get(World) — null without live GameInstance → return false.
//   3. Subscribe OnSolveComplete (idempotent; skip if SolveCompleteDelegateHandle.IsValid).
//   4. RequestSolve(empty FFrameModelPatch{}) — fires the 150 ms debounce.
//   5. OnSolveComplete callback: lazy-spawn HeatmapActor → populate → BuildHeatmap().
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

// FrameCoreUEVisualTypes.h included before WITH_EDITOR guard because FFrameMemberGeometry
// is used in BuildMemberGeometryFromRegistry's return type, and the header declaration
// (also outside WITH_EDITOR) needs the complete type to be visible in the same TU.
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"   // FFrameModelPatch (FFrameMemberGeometry is in FrameCoreUETypes.h, pulled in transitively)
#include "FrameCoreUE/FrameCoreUEModelTypes.h"    // FFrameModelDef, FFrameNode, FFrameMember, FFrameSection

#include "Editor/ArchSimScenarioWidget.h"

#if WITH_EDITOR

#include "Components/ArchSimMemberData.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"   // FFrameSolveResult
#include "FrameCoreUE/FrameUtilizationHeatmapActor.h"  // AFrameUtilizationHeatmapActor
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Editor.h"           // GEditor, GEditor->PlayWorld
#include "UObject/UObjectGlobals.h"

// Log category already declared in ArchSimGameInstance.h / .cpp
DECLARE_LOG_CATEGORY_EXTERN(LogArchSim, Log, All);

// Forward-declare the file-scope static helper so OnSolveComplete can call it before
// its definition (C++ requires declaration-before-use for file-scope functions).
// FFrameMemberGeometry and UArchSimModelRegistry are fully visible here (both already
// included above). Definition is below OnSolveComplete (after PlaceK1Column / solve flow).
static TArray<FFrameMemberGeometry> BuildMemberGeometryFromRegistry(UArchSimModelRegistry* Registry);

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
