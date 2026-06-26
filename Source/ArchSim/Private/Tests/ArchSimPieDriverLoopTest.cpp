// ArchSim - PIE test for AS-02 driver-loop branch.
// Sprint S-03 unit AS-13-u2. Depends on harness shipped in AS-13-u1
// (Source/ArchSim/Private/Tests/ArchSimPieHarness.{h,cpp}).
//
// What this test PINS (production-coverage advancement vs ArchSimGameInstanceTest.cpp):
//   - GetOrFindWorld() is non-null in commandlet (harness world contract verified).
//   - GetOrFindGameInstance() is null in Level 3 commandlet (Level 3 confirmed).
//   - World->Tick(LEVELTICK_All, dt) x3 does not crash in the driver-loop context.
//   - After World ticks, GetSolveTriggerCount on a NewObject<UArchSimGameInstance>
//     stays 0 (Tick registered as FTickableGameObject does NOT fire for a detached
//     NewObject GI — FTickableGameObject::RegisterTickableObject is invoked at ctor,
//     but the game-thread tick manager only dispatches tickables that IsTickable()).
//   - After World ticks, GetOrCreateModelRegistry() from harness stays at accum=0
//     (no driver-loop touched the registry — registry is independent of World tick).
//   - UArchSimGameInstance::IsTickable() returns false for uninit NewObject instance
//     (bIsActive == false gate in ArchSimGameInstance.cpp:123).
//
// What this test CANNOT verify in Level 3 headless (honest defer):
//   - UArchSimGameInstance::Tick body at cpp:60-110 being triggered by the World's
//     tick dispatch — FTickableGameObject only fires for tickables where IsTickable()
//     returns true AND the game thread's tick manager manages their lifetime. A
//     NewObject<UArchSimGameInstance>() without a World owner has bIsActive=false,
//     so IsTickable() returns false and the tick manager skips it.
//   - GetSolveTriggerCount advancing beyond 0 — requires the driver-loop body to
//     reach cpp:91-110, which needs GetSubsystem<UArchSimModelRegistry>() != nullptr,
//     which needs a real GameInstance attached to a World.
//   - LastSeenRegisteredCount advancing beyond -1 — same root cause.
//   Reachable at: Level 1 (real UArchSimGameInstance as World's OwningGameInstance)
//                 OR true PIE editor session with UArchSimGameInstance configured.
//
// Honors AS-07 lesson #1: pin actual production behavior; do NOT fabricate
// reachability of driver-loop just because PIE harness is now available.
//
// Precedent: ArchSimGameInstanceTest.cpp (AS-02c headless analog, same defer pattern).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Tests/ArchSimPieHarness.h"
#include "ArchSimGameInstance.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArchSimPieDriverLoopTest,
    "ArchSim.Integration.PieDriverLoop",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArchSimPieDriverLoopTest::RunTest(const FString& /*Parameters*/)
{
    // ---- Sub-check 1: Harness World is non-null --------------------------------
    // Level 3 commandlet always has at least one world context. A null World here
    // means GEngine is broken — unrecoverable for this test.
    UWorld* World = ArchSimPieHarness::GetOrFindWorld();
    if (!TestNotNull(TEXT("Sub-check 1: GetOrFindWorld() non-null "
                          "(commandlet always provides a world context)"), World))
    {
        AddError(TEXT("GetOrFindWorld() returned null — cannot continue."));
        return false;
    }

    // ---- Sub-check 2: No real GI in Level 3 commandlet -------------------------
    // GetOrFindGameInstance() scans all GEngine contexts for OwningGameInstance.
    // In headless -nullrhi commandlet mode no context has one. We assert null to
    // pin the Level 3 coverage class for this test's honest-defer statements.
    UGameInstance* GI = ArchSimPieHarness::GetOrFindGameInstance();
    TestNull(TEXT("Sub-check 2: GetOrFindGameInstance() == null in Level 3 "
                  "(headless: no OwningGameInstance in any GEngine WorldContext; "
                  "driver-loop Tick dispatch requires real GI — honest defer)"),
             GI);

    // ---- Sub-check 3: NewObject<UArchSimGameInstance> IsTickable() is false ----
    // A freshly created UArchSimGameInstance has bIsActive=false (Init() not called).
    // IsTickable() at ArchSimGameInstance.cpp:123 evaluates `bIsActive && ...` —
    // the first operand is false so the whole expression short-circuits to false.
    // This means even after World ticks, the tick manager will skip this GI.
    UArchSimGameInstance* TestGI = NewObject<UArchSimGameInstance>();
    if (!TestNotNull(TEXT("Sub-check 3 precondition: NewObject<UArchSimGameInstance> non-null"),
                     TestGI))
    {
        return false;
    }
    TestFalse(TEXT("Sub-check 3: uninit NewObject GI IsTickable() == false "
                   "(bIsActive=false gate at ArchSimGameInstance.cpp:123; "
                   "FTickableGameObject tick manager skips this instance on World->Tick)"),
              TestGI->IsTickable());
    // Baseline telemetry before any World ticks.
    TestEqual(TEXT("Sub-check 3a: initial SolveTriggerCount == 0"),
              TestGI->GetSolveTriggerCount(), 0);
    TestEqual(TEXT("Sub-check 3b: initial LastSeenRegisteredCount == -1"),
              TestGI->GetLastSeenRegisteredCount(), -1);

    // ---- Sub-check 4: World->Tick() x3 does not crash --------------------------
    // Three World ticks at 60fps delta. This validates the harness world is in a
    // tickable state. It does NOT fire UArchSimGameInstance::Tick because the
    // FTickableGameObject dispatch only reaches objects where IsTickable()==true,
    // and TestGI has bIsActive=false.
    //
    // Note: ArchSimPieSmokeTest.cpp already verified World->Tick() does not crash;
    // this test re-verifies it specifically in the driver-loop context to document
    // that driver-loop absence is about IsTickable(), not about World crash.
    const float kFrameDt = 1.0f / 60.0f;
    World->Tick(ELevelTick::LEVELTICK_All, kFrameDt);
    World->Tick(ELevelTick::LEVELTICK_All, kFrameDt);
    World->Tick(ELevelTick::LEVELTICK_All, kFrameDt);
    TestTrue(TEXT("Sub-check 4: World->Tick(LEVELTICK_All, 1/60) x3 did not crash "
                  "(world is tickable; driver-loop absence is due to IsTickable()=false, "
                  "not a world deficiency)"),
             true);

    // ---- Sub-check 5: SolveTriggerCount unchanged after World ticks -------------
    // The tick manager dispatches all FTickableGameObject instances that IsTickable()
    // returns true for. Since TestGI->IsTickable()==false, UArchSimGameInstance::Tick
    // is NOT called, and SolveTriggerCount must remain 0.
    //
    // HONEST DEFER (AS-07 lesson #1):
    //   We do NOT assert SolveTriggerCount > 0. The driver-loop body (cpp:91-110)
    //   is unreachable in Level 3. Fabricating this would hide the real limitation.
    TestEqual(TEXT("Sub-check 5a: SolveTriggerCount still 0 after 3 World ticks "
                   "(IsTickable()=false prevents UArchSimGameInstance::Tick dispatch; "
                   "driver-loop body at cpp:91-110 unreachable in Level 3). "
                   "HONEST DEFER: driver-loop fires only with attached UArchSimGameInstance "
                   "in Level 1; AS-13-u2 Level 3 cannot exercise."),
              TestGI->GetSolveTriggerCount(), 0);
    TestEqual(TEXT("Sub-check 5b: LastSeenRegisteredCount still -1 after 3 World ticks "
                   "(same root cause: Tick body at cpp:60 not reached)"),
              TestGI->GetLastSeenRegisteredCount(), -1);

    // ---- Sub-check 6: Registry from harness is unaffected by World ticks -------
    // The registry obtained from the harness (NewObject fallback in Level 3) is
    // completely independent from any World tick state. Its accumulator must still
    // be 0 after World->Tick x3 — confirming no spurious RequestSolve was called.
    UArchSimModelRegistry* HarnessReg = ArchSimPieHarness::GetOrCreateModelRegistry();
    if (!TestNotNull(TEXT("Sub-check 6 precondition: harness registry non-null"),
                     HarnessReg))
    {
        return false;
    }
    TestEqual(TEXT("Sub-check 6: harness registry accumulator == 0 after World ticks "
                   "(no driver-loop → no RequestSolve fired → registry unmodified)"),
              HarnessReg->GetPendingRankAccumulation(), 0);

    // ---- Sub-check 7: TickCount is the manual-tick observable (not World-tick) --
    // Calling TestGI->Tick() DIRECTLY still increments TickCount (cpp:61), because
    // the telemetry increment runs before the GI-null registry guard at cpp:82-89.
    // This pins the same manual-tick invariant established in ArchSimGameInstanceTest.cpp
    // Sub-check 2, but in the context where we also have a harness World.
    const int32 kManualTicks = 3;
    for (int32 i = 0; i < kManualTicks; ++i)
    {
        TestGI->Tick(kFrameDt);
    }
    TestEqual(TEXT("Sub-check 7a: manual Tick() x3 increments TickCount to 3 "
                   "(telemetry at cpp:61 runs before registry guard; "
                   "confirms harness world presence doesn't interfere with manual tick)"),
              TestGI->GetTickCount(), kManualTicks);
    // SolveTriggerCount must still be 0: the registry guard at cpp:82-89 exits early
    // because GetSubsystem<UArchSimModelRegistry>() returns null for a NewObject GI.
    TestEqual(TEXT("Sub-check 7b: SolveTriggerCount still 0 after manual Tick() x3 "
                   "(registry null guard at cpp:82-89 fires before driver body at cpp:91)"),
              TestGI->GetSolveTriggerCount(), 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
