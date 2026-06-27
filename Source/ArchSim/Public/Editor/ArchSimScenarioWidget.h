// ArchSim — UArchSimScenarioWidget : Editor Utility Widget skeleton for K1 column placement.
// Sprint S-05, SPIKE-Scenario-u1. Plan ref: docs/logs/S-05/plan_*.md § SPIKE-Scenario-u1.
//
// Design intent (Constructionism / PhET principle):
//   The "Design Phase" first verb is BUILD. A designer/student opens this widget in the UE
//   Editor and calls PlaceK1Column(WorldLocation) to plant a K1 column placeholder Actor.
//   The Actor automatically gains a UArchSimMemberData component and is registered in the
//   UArchSimModelRegistry GameInstanceSubsystem. No K2/K4 placement, no solver wiring, no
//   tutorial overlays — those are u2/u3 scope.
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
// Honest scope boundary: PlaceK1Column operates in a World obtained from a live PIE/Editor
// session. The headless smoke test (ArchSim.Gameplay.ScenarioWidget) verifies CDO/reflection
// only. Full end-to-end placement + Registry.RegisterMember requires a real World — deferred
// to AS-13 / u3 PIE fixture.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "EditorUtilityWidget.h"
#include "ArchSimScenarioWidget.generated.h"

class UArchSimMemberData;
class AActor;

/**
 * Editor Utility Widget that allows a designer or student to place a K1 column placeholder
 * Actor in the Editor world. Attaches UArchSimMemberData and registers via
 * UArchSimModelRegistry::RegisterMember.
 *
 * Usage:
 *   1. Open via Window > Editor Utility Widgets in the UE Editor (after creating a BP child).
 *   2. Call PlaceK1Column with a world-space location. A placeholder AActor spawns, gains a
 *      UArchSimMemberData component with default 1 m beam geometry, and is registered.
 *
 * K1 = single-column archetype (Level 1, ZPD baseline). K2/K4 placement lives in u3.
 */
// WHY no MinimalAPI: ARCHSIM_API (full export) and MinimalAPI are mutually exclusive in UE5.
// MinimalAPI is for classes in header-only form that should not export a full vtable. Here we
// need ARCHSIM_API for the DLL export of PlaceK1Column so BP callers can find the method.
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

protected:
    // UEditorUtilityWidget override — wired to BP "Run" entry if bAutoRunDefaultAction.
    virtual void NativeOnInitialized() override;
};

#endif // WITH_EDITOR
