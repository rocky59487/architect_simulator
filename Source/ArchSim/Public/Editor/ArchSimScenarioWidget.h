// ArchSim — UArchSimScenarioWidget : Editor Utility Widget for K-set placement + solve wire.
// Sprint S-05, SPIKE-Scenario-u1/u2/u3. Plan ref: docs/logs/S-05/plan_*.md § SPIKE-Scenario.
//
// Design intent (Constructionism / PhET / Kapur Productive Failure):
//   The "Design Phase" first verb is BUILD. A designer/student opens this widget in the UE
//   Editor and calls PlaceK1Column/PlaceK2Beam/PlaceK4Brace(WorldLocation) to plant K-set
//   placeholder Actors. Each Actor automatically gains a UArchSimMemberData component and is
//   registered in the UArchSimModelRegistry GameInstanceSubsystem. u2 adds
//   RequestSolveAndVisualize() which triggers a debounced solve and renders D/C results via
//   AFrameUtilizationHeatmapActor. u3 adds the Tutorial overlay state machine (non-modal,
//   PhET implicit scaffolding — the student is never blocked; Skip is always available).
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
//   u3: K2/K4 placement (shared helper refactor). Tutorial state machine (UENUM + non-modal
//       overlay event). Voice TTS hook (BlueprintImplementableEvent text-only; no 3rd-party
//       SDK). ResetWidgetState for widget reopen/reload smoke.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "EditorUtilityWidget.h"
#include "ArchSimScenarioWidget.generated.h"

class UArchSimMemberData;
class UArchSimModelRegistry;
class AFrameUtilizationHeatmapActor;
class AActor;

// ---------------------------------------------------------------------------
// Tutorial state machine (u3, Kapur Try-phase scaffolding)
// WHY UENUM: BP graph reads / switches on TutorialState without C++ coupling.
// WHY 6 states: maps 1-to-1 onto ZPD Level 1 K-set sequence + terminal FreeExplore.
// State transitions are linear (Welcome → PromptPlaceK1 → ... → FreeExplore).
// FreeExplore is terminal — AdvanceTutorialStep is a no-op once there.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EArchSimTutorialState : uint8
{
    Welcome          UMETA(DisplayName="Welcome"),           // Widget just opened
    PromptPlaceK1    UMETA(DisplayName="Prompt: Place K1"),  // Ask student to place a column
    PromptPlaceK2    UMETA(DisplayName="Prompt: Place K2"),  // Ask student to place a beam
    PromptPlaceK4    UMETA(DisplayName="Prompt: Place K4"),  // Ask student to place a brace
    PromptPressTest  UMETA(DisplayName="Prompt: Press Test"),// Ask student to press Test Structure
    FreeExplore      UMETA(DisplayName="Free Explore"),      // Tutorial done; student free
};

/**
 * Editor Utility Widget that allows a designer or student to place K-set placeholder
 * Actors in the Editor world and trigger a structural solve to visualize D/C utilization.
 *
 * Usage:
 *   1. Open via Window > Editor Utility Widgets in the UE Editor (after creating a BP child).
 *   2. Call PlaceK1Column / PlaceK2Beam / PlaceK4Brace with a world-space location.
 *      A placeholder AActor spawns, gains a UArchSimMemberData component, and is registered.
 *   3. Call RequestSolveAndVisualize() while PIE is active. This triggers the debounced
 *      solve pipeline and spawns/updates an AFrameUtilizationHeatmapActor in the PIE world.
 *   4. Use AdvanceTutorialStep() to walk the student through the Kapur Try-phase sequence.
 *      OnTutorialStateChanged fires with the new state + prompt text for the BP overlay UI.
 *
 * K1 = column (1 m +X).  K2 = horizontal beam (2 m +X).  K4 = diagonal brace (2 m 45° XZ).
 */
// WHY no MinimalAPI: ARCHSIM_API (full export) and MinimalAPI are mutually exclusive in UE5.
// MinimalAPI is for classes in header-only form that should not export a full vtable. Here we
// need ARCHSIM_API for the DLL export of methods so BP callers can find them.
UCLASS(Abstract, DisplayName="ArchSim Scenario Widget")
class ARCHSIM_API UArchSimScenarioWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()

public:
    // ===========================================================================
    // K-set placement (u1: K1 — existing; u3: K2, K4 — new)
    // ===========================================================================

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
     * Spawn a K2 horizontal beam placeholder at the given world-space location.
     * Default geometry: 2 m along +X (EndIOffsetUE=(-100,0,0), EndJOffsetUE=(+100,0,0) cm).
     *
     * WHY 2 m: ZPD Level 1 K-set; K2 is the next structural element after K1 column;
     * 2 m gives students a visually distinct span vs the 1 m K1.
     *
     * @param LocationWorld  World-space spawn origin (UE cm).
     * @return The spawned AActor, or nullptr on failure.
     */
    UFUNCTION(BlueprintCallable, Category="ArchSim|Design")
    AActor* PlaceK2Beam(FVector LocationWorld);

    /**
     * Spawn a K4 diagonal brace placeholder at the given world-space location.
     * Default geometry: 2 m at 45° in XZ plane
     *   EndIOffsetUE=(-71,0,-71) cm, EndJOffsetUE=(+71,0,+71) cm.
     *   Full member length ≈ 2 × sqrt(71² + 71²) cm ≈ 200.8 cm ≈ 2 m.
     *
     * WHY 45° XZ: diagonal brace teaches students about triangulation for lateral load
     * resistance; XZ plane (not XY) shows the brace rising in the vertical plane,
     * matching intuitive "cross-brace" of a frame portal.
     *
     * @param LocationWorld  World-space spawn origin (UE cm).
     * @return The spawned AActor, or nullptr on failure.
     */
    UFUNCTION(BlueprintCallable, Category="ArchSim|Design")
    AActor* PlaceK4Brace(FVector LocationWorld);

    // ===========================================================================
    // Solve + visualize (u2 — unchanged signature)
    // ===========================================================================

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
     * Full PIE solve→delegate→heatmap chain is covered by u3 PIE smoke (user-driven).
     */
    UFUNCTION(BlueprintCallable, Category="ArchSim|Solve")
    bool RequestSolveAndVisualize();

    // ===========================================================================
    // Tutorial state machine (u3, non-modal Kapur Try-phase scaffolding)
    // ===========================================================================

    /**
     * Current tutorial state. BP overlay graph reads this to update overlay text / icon.
     * Starts at Welcome; advances via AdvanceTutorialStep(); FreeExplore is terminal.
     *
     * WHY BlueprintReadOnly (not BlueprintReadWrite): only the C++ state machine may
     * advance the enum — direct BP assignment would bypass OnTutorialStateChanged.
     *
     * WHY WITH_EDITORONLY_DATA: UHT 5.4+ requires UPROPERTY members in a WITH_EDITOR-guarded
     * class body to use WITH_EDITORONLY_DATA (not WITH_EDITOR). This is the same rule that
     * applies to HeatmapActor and PlacedActors. The class itself only exists in Editor builds;
     * WITH_EDITORONLY_DATA is the more precise UHT sentinel for "field absent in cooked builds."
     */
#if WITH_EDITORONLY_DATA
    UPROPERTY(BlueprintReadOnly, Category="ArchSim|Tutorial")
    EArchSimTutorialState TutorialState = EArchSimTutorialState::Welcome;
#endif // WITH_EDITORONLY_DATA

    /**
     * Advance the tutorial one step forward. Fires OnTutorialStateChanged with the new
     * state and the associated prompt text. No-op when already FreeExplore (terminal).
     *
     * State machine:
     *   Welcome → PromptPlaceK1 → PromptPlaceK2 → PromptPlaceK4
     *   → PromptPressTest → FreeExplore (stays here forever)
     *
     * WHY the BP "Skip Tutorial" button calls this in a loop or jumps to FreeExplore:
     *   The student is NEVER blocked (PhET implicit scaffolding). Skipping = calling
     *   AdvanceTutorialStep repeatedly until FreeExplore, or setting TutorialState in C++
     *   via a future SkipTutorial() helper (out of u3 scope).
     */
    UFUNCTION(BlueprintCallable, Category="ArchSim|Tutorial")
    void AdvanceTutorialStep();

    /**
     * Return the FText prompt for the current TutorialState. Used by BP to populate a
     * TextBlock without needing a switch node in the graph. Always returns non-empty FText
     * (FreeExplore returns a generic encouragement string).
     *
     * WHY BlueprintPure: no side effects; BP can call it freely in construction script or
     * event graph without a Sequence node.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ArchSim|Tutorial")
    FText GetCurrentPromptText() const;

    /**
     * Event fired whenever TutorialState changes. BP child class overrides this to show
     * the overlay widget panel and update text / animation.
     *
     * @param NewState    The new tutorial state (same as TutorialState after advance).
     * @param PromptText  The FText prompt for this state (same as GetCurrentPromptText()).
     *
     * WHY BlueprintImplementableEvent (not BlueprintNativeEvent): no default C++ behavior
     * is needed — UMG overlay management is entirely in BP. The C++ side only drives the
     * state machine; BP reacts to it. This is the PhET "non-modal overlay" design contract.
     */
    UFUNCTION(BlueprintImplementableEvent, Category="ArchSim|Tutorial")
    void OnTutorialStateChanged(EArchSimTutorialState NewState, const FText& PromptText);

    /**
     * Voice TTS hook: fired whenever a new tutorial prompt is shown. The BP child class
     * overrides this to play a voice line via any BP-callable TTS node, or ignore it.
     * C++ emits only the plain text — no 3rd-party SDK is linked or invoked in C++ code.
     *
     * WHY BlueprintImplementableEvent: text-only in C++ is sufficient (game-designer spec).
     * BP can call any BP-callable TTS node available in the project, or simply ignore the
     * event. Keeping SDK integration in BP means no 3rd-party dependency in the C++ build.
     *
     * @param PromptText  Plain English (or localized) prompt string to speak aloud.
     */
    UFUNCTION(BlueprintImplementableEvent, Category="ArchSim|Tutorial")
    void OnVoicePromptShouldPlay(const FString& PromptText);

    /**
     * Reset the widget state as if it were freshly opened. Safe to call on widget reopen
     * or after a PIE restart (reload smoke).
     *
     * What Reset does:
     *   1. Unsubscribe OnSolveComplete delegate (same as BeginDestroy path, idempotent).
     *   2. Destroy HeatmapActor if it is still valid in the PIE world.
     *   3. Reset TutorialState → Welcome.
     *   4. Clear PlacedActors list (does NOT destroy the placed K-set actors — they live
     *      in the PIE world and the student may want to keep them). WHY: destroying is
     *      irreversible and unexpected to the student; only the widget's reference list
     *      is cleared so the registry is the authoritative source of truth.
     *
     * WHY this is a UFUNCTION: BP "Close and Reopen" button calls it directly from the
     * widget graph so the student can reset the tutorial without restarting the editor.
     */
    UFUNCTION(BlueprintCallable, Category="ArchSim|Tutorial")
    void ResetWidgetState();

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

    // Tracks every K-set Actor this widget has spawned (K1/K2/K4). WHY TObjectPtr in
    // TArray: UE5 GC correctly tracks TObjectPtr<AActor>; if PIE tears down, entries
    // become null and we guard with IsValid() before use. Widget holds a soft ownership
    // reference — destroying these actors is the student's (or PIE world's) responsibility.
    UPROPERTY(BlueprintReadOnly, Category="ArchSim|Design")
    TArray<TObjectPtr<AActor>> PlacedActors;
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

    // Shared K-set placement helper. Spawns an AActor at LocationWorld, attaches
    // UArchSimMemberData with the supplied endpoint offsets (UE cm), and registers
    // with UArchSimModelRegistry if available.
    // WHY private helper: K1/K2/K4 differ only in EndI/EndJ offsets; sharing avoids
    // ~120 LOC of code duplication across three UFUNCTIONs.
    AActor* PlaceKSetMember(FVector LocationWorld,
                            FVector EndIOffsetUE,
                            FVector EndJOffsetUE,
                            const TCHAR* MemberTag);

    // Callback bound to Registry->OnSolveComplete. Spawns / reuses HeatmapActor in the
    // PIE world, populates Solution + MemberGeometry, and calls BuildHeatmap().
    void OnSolveComplete(const struct FFrameSolveResult& Result);
};

#endif // WITH_EDITOR
