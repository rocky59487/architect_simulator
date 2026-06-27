// ArchSim — UArchSimScenarioWidget : Editor Utility Widget for K1 column placement + solve wire.
// Sprint S-05, SPIKE-Scenario-u1/u2. Plan ref: docs/logs/S-05/plan_*.md § SPIKE-Scenario.
//
// Design intent (Constructionism / PhET principle):
//   The "Design Phase" first verb is BUILD. A designer/student opens this widget in the UE
//   Editor and calls PlaceK1Column(WorldLocation) to plant a K1 column placeholder Actor.
//   The Actor automatically gains a UArchSimMemberData component and is registered in the
//   UArchSimModelRegistry GameInstanceSubsystem. u2 adds RequestSolveAndVisualize() which
//   triggers a debounced solve and renders D/C results via AFrameUtilizationHeatmapActor.
//
// Architecture decision: Option A (WITH_EDITOR guard in runtime module)
//   Files live under Source/ArchSim/{Public,Private}/Editor/ and are wrapped in
//   #if WITH_EDITOR so no symbol is compiled into packaged (non-Editor) builds.
//   If this guard proves insufficient for a separate Editor module (Option B), see ESCALATE
//   comment in ArchSimScenarioWidget.cpp.
//
// Dependencies added to ArchSim.Build.cs (final form after build-time iteration):
//   if (Target.Type == TargetType.Editor) { Blutility, UMG, UMGEditor, UnrealEd }
// EditorScriptingUtilities was removed during u1 dispatch: it requires a corresponding
// `.uproject` Plugins entry which iron rule #5 forbids touching, and PlaceK1Column does
// not actually use any UEditorActorUtilities API (just GEditor + SpawnActor). UnrealEd
// was added for GEditor + GetEditorWorldContext (LNK2019 fix during build iteration).
//
// Honest scope boundary:
//   u1: PlaceK1Column operates in a World obtained from a live PIE/Editor session. The
//       headless smoke test verifies CDO/reflection only.
//   u2: RequestSolveAndVisualize() requires PIE active (UArchSimModelRegistry only lives
//       in a GameInstanceSubsystem). Graceful-fail (return false + log) when no PIE.
//       Full PIE solve→delegate→heatmap chain deferred to u3 PIE fixture.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "EditorUtilityWidget.h"
#include "ArchSimScenarioWidget.generated.h"

class UArchSimMemberData;
class UArchSimModelRegistry;
class AFrameUtilizationHeatmapActor;
class AActor;

/**
 * Editor Utility Widget that allows a designer or student to place K1 column placeholder
 * Actors in the Editor world and trigger a structural solve to visualize D/C utilization.
 *
 * Usage:
 *   1. Open via Window > Editor Utility Widgets in the UE Editor (after creating a BP child).
 *   2. Call PlaceK1Column with a world-space location. A placeholder AActor spawns, gains a
 *      UArchSimMemberData component with default 1 m beam geometry, and is registered.
 *   3. Call RequestSolveAndVisualize() while PIE is active. This triggers the debounced
 *      solve pipeline and spawns/updates an AFrameUtilizationHeatmapActor in the PIE world.
 *
 * K1 = single-column archetype (Level 1, ZPD baseline). K2/K4 placement lives in u3.
 */
// WHY no MinimalAPI: ARCHSIM_API (full export) and MinimalAPI are mutually exclusive in UE5.
// MinimalAPI is for classes in header-only form that should not export a full vtable. Here we
// need ARCHSIM_API for the DLL export of methods so BP callers can find them.
UCLASS(Abstract, DisplayName="ArchSim Scenario Widget")
class ARCHSIM_API UArchSimScenarioWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()

public:
    /**
     * Spawn a K1 column placeholder at the given world-space location, attach
     * UArchSimMemberData with default 1 m horizontal beam geometry (+X), and
     * attempt to register it with the UArchSimModelRegistry.
     *
     * @param LocationWorld  World-space spawn origin (UE cm).
     * @return The spawned AActor, or nullptr on failure (no valid World / Editor context).
     *
     * Note: Registry registration succeeds only when a GameInstance is live (PIE / in-Editor
     * WorldSettings active). In headless test environments this call may return a non-null
     * AActor but skip registration (Registry is null). The smoke test verifies CDO reflection
     * only; end-to-end registration is covered by the u3 PIE fixture.
     */
    UFUNCTION(BlueprintCallable, Category="ArchSim|Design")
    AActor* PlaceK1Column(FVector LocationWorld);

    /**
     * u2: Trigger a structural solve on the current model and visualize D/C results.
     *
     * Requires PIE to be active (UArchSimModelRegistry is a GameInstanceSubsystem — null
     * without a live GameInstance). Graceful-fails (return false + UE_LOG warning) when
     * no PIE world is available.
     *
     * Flow:
     *   1. Acquire PIE world (GEditor->PlayWorld if available, else Editor world).
     *   2. Get UArchSimModelRegistry::Get(World). Return false if null (no PIE).
     *   3. Subscribe OnSolveComplete delegate (once; idempotent on repeat calls).
     *   4. Call Registry->RequestSolve(empty patch) to trigger the 150 ms debounce.
     *   5. OnSolveComplete fires → spawn/reuse AFrameUtilizationHeatmapActor in PIE world
     *      → populate Solution + MemberGeometry → call BuildHeatmap().
     *
     * @return true if the solve was dispatched; false if Registry unavailable (no PIE).
     *
     * Full PIE solve→delegate→heatmap chain is [DEFERRED to u3 PIE fixture].
     */
    UFUNCTION(BlueprintCallable, Category="ArchSim|Solve")
    bool RequestSolveAndVisualize();

    // Lazy-spawned heatmap actor in the PIE world. Widget owns the reference; destroyed when
    // the widget is garbage-collected or when the PIE world tears down.
    // WHY TObjectPtr: UObject UPROPERTY → TObjectPtr<T> per UE5 iron rule.
    // WHY WITH_EDITORONLY_DATA wrapper: UHT in UE5.4+ requires UPROPERTY members nested
    // inside a WITH_EDITOR class scope to use WITH_EDITORONLY_DATA (not WITH_EDITOR) per
    // the "UProperties should not be wrapped by WITH_EDITOR" UHT enforcement rule. The two
    // macros are semantically equivalent in Editor targets where this class exists, but UHT
    // uses the more precise WITH_EDITORONLY_DATA sentinel to denote "this field doesn't
    // exist in cooked / packaged targets". BlueprintReadOnly is retained because
    // EditorUtilityWidget BP graphs run only in Editor builds where WITH_EDITORONLY_DATA
    // is always defined.
#if WITH_EDITORONLY_DATA
    UPROPERTY(BlueprintReadOnly, Category="ArchSim|Solve")
    TObjectPtr<AFrameUtilizationHeatmapActor> HeatmapActor = nullptr;
#endif // WITH_EDITORONLY_DATA

protected:
    // UEditorUtilityWidget override — wired to BP "Run" entry if bAutoRunDefaultAction.
    virtual void NativeOnInitialized() override;

    // UObject override — unsubscribe Registry delegate to avoid dangling handle after widget GC.
    virtual void BeginDestroy() override;

private:
    // Handle for the Registry->OnSolveComplete subscription. Valid once RequestSolveAndVisualize
    // successfully subscribes; Removed + Reset on BeginDestroy. WHY FDelegateHandle: multicast
    // delegates require a handle to be able to remove a specific subscriber later.
    FDelegateHandle SolveCompleteDelegateHandle;

    // Weak reference to the Registry we successfully subscribed to. WHY needed:
    // BeginDestroy must explicitly call Registry->OnSolveComplete.Remove(Handle) — Reset()ing
    // a local FDelegateHandle alone does NOT unsubscribe from the Registry-side multicast
    // (per Phase 3 reviewer S-05 SPIKE-Scenario-u2 Finding #1). Stored as TWeakObjectPtr so
    // we don't keep Registry alive past PIE-end and we get a safe-null on Registry teardown.
    // Non-UPROPERTY because TWeakObjectPtr is weak by design and does not need GC tracking.
    TWeakObjectPtr<UArchSimModelRegistry> SubscribedRegistry;

    // BuildMemberGeometryFromRegistry is intentionally NOT declared here.
    // It is a file-scope static helper in ArchSimScenarioWidget.cpp, declared there with
    // full FFrameMemberGeometry visibility. WHY: exposing FFrameMemberGeometry in the header
    // would force all TUs that include this header (including UHT gen.cpp unity TU) to
    // transitively include FrameCoreUEVisualTypes.h, causing ODR/compile issues in the
    // non-Editor unity build path. The helper is called only from OnSolveComplete (same TU).

    // Callback bound to Registry->OnSolveComplete. Spawns / reuses HeatmapActor in the
    // PIE world, populates Solution + MemberGeometry, and calls BuildHeatmap().
    void OnSolveComplete(const struct FFrameSolveResult& Result);
};

#endif // WITH_EDITOR
