// ArchSim - Headless smoke test for UArchSimGameInstance lifecycle + Tick telemetry.
// Sprint S-02 AS-02c. Spec: docs/logs/S-02/plan_2026-06-26T1033.md § AS-02c.
//
// What this test CAN verify (in headless -nullrhi -unattended):
//   - Tick body increments TickCount and AccumulatedSeconds correctly
//   - IsTickable() returns false for the CDO (IsTemplate()==true)
//   - IsTickable() returns false when bIsActive==false (post-Shutdown / never Init'd)
//   - GetTickCount / GetAccumulatedTime BP-pure getters observe the same state
//   - GetLastSeenRegisteredCount default == -1 (production-correct initial)
//   - GetSolveTriggerCount default == 0
//
// What this test CANNOT verify in headless (deferred to PIE-world fixture, AS-13?):
//   - GetSubsystem<UArchSimModelRegistry>() returns the live registry — in
//     NewObject<UArchSimGameInstance>() the subsystem pipeline isn't engaged,
//     so the registry lookup returns null and Tick early-bails at
//     ArchSimGameInstance.cpp:83-89 BEFORE updating LastSeen/SolveTrigger.
//   - The empty-patch RequestSolve cascade (Tick -> RequestSolve -> debounce
//     150ms -> ExecuteSolve -> FlushAndStartSession) — same root cause.
//   - The IsTickable()'s GetWorld()!=nullptr branch — NewObject fixture has no
//     World, so bIsActive gate trips first; the World branch is unreachable here.
//
// Honors AS-07 lesson #1 (also re-applied in AS-10): test pins what production
// ACTUALLY does in this fixture, NOT what the spec wished.
//
// Telemetry counters increment at ArchSimGameInstance.cpp:61-62 (TickCount / AccumulatedSeconds),
// which runs BEFORE the GetSubsystem registry lookup at cpp:82-89. So manual
// Tick() calls in headless DO update the telemetry even though the driver loop
// early-bails — this makes the telemetry counters fully testable in isolation.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ArchSimGameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArchSimTickDriverSmokeTest,
    "ArchSim.Integration.TickDriver",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArchSimTickDriverSmokeTest::RunTest(const FString& Parameters)
{
    // ---- Sub-check 1: construction + initial telemetry defaults ----------------
    // NewObject<UArchSimGameInstance>() yields a live transient instance.
    // All four telemetry counters must sit at their documented production defaults
    // before any Tick() or Init() is called.
    UArchSimGameInstance* GI = NewObject<UArchSimGameInstance>();
    if (!TestNotNull(TEXT("Sub-check 1: GI constructed via NewObject"), GI)) return false;

    TestEqual(TEXT("Sub-check 1a: initial TickCount == 0"),
              GI->GetTickCount(), 0);
    TestTrue(TEXT("Sub-check 1b: initial AccumulatedSeconds == 0.f"),
             FMath::IsNearlyEqual(GI->GetAccumulatedTime(), 0.f, 1e-6f));
    // LastSeenRegisteredCount starts at -1 so the first Tick after Init() sees a
    // delta even when the registry has 0 members (ArchSimGameInstance.h:98-99).
    TestEqual(TEXT("Sub-check 1c: initial LastSeenRegisteredCount == -1"),
              GI->GetLastSeenRegisteredCount(), -1);
    // SolveTriggerCount starts at 0; only incremented when the driver loop body fires.
    TestEqual(TEXT("Sub-check 1d: initial SolveTriggerCount == 0"),
              GI->GetSolveTriggerCount(), 0);

    // ---- Sub-check 2: 5-tick burst increments telemetry deterministically ------
    // Even though GetSubsystem() returns null in headless (no subsystem pipeline),
    // the telemetry increment at ArchSimGameInstance.cpp:61-62 runs BEFORE the
    // registry lookup at cpp:82, so we observe TickCount and AccumulatedSeconds
    // changing even when the driver-loop branch cannot fire.
    const float kFrameDt = 1.f / 60.f;
    for (int32 i = 0; i < 5; ++i)
    {
        GI->Tick(kFrameDt);
    }
    TestEqual(TEXT("Sub-check 2a: TickCount after 5 manual Tick() calls == 5"),
              GI->GetTickCount(), 5);
    // AccumulatedSeconds = 5 * (1/60) ≈ 0.08333... — use 1e-5 epsilon (float32 safe).
    TestTrue(TEXT("Sub-check 2b: AccumulatedSeconds ~ 5 * (1/60)"),
             FMath::IsNearlyEqual(GI->GetAccumulatedTime(), 5.f * kFrameDt, 1e-5f));

    // ---- Sub-check 3: headless guarantee — driver loop never fires -------------
    // GetSubsystem<UArchSimModelRegistry>() returns null (headless). The bail at
    // cpp:83-89 returns BEFORE the count comparison at cpp:91-110. So LastSeen and
    // SolveTrigger stay at their initial values after any number of Tick() calls.
    TestEqual(TEXT("Sub-check 3a: LastSeenRegisteredCount unchanged at -1 "
                   "(headless: Registry==null early-bail at cpp:83-89)"),
              GI->GetLastSeenRegisteredCount(), -1);
    TestEqual(TEXT("Sub-check 3b: SolveTriggerCount still 0 "
                   "(headless: driver-loop body at cpp:91-110 never reached)"),
              GI->GetSolveTriggerCount(), 0);

    // ---- Sub-check 4: CDO IsTickable() is false (IsTemplate() == true) --------
    // FTickableGameObject queries IsTickable() every frame on all registered
    // instances. The CDO must return false to prevent phantom ticks before any
    // play session is active.
    // See ArchSimGameInstance.cpp:113-123: `return bIsActive && GetWorld() != nullptr && !IsTemplate()`.
    UArchSimGameInstance* CDO =
        UArchSimGameInstance::StaticClass()->GetDefaultObject<UArchSimGameInstance>();
    if (!TestNotNull(TEXT("Sub-check 4: CDO accessible"), CDO)) return false;
    TestFalse(TEXT("Sub-check 4a: CDO IsTickable() == false (IsTemplate()==true)"),
              CDO->IsTickable());

    // ---- Sub-check 5: uninit instance IsTickable() is false (bIsActive==false) -
    // A freshly NewObject'd instance has never had Init() called, so bIsActive
    // stays at the header default of false (ArchSimGameInstance.h:111).
    // IsTickable() at cpp:123 evaluates: false && ... == false immediately.
    // This protects against premature ticks in Shutdown/teardown edge cases.
    TestFalse(TEXT("Sub-check 5: uninit instance IsTickable() == false (bIsActive=false)"),
              GI->IsTickable());

    // ---- Sub-check 6: 100-tick burst — no aliasing or integer wrap -------------
    // Pure counter discipline: a burst of 100 ticks must yield TickCount==100
    // and AccumulatedSeconds==100*(1/60) with float32 epsilon. Starting from
    // a fresh GI2 so GI's 5-tick state doesn't interfere.
    UArchSimGameInstance* GI2 = NewObject<UArchSimGameInstance>();
    if (!TestNotNull(TEXT("Sub-check 6: GI2 constructed"), GI2)) return false;
    for (int32 i = 0; i < 100; ++i)
    {
        GI2->Tick(kFrameDt);
    }
    TestEqual(TEXT("Sub-check 6a: 100-tick burst TickCount == 100"),
              GI2->GetTickCount(), 100);
    // Use 1e-4 epsilon for 100 frames of float accumulation; float32 ULP at
    // 100*(1/60) ≈ 1.667 is ~2e-5, so 1e-4 gives ~5 ULP of headroom.
    TestTrue(TEXT("Sub-check 6b: 100-tick burst AccumulatedSeconds ~ 100*(1/60)"),
             FMath::IsNearlyEqual(GI2->GetAccumulatedTime(),
                                   100.f * kFrameDt, 1e-4f));

    // ---- Sub-check 7: const-getters are observably pure (no mutation on read) --
    // Calling the same BP-pure getter twice in a row must return identical values.
    // This documents the BlueprintPure contract: no implicit state change on read.
    // (UE4 § UFUNCTION BlueprintPure re-executes body per usage — the body being
    // a plain `return Field;` is the correct proof that it is truly side-effect-free.)
    const int32 CountA = GI2->GetTickCount();
    const int32 CountB = GI2->GetTickCount();
    TestEqual(TEXT("Sub-check 7a: GetTickCount() observably pure (two reads identical)"),
              CountA, CountB);
    const float TimeA = GI2->GetAccumulatedTime();
    const float TimeB = GI2->GetAccumulatedTime();
    TestTrue(TEXT("Sub-check 7b: GetAccumulatedTime() observably pure (two reads identical)"),
             FMath::IsNearlyEqual(TimeA, TimeB, 0.f));  // must be bit-identical

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
