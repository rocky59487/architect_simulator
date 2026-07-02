// ArchSimPieHarness.h
// PIE-world bootstrap helpers for ArchSim integration tests.
//
// Origin: AS-13 backlog (docs/logs/S-02/manager.md, AS-13 row)
//   "PIE-world fixture for driver-loop observability + AS-10 trip-path + AS-03d input runtime"
//
// Design rationale — WHY NOT UWorld::CreateWorld:
//   FrameCoreUEActorStressMeshTest.cpp:35-37 (same repo) documents that
//   UWorld::CreateWorld(EWorldType::Game) CRASHES under -ExecCmds=Automation
//   commandlet mode because the editor GameInstance template is not loaded.
//   This harness uses the proven GEngine->GetWorldContexts() scan instead,
//   bit-identical to the precedent in FrameCoreUEActorStressMeshTest.cpp:35-51.
//
// Design rationale — WHY NOT FAutomationEditorCommonUtils::CreateNewMap:
//   Requires Editor module linkage and a live editor frame; unavailable in
//   headless -nullrhi -unattended commandlet runs used by the 5-leg gate.
//
// Honest 3-level coverage contract:
//   Level 1 — real GI is UArchSimGameInstance:
//     GetSubsystem<UArchSimModelRegistry>() works AND the driver-loop
//     (ArchSimGameInstance Tick → GetSubsystem → RequestSolve) is reachable.
//     Production-coverage: FULL.
//   Level 2 — real GI exists but is not UArchSimGameInstance:
//     GetSubsystem<UArchSimModelRegistry>() still works (it is a
//     UGameInstanceSubsystem). Driver-loop unreachable (wrong GI type).
//     Production-coverage: REGISTRY ONLY.
//   Level 3 — no OwningGameInstance in any context (headless commandlet):
//     NewObject<UArchSimModelRegistry>() fallback. Subsystem API available
//     but neither the driver-loop nor PIE BeginPlay are reachable.
//     Production-coverage: UNIT-LEVEL ONLY (honest defer, same as AS-02c/AS-10).
//
// Thread safety: helpers are read-only accessors of GEngine state; call only
// from the game thread (automation tests always run on game thread).
//
// Precedent files:
//   FrameCoreUEActorStressMeshTest.cpp:35-51  — World scan pattern
//   FrameCoreUEInteractiveSubsystemTest.cpp    — Subsystem + NewObject fallback

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Forward declarations — avoid dragging full headers into every test TU.
class UArchSimModelRegistry;
class AArchSimCharacter;

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
class FAutomationTestBase;  // forward-decl; caller passes Test* for AddInfo/AddWarning
#endif

namespace ArchSimPieHarness
{
    // Returns the first valid UWorld from GEngine's contexts.
    // In -ExecCmds=Automation commandlet mode the engine consistently has at least one
    // world context (the editor preview world) in our verified test runs. Returns nullptr
    // ONLY if GEngine itself is null, which indicates a genuine engine init failure.
    // Pattern: FrameCoreUEActorStressMeshTest.cpp:35-51 (proven, non-crashing).
    UWorld* GetOrFindWorld();

    // Returns the UGameInstance of any context that has an OwningGameInstance.
    // In headless commandlet mode no context has one → returns nullptr.
    // Callers should NOT assume the returned GI is UArchSimGameInstance; check
    // Cast<UArchSimGameInstance>(GetOrFindGameInstance()) explicitly if needed.
    UGameInstance* GetOrFindGameInstance();

    // Returns the UArchSimModelRegistry reachable from a real GameInstance if
    // available (Level 1 or 2), else falls back to NewObject<UArchSimModelRegistry>()
    // (Level 3). The returned object is ALWAYS non-null.
    // IMPORTANT: Do NOT assume the registry is connected to a live GameInstance.
    // Call IsRegistryFromRealGI() to distinguish production coverage level.
    UArchSimModelRegistry* GetOrCreateModelRegistry();

    // Returns true iff the last call to GetOrCreateModelRegistry() obtained its
    // registry via a real OwningGameInstance (Level 1 or 2). Returns false when
    // the NewObject fallback was used (Level 3).
    // This function is stateless in that it re-queries GEngine each time; it does
    // NOT cache the last GetOrCreateModelRegistry() result.
    bool IsRegistryFromRealGI();

    // Spawns an Actor of type T into the given World.
    // Returns nullptr if World is null or if the World's spawn system refuses
    // (e.g. special commandlet worlds that disallow deferred-construction spawns).
    // Caller is responsible for cleanup (DestroyActor or rely on World teardown).
    template <typename T>
    T* SpawnActor(UWorld* World)
    {
        if (!World) { return nullptr; }
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<T>(T::StaticClass(), FTransform::Identity, Params);
    }

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    // OverrideGameModeForSafePIE — sidestep the ALS commandlet-PIE crash.
    //
    // WHY this helper exists (AS-37-u1 crash chain):
    //   GlobalDefaultGameMode=ArchSimGameMode spawns AArchSimCharacter (ALS subclass)
    //   on PIE start. In UnrealEditor-Cmd commandlet mode, AssetRegistry finishes
    //   mounting `/ALS/` plugin content AFTER the pawn-spawn timing window, so all
    //   four LoadObject<T>() calls in AArchSimCharacter::LoadAlsAssetsLate() fail
    //   (CDO and spawn rounds). ALS then fires without null-guarding:
    //     AlsCharacterMovementComponent.cpp:L894 (SetMovementSettings) — FIRST ensure hit
    //     AlsCharacterMovementComponent.cpp:L903 (RefreshGaitSettings)
    //     AlsCharacter.cpp:L526 (NotifyLocomotionModeChanged) — EXCEPTION_ACCESS_VIOLATION
    //   Crash log: Saved/Crashes/UECC-Windows-7ECCED384D44B2CCD56A45B7F390734D_0002
    //   Severity: commandlet-only (cooked/pak packaging not affected;
    //             Dev -game without pak — unverified caveat).
    //
    // WHY AGameModeBase (not nullptr):
    //   WorldSettings.DefaultGameMode = nullptr falls back to GlobalDefaultGameMode
    //   (Config/DefaultEngine.ini). An explicit non-ArchSim class is required.
    //   AGameModeBase is always linked (part of Engine), spawns no Pawn, has no ALS dep.
    //
    // WHY CreateNewMap() is called before the override:
    //   The test runner may start on a map that already has ALS actors placed in it.
    //   A blank map + explicit GameMode override gives a clean slate.
    //
    // FUTURE RULE:
    //   All commandlet PIE tests (including AS-08-u2 SPUD smoke) MUST call this
    //   helper in their RunTest() pre-step, unless the test goal is specifically to
    //   exercise ALS character behaviour.
    //
    // Returns true if the blank map + override succeeded; false otherwise.
    // On failure the caller should AddError and return false from RunTest().
    // Test* is accepted for AddInfo/AddWarning diagnostics (may be nullptr, guarded).
    bool OverrideGameModeForSafePIE(FAutomationTestBase* Test = nullptr);
#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

} // namespace ArchSimPieHarness
