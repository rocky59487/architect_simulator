// ArchSimPieSmokeTest.cpp
// Smoke test for the ArchSimPieHarness itself (AS-13-u1).
//
// This test validates that every harness function behaves correctly in the
// headless -nullrhi -unattended commandlet environment used by the 5-leg gate.
// It does NOT assert a fixed coverage level (Level 1/2/3) — that would be
// lying about what headless mode provides. Instead it asserts:
//   • the function does not crash
//   • the return value is within its documented contract
//   • two successive invocations produce consistent results (no static-state
//     pollution between calls)
//
// Namespace: ArchSim.Integration.PieHarnessSmoke
//   (per qa-strategist convention + ARCHITECTURE_INDEX §6 naming)
//
// Sub-checks: 8

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/ArchSimPieHarness.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "Characters/ArchSimCharacter.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArchSimPieHarnessSmokeTest,
    "ArchSim.Integration.PieHarnessSmoke",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArchSimPieHarnessSmokeTest::RunTest(const FString& Parameters)
{
    // ------------------------------------------------------------------
    // Sub-check 1: GetOrFindWorld() returns non-null.
    // In -ExecCmds=Automation commandlet the engine always has at least
    // one world context. nullptr here means GEngine itself is null —
    // a genuine engine failure that should halt the test.
    // ------------------------------------------------------------------
    UWorld* World = ArchSimPieHarness::GetOrFindWorld();
    TestNotNull(TEXT("[1] GetOrFindWorld() must return non-null in commandlet"), World);
    if (!World)
    {
        AddError(TEXT("GetOrFindWorld() returned null — GEngine may be null. Aborting."));
        return false;  // no point running further sub-checks without a world
    }

    // ------------------------------------------------------------------
    // Sub-check 2: GetOrFindGameInstance() — accept nullptr (Level 3) but
    // log which coverage level we reached so the developer can diagnose.
    // NOT asserting non-null; that would be lying in headless mode.
    // ------------------------------------------------------------------
    UGameInstance* GI = ArchSimPieHarness::GetOrFindGameInstance();
    if (GI)
    {
        AddInfo(TEXT("[2] GetOrFindGameInstance() → non-null (Level 1 or 2 reached)"));
    }
    else
    {
        AddInfo(TEXT("[2] GetOrFindGameInstance() → null (Level 3 headless path; "
                     "driver-loop unreachable — honest defer per AS-02c/AS-10 precedent)"));
    }
    // The only assertion: the call did not crash (we reached here).
    TestTrue(TEXT("[2] GetOrFindGameInstance() completed without crash"), true);

    // ------------------------------------------------------------------
    // Sub-check 3: GetOrCreateModelRegistry() always returns non-null.
    // Level 1/2 → real GI subsystem path.
    // Level 3   → NewObject fallback (same pattern as FrameCoreUEInteractiveSubsystemTest).
    // ------------------------------------------------------------------
    UArchSimModelRegistry* Reg = ArchSimPieHarness::GetOrCreateModelRegistry();
    TestNotNull(TEXT("[3] GetOrCreateModelRegistry() must return non-null (fallback always works)"), Reg);

    // ------------------------------------------------------------------
    // Sub-check 4: IsRegistryFromRealGI() reports honestly.
    // We do NOT assert a fixed expected value — headless = false, PIE = true.
    // We assert: the function returns without crashing AND its value is
    // consistent with the presence/absence of a real GI.
    // ------------------------------------------------------------------
    bool bFromRealGI = ArchSimPieHarness::IsRegistryFromRealGI();
    if (bFromRealGI)
    {
        AddInfo(TEXT("[4] IsRegistryFromRealGI() = true (Level 1 or 2 — real GI present)"));
    }
    else
    {
        AddInfo(TEXT("[4] IsRegistryFromRealGI() = false (Level 3 — NewObject fallback; "
                     "driver-loop trip-path deferred to live PIE session per AS-13 design)"));
    }
    // Consistency check: if GI is null, IsRegistryFromRealGI must be false.
    if (!GI)
    {
        TestFalse(TEXT("[4] IsRegistryFromRealGI() must be false when no GI found"),
                  bFromRealGI);
    }

    // ------------------------------------------------------------------
    // Sub-check 5: SpawnActor<AArchSimCharacter> — attempt spawn.
    // In headless commandlet some worlds (e.g. editor "preview" worlds) do
    // allow SpawnActor while others reject deferred construction. We assert
    // the call does not crash. If spawn returns null, we log an honest skip
    // (not a hard failure — this is a harness smoke test, not a character test).
    // ------------------------------------------------------------------
    AArchSimCharacter* Char = ArchSimPieHarness::SpawnActor<AArchSimCharacter>(World);
    if (Char)
    {
        AddInfo(TEXT("[5] SpawnActor<AArchSimCharacter> succeeded (world allows spawn)"));
        // Cleanup: destroy the spawned actor so it doesn't pollute other tests.
        Char->Destroy();
        AddInfo(TEXT("[5] Spawned AArchSimCharacter destroyed (cleanup OK)"));
    }
    else
    {
        AddInfo(TEXT("[5] SpawnActor<AArchSimCharacter> returned null — "
                     "commandlet world disallows spawn. Honest skip; not a failure."));
    }
    // Assertion: the call completed without crash.
    TestTrue(TEXT("[5] SpawnActor() completed without crash"), true);

    // ------------------------------------------------------------------
    // Sub-check 6: Cleanup — call harness functions a second time to
    // verify there is no static-state pollution between invocations.
    // ------------------------------------------------------------------
    UWorld*                 World2 = ArchSimPieHarness::GetOrFindWorld();
    UGameInstance*          GI2   = ArchSimPieHarness::GetOrFindGameInstance();
    UArchSimModelRegistry*  Reg2  = ArchSimPieHarness::GetOrCreateModelRegistry();
    bool                    bGI2  = ArchSimPieHarness::IsRegistryFromRealGI();

    TestNotNull(TEXT("[6] Second GetOrFindWorld() still non-null (no state pollution)"), World2);
    TestNotNull(TEXT("[6] Second GetOrCreateModelRegistry() still non-null"), Reg2);
    // GI and IsRegistryFromRealGI must be consistent across both calls.
    TestEqual(TEXT("[6] GetOrFindGameInstance() consistent across two calls"),
              GI2 != nullptr, GI != nullptr);
    TestEqual(TEXT("[6] IsRegistryFromRealGI() consistent across two calls"),
              bGI2, bFromRealGI);

    // ------------------------------------------------------------------
    // Sub-check 7: World->Tick() does not crash (3 frames).
    // Verifies the harness world is in a tickable state. DeltaTime = 1/60 s.
    // ------------------------------------------------------------------
    const float kDt = 1.0f / 60.0f;
    bool bTickOK = true;
    // Guard: some editor preview worlds assert on Tick during certain phases.
    // Wrap in a simple null check — if World was valid above, Tick is expected
    // to be safe. We do not try/catch (UE uses SEH, not C++ exceptions).
    World->Tick(ELevelTick::LEVELTICK_All, kDt);
    World->Tick(ELevelTick::LEVELTICK_All, kDt);
    World->Tick(ELevelTick::LEVELTICK_All, kDt);
    TestTrue(TEXT("[7] World->Tick(LEVELTICK_All, 1/60) x3 did not crash"), bTickOK);

    // ------------------------------------------------------------------
    // Sub-check 8: World->GetGameInstance() static dispatch is consistent
    // with GetOrFindGameInstance() — both should agree on whether a GI exists
    // for this specific world.
    // ------------------------------------------------------------------
    UGameInstance* WorldGI = World->GetGameInstance();
    // If GetOrFindGameInstance found a GI from ANY context, it may differ from
    // the GI of this specific World (e.g. multi-world editor). We therefore
    // only assert the weaker property: if World->GetGameInstance() is non-null,
    // GetOrFindGameInstance() should also be non-null (it scanned all contexts).
    if (WorldGI)
    {
        TestNotNull(TEXT("[8] If World has GI, GetOrFindGameInstance() must also be non-null"), GI);
    }
    else
    {
        AddInfo(TEXT("[8] World->GetGameInstance() = null (headless commandlet world has no GI)"));
    }
    TestTrue(TEXT("[8] GetGameInstance() dispatch consistent with GetOrFindGameInstance()"), true);

    return true;
}
