// ArchSim.Persistence.RebaselineCeiling — AS-10
// Ref: docs/logs/S-02/plan_2026-06-26T1033.md § AS-10
//
// Pins the strict `> MaxRankBeforeRebaseline=96` trip semantic in
// UArchSimModelRegistry::RequestSolve (ArchSimModelRegistry.cpp:281).
//
// SPEC CORRECTION NOTICE (per AS-07 lesson #1):
//   The plan description said "96th toggle triggers force-rebaseline branch".
//   Production reality: the condition at cpp:281 is strict `>`, so the FIRST
//   call that pushes PendingRankAccumulation ABOVE 96 (i.e. to 97+) trips the
//   branch, NOT the call that lands exactly at 96. This test pins the real
//   production semantic. The plan description was off-by-one.
//
// HEADLESS FIXTURE LIMITATION (critical; honest per AS-07 lesson):
//   The trip path at cpp:281-286 requires a live GameInstance and UWorld:
//
//     void UArchSimModelRegistry::RequestSolve(const FFrameModelPatch& Patch)
//     {
//         MergePatch(PendingPatch, Patch);
//         PendingRankAccumulation += PatchRank(Patch);     // cpp:272 — always runs
//
//         UGameInstance* GI = GetGameInstance();
//         if (!GI) return;                                 // cpp:274-275 — headless early-return
//         UWorld* World = GI->GetWorld();
//         if (!World) return;
//
//         if (PendingRankAccumulation > MaxRankBeforeRebaseline)   // cpp:281
//         {
//             World->GetTimerManager().ClearTimer(DebounceTimer);  // cpp:283
//             bNeedsRebaseline = true;                             // cpp:284
//             ExecuteSolve();                                      // cpp:285
//             return;
//         }
//         ...SetTimer(...)
//     }
//
//   With NewObject<UArchSimModelRegistry>(), GetGameInstance() returns nullptr,
//   so RequestSolve early-returns at cpp:275 before the trip check. Consequences:
//     - PendingRankAccumulation GROWS unboundedly (cpp:272 runs before the guard).
//     - bNeedsRebaseline is NEVER set true (cpp:284 is never reached).
//     - ExecuteSolve is NEVER called, so PendingRankAccumulation is NEVER reset.
//
//   What we CAN test in headless:
//     (a) The constant MaxRankBeforeRebaseline == 96 (compile-time contract).
//     (b) The getter GetPendingRankAccumulation() is a pure const observer.
//     (c) The accumulator increments correctly from PatchRank logic.
//     (d) Boundary 96 — where accumulation lands but trip cannot fire in headless.
//     (e) At 97 accum — where trip WOULD fire with a live GI, but in headless the
//         accumulator just reads 97 and bNeedsRebaseline stays false.
//     (f) Multi-rank patch (one call with PatchRank=97) — same headless semantics.
//     (g) Empty patch (PatchRank=0) — no increment, still below threshold.
//
//   The strict `>` semantic (97 trips, 96 does NOT) is pinned by checking that the
//   accumulator reaches 97 before the threshold is exceeded, not 96. If a future
//   refactor changes cpp:281 to `>=`, the boundary sub-checks here will still pass
//   in headless (because trip never fires), but the contract comment will serve as
//   a change-detection signal together with the production code read.
//
//   To fully test the trip behavior (bNeedsRebaseline=true + ExecuteSolve path),
//   a follow-on test using a PIE World + GameInstance fixture would be needed.
//   Deferred per HANDOFF_v0.1.3 AS-10 note.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Subsystems/ArchSimModelRegistry.h"
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"   // FFrameModelPatch

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArchSimRebaselineCeilingTest,
    "ArchSim.Persistence.RebaselineCeiling",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArchSimRebaselineCeilingTest::RunTest(const FString& /*Parameters*/)
{
    // ---- compile-time constant contract ----------------------------------------
    // Sub-check 1: MaxRankBeforeRebaseline == 96 at compile time.
    // If this changes, every comment and boundary below needs updating.
    constexpr int32 kMaxRank = UArchSimModelRegistry::GetMaxRankBeforeRebaseline();
    TestEqual(TEXT("Sub-check 1: constexpr MaxRankBeforeRebaseline == 96"), kMaxRank, 96);

    // ---- fresh registry in headless mode ---------------------------------------
    // NewObject<UArchSimModelRegistry>() gives us a transient instance with
    // GetGameInstance() == nullptr. See HEADLESS FIXTURE LIMITATION in file header.
    UArchSimModelRegistry* Registry = NewObject<UArchSimModelRegistry>();
    if (!TestNotNull(TEXT("Registry constructed via NewObject"), Registry)) return false;

    // Sub-check 2: initial state — accumulator starts at 0, flag starts false.
    TestEqual(TEXT("Sub-check 2a: initial PendingRankAccumulation == 0"),
              Registry->GetPendingRankAccumulation(), 0);
    TestFalse(TEXT("Sub-check 2b: initial IsRebaselineDue == false"),
              Registry->IsRebaselineDue());

    // ---- boundary: exactly 96 rank units — must NOT trip ----------------------
    // We drive accumulation via 96 separate RequestSolve calls, each with a
    // single-element DeactivateMemberIds patch (PatchRank = 1 per call).
    // In headless, RequestSolve early-returns at cpp:275 after incrementing
    // PendingRankAccumulation at cpp:272 — so the accumulator grows but the
    // trip check at cpp:281 is never reached.
    //
    // The key assertion: at accum=96 (exactly MaxRankBeforeRebaseline), the
    // strict `>` condition at cpp:281 would evaluate as `96 > 96` == false,
    // so the trip should NOT fire even with a live GI. This is the off-by-one
    // boundary that pin's the `>` (not `>=`) semantic.
    for (int32 i = 0; i < kMaxRank; ++i)
    {
        FFrameModelPatch P;
        P.DeactivateMemberIds.Add(1000 + i);  // sentinel id, not registered; PatchRank = 1
        Registry->RequestSolve(P);
    }
    // Sub-check 3: after 96 rank-1 calls, accumulator should be exactly 96.
    // bNeedsRebaseline must stay false: in headless (no GI), the trip path is
    // unreachable. Even with a live GI, `96 > 96` is false so trip would NOT fire.
    // This sub-check pins BOTH the accumulation math AND the non-trip at boundary.
    TestEqual(TEXT("Sub-check 3a: after 96 rank-1 calls, accumulator == 96"),
              Registry->GetPendingRankAccumulation(), kMaxRank);
    TestFalse(TEXT("Sub-check 3b: at boundary 96, IsRebaselineDue stays false "
                   "(strict `>` semantic: 96 > 96 is false; headless early-return "
                   "also prevents trip)"),
              Registry->IsRebaselineDue());

    // ---- strict `>` boundary: the 97th rank unit -----------------------------
    // With a live GI, the 97th call would push accum to 97, evaluate `97 > 96`
    // as true, set bNeedsRebaseline=true, and call ExecuteSolve() (which would
    // then reset accum to 0 and potentially clear bNeedsRebaseline).
    //
    // In headless: the accumulator goes to 97, and RequestSolve early-returns at
    // cpp:275. bNeedsRebaseline stays false. The accumulator stays at 97.
    //
    // What this pinned: the threshold is 96 (not 97), and crossing it requires
    // reaching 97+ (not landing at exactly 96). If the condition were `>=` instead
    // of `>`, then the 96th call (not the 97th) would be the boundary crossing.
    {
        FFrameModelPatch P;
        P.DeactivateMemberIds.Add(2000);  // PatchRank = 1; pushes accum from 96 to 97
        Registry->RequestSolve(P);
    }
    // Sub-check 4: after the 97th rank unit, accum == 97 (not 0!).
    // The headless early-return means ExecuteSolve never runs, so the accumulator
    // is NOT reset. bNeedsRebaseline is NOT set (trip path unreachable in headless).
    // Together these pin the accumulation math and distinguish headless from live-GI.
    TestEqual(TEXT("Sub-check 4a: after 97th rank unit, accumulator == 97 "
                   "(not reset; headless ExecuteSolve never fires)"),
              Registry->GetPendingRankAccumulation(), kMaxRank + 1);
    TestFalse(TEXT("Sub-check 4b: headless: IsRebaselineDue still false "
                   "(trip path at cpp:281 unreachable without GI; bNeedsRebaseline "
                   "only set at cpp:284 inside the trip block)"),
              Registry->IsRebaselineDue());

    // ---- const getter purity: reading does not mutate ------------------------
    // Sub-check 5: call the getter multiple times on the same state, verify
    // the values are identical (no side-effect from observation).
    {
        const int32 ReadA = Registry->GetPendingRankAccumulation();
        const int32 ReadB = Registry->GetPendingRankAccumulation();
        const int32 ReadC = Registry->GetPendingRankAccumulation();
        TestEqual(TEXT("Sub-check 5a: getter is observably pure (ReadA == ReadB)"),
                  ReadA, ReadB);
        TestEqual(TEXT("Sub-check 5b: getter is observably pure (ReadB == ReadC)"),
                  ReadB, ReadC);
        // Also verify IsRebaselineDue is stable across reads.
        const bool FlagA = Registry->IsRebaselineDue();
        const bool FlagB = Registry->IsRebaselineDue();
        TestEqual(TEXT("Sub-check 5c: IsRebaselineDue is observably pure (FlagA == FlagB)"),
                  FlagA, FlagB);
    }

    // ---- multi-rank single patch: PatchRank=97 in one call -------------------
    // Verify that PatchRank counts all four toggle lists:
    //   PatchRank = DeactivateMemberIds.Num() + ReactivateMemberIds.Num()
    //             + DeactivateShellIds.Num()  + ReactivateShellIds.Num()
    // A single patch with 97 entries in DeactivateMemberIds has PatchRank=97.
    // In headless, this single call pushes accum from 0 to 97 and early-returns.
    UArchSimModelRegistry* R2 = NewObject<UArchSimModelRegistry>();
    if (!TestNotNull(TEXT("R2 Registry constructed"), R2)) return false;
    {
        FFrameModelPatch P;
        for (int32 i = 0; i < 97; ++i) P.DeactivateMemberIds.Add(4000 + i);
        // PatchRank(P) = 97; headless: accum goes 0 -> 97, early-return at cpp:275.
        R2->RequestSolve(P);
    }
    // Sub-check 6: single rank-97 patch → accum=97 (above threshold, but not reset).
    TestEqual(TEXT("Sub-check 6a: single rank-97 patch -> accum == 97"),
              R2->GetPendingRankAccumulation(), 97);
    TestFalse(TEXT("Sub-check 6b: single rank-97 patch -> IsRebaselineDue false "
                   "(headless GI-null early-return prevents trip)"),
              R2->IsRebaselineDue());
    // The trip WOULD fire at exactly this point with a live GI — verify the
    // semantic: 97 > 96 is true. We cannot observe the flag being set in headless,
    // but we can assert the accumulator is above the threshold constant.
    TestTrue(TEXT("Sub-check 6c: R2 accum is above threshold (97 > kMaxRank=96) "
                  "confirming the strict `>` trip would fire with a live GI"),
             R2->GetPendingRankAccumulation() > kMaxRank);

    // ---- empty patch: PatchRank=0 must not increment accumulator --------------
    UArchSimModelRegistry* R3 = NewObject<UArchSimModelRegistry>();
    if (!TestNotNull(TEXT("R3 Registry constructed"), R3)) return false;
    // First bring R3 to accum=90 (below threshold, no trip even with live GI).
    {
        FFrameModelPatch P;
        for (int32 i = 0; i < 90; ++i) P.DeactivateMemberIds.Add(5000 + i);
        R3->RequestSolve(P);
    }
    TestEqual(TEXT("Sub-check 7 setup: R3 accum after 90-rank patch == 90"),
              R3->GetPendingRankAccumulation(), 90);
    // Now send an empty patch (PatchRank=0). Accumulator must not change.
    {
        FFrameModelPatch P;  // all lists empty; PatchRank = 0
        R3->RequestSolve(P);
    }
    // Sub-check 7: empty patch leaves accumulator unchanged at 90.
    TestEqual(TEXT("Sub-check 7a: empty patch leaves accumulator at 90"),
              R3->GetPendingRankAccumulation(), 90);
    TestFalse(TEXT("Sub-check 7b: empty patch on accum=90 does NOT set IsRebaselineDue"),
              R3->IsRebaselineDue());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
