// ArchSim - PIE test for AS-10 rebaseline trip-path branch.
// Sprint S-03 unit AS-13-u2. Depends on harness shipped in AS-13-u1
// (Source/ArchSim/Private/Tests/ArchSimPieHarness.{h,cpp}).
//
// What this test PINS (production-coverage advancement vs ArchSimRebaselineTest.cpp):
//   - GetOrCreateModelRegistry() via PIE harness is non-null and behaviorally
//     equivalent to NewObject<UArchSimModelRegistry>() for accumulator math.
//   - 96 rank-1 calls via harness → accumulator == 96 + IsRebaselineDue() == false.
//   - 97th rank unit via harness → accumulator == 97 (not reset; ExecuteSolve cannot
//     fire in Level 3) + IsRebaselineDue() == false (honest headless defer).
//   - IsRegistryFromRealGI() == false in Level 3 commandlet (confirmed expectation).
//   - Two successive GetOrCreateModelRegistry() calls return independent instances
//     with isolated state (no static singleton leaking between calls).
//   - Harness GetOrFindWorld() is non-null (prerequisite for Level 3 harness path).
//
// What this test CANNOT verify in Level 3 headless (honest defer):
//   - Trip path: bNeedsRebaseline = true at ArchSimModelRegistry.cpp:284
//     requires GetGameInstance() != nullptr at cpp:274 (early-return guard).
//   - ExecuteSolve() at cpp:285 (resets accumulator and dispatches the solve).
//   - SetTimer / ClearTimer at cpp:283 (TimerManager needs a live World + GI loop).
//   Reachable at: Level 1 (real UArchSimGameInstance attached to World) OR
//                 true PIE editor session with active GameInstance.
//
// Honors AS-07 lesson #1: pin actual production behavior; do NOT fabricate
// reachability of trip-path just because PIE harness is now available.
//
// Precedent: ArchSimRebaselineTest.cpp (AS-10 headless analog, same boundary pins).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Tests/ArchSimPieHarness.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"   // FFrameModelPatch

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArchSimPieRebaselineTest,
    "ArchSim.Integration.PieRebaseline",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArchSimPieRebaselineTest::RunTest(const FString& /*Parameters*/)
{
    // ---- Sub-check 1: Level 3 reality — no real GI in commandlet ---------------
    // IsRegistryFromRealGI() re-queries GEngine every call; must be false in the
    // headless 5-leg gate because no UArchSimGameInstance is attached to any context.
    // This sub-check names the coverage level explicitly so gate logs are readable.
    const bool bFromRealGI = ArchSimPieHarness::IsRegistryFromRealGI();
    TestFalse(TEXT("Sub-check 1: IsRegistryFromRealGI() == false in Level 3 commandlet "
                   "(headless: no OwningGameInstance in any GEngine WorldContext)"),
              bFromRealGI);

    // ---- Sub-check 2: Harness world is present ---------------------------------
    // GetOrFindWorld() is the entry-point for all Level 3 harness operations.
    // A null return means GEngine itself is broken — not a registry issue.
    UWorld* World = ArchSimPieHarness::GetOrFindWorld();
    if (!TestNotNull(TEXT("Sub-check 2: GetOrFindWorld() non-null (commandlet always has world)"),
                     World))
    {
        AddError(TEXT("GetOrFindWorld() returned null — cannot continue harness test."));
        return false;
    }

    // ---- Sub-check 3: Harness registry is non-null (Level 3 fallback) ----------
    // GetOrCreateModelRegistry() falls back to NewObject<UArchSimModelRegistry>()
    // when no real GI is present. The returned object must be non-null.
    UArchSimModelRegistry* Reg = ArchSimPieHarness::GetOrCreateModelRegistry();
    if (!TestNotNull(TEXT("Sub-check 3: GetOrCreateModelRegistry() non-null via harness"),
                     Reg))
    {
        return false;
    }

    // ---- Sub-check 4: Accumulator math via harness is identical to NewObject ---
    // Accumulator starts at 0. 96 rank-1 RequestSolve calls must yield accum=96
    // and IsRebaselineDue()==false. This mirrors ArchSimRebaselineTest Sub-check 3
    // to confirm the harness NewObject fallback behaves identically to direct
    // NewObject<UArchSimModelRegistry>() (no extra state injected by harness).
    constexpr int32 kMaxRank = UArchSimModelRegistry::GetMaxRankBeforeRebaseline();  // 96
    TestEqual(TEXT("Sub-check 4 precondition: initial accumulator == 0"),
              Reg->GetPendingRankAccumulation(), 0);
    TestFalse(TEXT("Sub-check 4 precondition: initial IsRebaselineDue == false"),
              Reg->IsRebaselineDue());

    for (int32 i = 0; i < kMaxRank; ++i)
    {
        FFrameModelPatch P;
        P.DeactivateMemberIds.Add(1000 + i);  // PatchRank = 1; sentinel id, not registered
        Reg->RequestSolve(P);
    }
    TestEqual(TEXT("Sub-check 4a: after 96 rank-1 calls via harness registry, accum == 96"),
              Reg->GetPendingRankAccumulation(), kMaxRank);
    TestFalse(TEXT("Sub-check 4b: at boundary 96, IsRebaselineDue stays false "
                   "(strict `>` semantic: 96 > 96 is false; Level 3 GI-null guard "
                   "at ArchSimModelRegistry.cpp:274 prevents trip path anyway)"),
              Reg->IsRebaselineDue());

    // ---- Sub-check 5: 97th rank unit — honest defer on trip path ---------------
    // The 97th rank unit pushes accum to 97 (97 > 96 == true). With a live GI this
    // is where bNeedsRebaseline = true and ExecuteSolve() would fire. In Level 3
    // the GI-null early-return at cpp:274 prevents reaching the trip check at
    // cpp:281, so accum grows but IsRebaselineDue stays false.
    //
    // HONEST DEFER (AS-07 lesson #1):
    //   We do NOT assert bNeedsRebaseline = true here — that would be fabricating
    //   trip-path reachability. We assert the headless reality: accum=97, flag false.
    //   Full trip verification requires Level 1 (real GI) or true PIE session.
    {
        FFrameModelPatch P;
        P.DeactivateMemberIds.Add(2000);  // PatchRank = 1; pushes accum from 96 to 97
        Reg->RequestSolve(P);
    }
    TestEqual(TEXT("Sub-check 5a: after 97th rank unit, accumulator == 97 "
                   "(not reset; Level 3 ExecuteSolve never fires — GI null guard "
                   "at ArchSimModelRegistry.cpp:274 is the gate)"),
              Reg->GetPendingRankAccumulation(), kMaxRank + 1);
    TestFalse(TEXT("Sub-check 5b: IsRebaselineDue still false in Level 3 "
                   "(trip path at cpp:281 unreachable without GI; bNeedsRebaseline only "
                   "set at cpp:284 inside the trip block, which is never entered). "
                   "HONEST DEFER: trip-path requires Level 1 or true PIE session."),
              Reg->IsRebaselineDue());

    // ---- Sub-check 6: Two successive harness calls return independent instances -
    // GetOrCreateModelRegistry() in Level 3 calls NewObject<> each time; there
    // must be no static singleton state bleeding between calls. Verify the second
    // registry starts with accum=0 (fresh), independent of the first's state (97).
    UArchSimModelRegistry* Reg2 = ArchSimPieHarness::GetOrCreateModelRegistry();
    if (!TestNotNull(TEXT("Sub-check 6: second GetOrCreateModelRegistry() non-null"),
                     Reg2))
    {
        return false;
    }
    // The first registry is at accum=97. The second must be at accum=0 — they must
    // be independent objects (no shared static accumulator).
    TestEqual(TEXT("Sub-check 6a: second harness registry accum == 0 (fresh, independent "
                   "of first registry's accum=97; no static state pollution)"),
              Reg2->GetPendingRankAccumulation(), 0);
    TestFalse(TEXT("Sub-check 6b: second harness registry IsRebaselineDue == false (fresh)"),
              Reg2->IsRebaselineDue());
    // The two registries must be distinct UObject instances.
    TestTrue(TEXT("Sub-check 6c: two harness registry calls return distinct instances "
                  "(Reg != Reg2)"),
             Reg != Reg2);

    // ---- Sub-check 7: First registry state is unaffected by second creation ----
    // Verify no aliasing: the first registry's accum must still be 97 after the
    // second GetOrCreateModelRegistry() call (no shared backing store).
    TestEqual(TEXT("Sub-check 7: first registry accum still 97 after second creation "
                   "(no alias, no shared static, no mutation via second call)"),
              Reg->GetPendingRankAccumulation(), kMaxRank + 1);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
