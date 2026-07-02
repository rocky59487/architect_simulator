// ArchSimPieHarness.cpp
// Implementation of PIE-world bootstrap helpers.
// See ArchSimPieHarness.h for design rationale and coverage-level contract.

#include "Tests/ArchSimPieHarness.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Subsystems/ArchSimModelRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"         // FAutomationTestBase
#include "Tests/AutomationEditorCommon.h" // FAutomationEditorCommonUtils::CreateNewMap
#include "GameFramework/WorldSettings.h"  // AWorldSettings
#include "GameFramework/GameModeBase.h"   // AGameModeBase
#endif

// ---------------------------------------------------------------------------
// GetOrFindWorld
// ---------------------------------------------------------------------------
// Scan GEngine's world contexts and return the first one that has a valid
// World pointer. In -ExecCmds=Automation commandlet mode the engine always
// provides at least one context (the editor "preview" world).
// Precedent: FrameCoreUEActorStressMeshTest.cpp:35-51.
// NOT using UWorld::CreateWorld — see header rationale.
// ---------------------------------------------------------------------------
UWorld* ArchSimPieHarness::GetOrFindWorld()
{
    if (!GEngine) { return nullptr; }

    for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
    {
        if (Ctx.World()) { return Ctx.World(); }
    }

    // No valid world found — unexpected in any normal UE run. Return nullptr so
    // callers can emit a TestNotNull diagnostic rather than crashing.
    return nullptr;
}

// ---------------------------------------------------------------------------
// GetOrFindGameInstance
// ---------------------------------------------------------------------------
// Walk contexts looking for one that has an OwningGameInstance. In headless
// commandlet mode all contexts have OwningGameInstance == nullptr because no
// game has been launched. Returns nullptr honestly (Level 3 path).
// ---------------------------------------------------------------------------
UGameInstance* ArchSimPieHarness::GetOrFindGameInstance()
{
    if (!GEngine) { return nullptr; }

    for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
    {
        if (Ctx.OwningGameInstance) { return Ctx.OwningGameInstance; }
    }

    return nullptr;  // headless: no real GI available (Level 3, honest defer)
}

// ---------------------------------------------------------------------------
// GetOrCreateModelRegistry
// ---------------------------------------------------------------------------
// Level 1/2: obtain via GetSubsystem<UArchSimModelRegistry>() from a real GI.
// Level 3:   fallback NewObject<UArchSimModelRegistry>() — same pattern as
//            FrameCoreUEInteractiveSubsystemTest.cpp (NewObject<UFrameInteractiveSubsystem>).
// Always returns a non-null pointer.
// ---------------------------------------------------------------------------
UArchSimModelRegistry* ArchSimPieHarness::GetOrCreateModelRegistry()
{
    if (GEngine)
    {
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            if (Ctx.OwningGameInstance)
            {
                if (UArchSimModelRegistry* Reg =
                        Ctx.OwningGameInstance->GetSubsystem<UArchSimModelRegistry>())
                {
                    return Reg;  // Level 1 or Level 2
                }
            }
        }
    }

    // Level 3 fallback: subsystem API is available even without a real GI.
    // Matches the honest defer pattern established in AS-02c / AS-10.
    //
    // AS-26: GetTransientPackage() outer for ClassWithin(UGameInstance) consistency.
    // UArchSimModelRegistry inherits from UGameInstanceSubsystem which has the
    // explicit UCLASS macro `UCLASS(Abstract, Within = GameInstance, MinimalAPI)`
    // at UE5.7 GameInstanceSubsystem.h:15. Without an explicit outer here the
    // NewObject default outer is already GetTransientPackage() (per UE5.7
    // UObjectGlobals.h:1919 default arg `GetTransientPackageAsObject()`), so this
    // change is technically equivalent to the no-arg call. The value of this fix
    // is intent-documentation + parity with the AS-24 fix at FrameCoreUE 3 test
    // sites (commit 2883d40). See HANDOFF_v0.3.0.md §4 AS-24 first-action.
    return NewObject<UArchSimModelRegistry>(GetTransientPackage());
}

// ---------------------------------------------------------------------------
// IsRegistryFromRealGI
// ---------------------------------------------------------------------------
// Re-queries GEngine each call (no cached state) so it stays accurate across
// successive harness invocations in the same test run.
// ---------------------------------------------------------------------------
bool ArchSimPieHarness::IsRegistryFromRealGI()
{
    if (!GEngine) { return false; }

    for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
    {
        if (Ctx.OwningGameInstance)
        {
            // A subsystem is registered iff the GI's subsystem manager contains it.
            if (Ctx.OwningGameInstance->GetSubsystem<UArchSimModelRegistry>() != nullptr)
            {
                return true;  // Level 1 or Level 2
            }
        }
    }

    return false;  // Level 3: no real GI → NewObject fallback was (or would be) used
}

// ---------------------------------------------------------------------------
// OverrideGameModeForSafePIE
// ---------------------------------------------------------------------------
// Creates a blank Editor map and overrides WorldSettings.DefaultGameMode to
// AGameModeBase so PIE does not spawn AArchSimCharacter (ALS subclass).
//
// WHY this sidestep is necessary — full crash chain:
//   GlobalDefaultGameMode=ArchSimGameMode → AArchSimCharacter (ALS subclass) spawns
//   on PIE start → in UnrealEditor-Cmd commandlet mode AssetRegistry scan for /ALS/
//   content is not complete at pawn-spawn timing → all 4 LoadObject<T>() calls in
//   AArchSimCharacter::LoadAlsAssetsLate() return nullptr (both CDO + spawn rounds)
//   → ALS fires without null-guard:
//       AlsCharacterMovementComponent.cpp:L894  SetMovementSettings   ← first crash point
//       AlsCharacterMovementComponent.cpp:L903  RefreshGaitSettings
//       AlsCharacter.cpp:L526                   NotifyLocomotionModeChanged
//         → EXCEPTION_ACCESS_VIOLATION (reading 0xd8)
//   Crash log: Saved/Crashes/UECC-Windows-7ECCED384D44B2CCD56A45B7F390734D_0002
//   Severity: commandlet-only (cooked/pak packaging not affected;
//             Dev -game without pak — unverified caveat).
//
// WHY AGameModeBase (not nullptr):
//   WorldSettings.DefaultGameMode = nullptr falls back to GlobalDefaultGameMode
//   (Config/DefaultEngine.ini → ArchSimGameMode). Explicit non-ALS class required.
//
// WHY CreateNewMap() first:
//   Test runner may start on a map already containing ALS actors, pre-seeded with
//   the problematic GameMode. Fresh blank map guarantees a clean WorldSettings slate.
//
// FUTURE RULE (documented here and in ArchSimPieHarness.h):
//   All commandlet PIE tests (including AS-08-u2 SPUD smoke) MUST call this helper
//   in their RunTest() pre-step, unless the test goal is specifically ALS character
//   behaviour.
//
// Only compiled under WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR because CreateNewMap
// and FAutomationEditorCommonUtils are editor-only symbols.
// ---------------------------------------------------------------------------
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
bool ArchSimPieHarness::OverrideGameModeForSafePIE(FAutomationTestBase* Test)
{
    UWorld* NewWorld = FAutomationEditorCommonUtils::CreateNewMap();
    if (Test)
    {
        // Exact parity with the pre-helper inline pre-step (v0.5.1..v0.5.2):
        // TestNotNull writes the structured pass/fail check record the automation
        // report expects; AddError below (null path) carries the abort reason.
        // The original inline fired both — keep both. (S-08 AS-37-u2 review NIT #1)
        Test->TestNotNull(TEXT("Pre-step: CreateNewMap() returned a world"), NewWorld);
    }
    if (!NewWorld)
    {
        // No blank world: abort the test. The TestNotNull record above plus this
        // AddError fail the test; caller returns false out of RunTest.
        if (Test)
        {
            Test->AddError(TEXT("OverrideGameModeForSafePIE: CreateNewMap() returned null. "
                                "Cannot set up blank map for PIE. Aborting test."));
        }
        return false;
    }

    AWorldSettings* WS = NewWorld->GetWorldSettings();
    if (WS)
    {
        WS->DefaultGameMode = AGameModeBase::StaticClass();
        if (Test)
        {
            Test->AddInfo(TEXT("OverrideGameModeForSafePIE [VERIFIED]: "
                               "WorldSettings.DefaultGameMode → AGameModeBase. "
                               "PIE will not spawn ALS character, avoiding "
                               "commandlet-mode ALS content crash "
                               "(AlsCharacterMovementComponent.cpp:L894/L903 + AlsCharacter.cpp:L526)."));
        }
    }
    else
    {
        // WorldSettings null on a freshly created map is unexpected but non-fatal:
        // PIE may still hit the ALS crash if ArchSimGameMode is active.
        if (Test)
        {
            Test->AddWarning(TEXT("OverrideGameModeForSafePIE: WorldSettings null on new map. "
                                  "PIE may still spawn ALS character and crash in commandlet mode."));
        }
    }

    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
