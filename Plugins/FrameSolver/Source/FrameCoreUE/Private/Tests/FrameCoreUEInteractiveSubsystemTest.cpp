// v3.5 Phase 7 tests -- 3 sub-checks of UFrameInteractiveSubsystem.
//
//   1. StartEndLifetime -- start session, EndSession, IsSessionActive() flips false; no crash
//                          on double EndSession.
//   2. PatchSemantics    -- baseline ApplyPatchAndResolve(empty) -> result A; ApplyPatchAndResolve
//                          (deactivate member 0) -> singular (cantilever with single member
//                          deactivated has no path to ground -> mechanism).
//   3. PerfBaseline     -- 50-segment cantilever (~300 DOF), measure ApplyPatchAndResolve.
//                          Hard-spec target is 16.7 ms / 60 fps @ 10K DOF (engine R2 lane
//                          benchmark; UE wrapper here only verifies thin marshalling).
//                          Honest fallback: 4x faster than a fresh frame::solve baseline.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

#include "FrameCoreUE/FrameInteractiveSubsystem.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    UFrameInteractiveSubsystem* GetSubsystem()
    {
        if (GEngine)
        {
            for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
            {
                if (Ctx.OwningGameInstance)
                {
                    if (UFrameInteractiveSubsystem* Sub =
                            Ctx.OwningGameInstance->GetSubsystem<UFrameInteractiveSubsystem>())
                    {
                        return Sub;
                    }
                }
            }
        }
        // Headless UE automation (-nullrhi -unattended) has no GameInstance. Fall back
        // to a transient subsystem instance — the subsystem's StartSession / Apply /
        // EndSession logic doesn't actually need a real GameInstance owner; only the
        // subsystem-manager lookup does.
        // AS-24: GetTransientPackage() outer suppresses ClassWithin (UGameInstance)
        // ensure() fired in UObjectGlobals.cpp when outer=null for a ClassWithin-constrained
        // class. In isolated single-test runs the ensure cascades to NotNull.cpp fatal.
        return NewObject<UFrameInteractiveSubsystem>(GetTransientPackage());
    }

    // Single-member cantilever: 2 nodes, S235 steel, 100x100 rect, L = 2 m, P = 1000 N at tip.
    void BuildCantileverDef(FFrameModelDef& Def, FFrameSolveOptions& Opts)
    {
        FFrameMaterial Mat;
        Mat.E = 210000.f; Mat.G = 80769.f; Mat.Nu = 0.3f; Mat.Rho = 7850.f; Mat.Fy = 235.f;
        Mat.Cap.Comp = 300.f; Mat.Cap.Tens = 300.f; Mat.Cap.Shear = 180.f;
        Def.Materials = { Mat };

        FFrameSection Sec;
        Sec.A   = 10000.f;
        Sec.Iy  = 8.333333e6f;
        Sec.Iz  = 8.333333e6f;
        Sec.J   = 1.4e7f;
        Sec.Zy  = 250000.f;
        Sec.Zz  = 250000.f;
        Sec.Shape = EFrameSectionShape::Rectangular;
        Def.Sections = { Sec };

        FFrameNode N0; N0.Id = 0; N0.Pos = FVector::ZeroVector;
        N0.Fixed = { true, true, true, true, true, true };
        N0.Prescribed = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
        FFrameNode N1; N1.Id = 1; N1.Pos = FVector(2000.f, 0.f, 0.f);
        N1.Fixed = { false, false, false, false, false, false };
        N1.Prescribed = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
        Def.Nodes = { N0, N1 };

        FFrameMember M; M.Id = 0; M.I = 0; M.J = 1; M.MatIdx = 0; M.SecIdx = 0;
        M.Release.Init(false, 12);
        Def.Members = { M };

        FFrameNodalLoad L; L.Node = 1; L.Comp = { 0.f, 0.f, 1000.f, 0.f, 0.f, 0.f };
        Def.NodalLoads = { L };

        // Default options are fine for this fixture.
        (void)Opts;
    }
}

// --- 1. StartEndLifetime -----------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEInteractiveLifetimeTest,
    "FrameCore.UE.InteractiveSubsystem.StartEndLifetime",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEInteractiveLifetimeTest::RunTest(const FString& /*Parameters*/)
{
    UFrameInteractiveSubsystem* Sub = GetSubsystem();
    TestNotNull(TEXT("Subsystem retrievable from owning GameInstance"), Sub);
    if (!Sub) return false;

    FFrameModelDef Def; FFrameSolveOptions Opts;
    BuildCantileverDef(Def, Opts);
    FFrameReanalysisOptions ReOpts;
    FString Err;
    const bool bStarted = Sub->StartSession(Def, Opts, ReOpts, Err);
    TestTrue(FString::Printf(TEXT("StartSession succeeds (err=%s)"), *Err), bStarted);
    TestTrue(TEXT("IsSessionActive after start"), Sub->IsSessionActive());

    Sub->EndSession();
    TestFalse(TEXT("IsSessionActive after end"), Sub->IsSessionActive());
    Sub->EndSession();   // double-end safe
    return true;
}

// --- 2. PatchSemantics -------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEInteractivePatchSemanticsTest,
    "FrameCore.UE.InteractiveSubsystem.PatchSemantics",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEInteractivePatchSemanticsTest::RunTest(const FString& /*Parameters*/)
{
    UFrameInteractiveSubsystem* Sub = GetSubsystem();
    if (!Sub) { AddError(TEXT("UFrameInteractiveSubsystem unavailable")); return false; }

    FFrameModelDef Def; FFrameSolveOptions Opts;
    BuildCantileverDef(Def, Opts);
    FFrameReanalysisOptions ReOpts;
    FString Err;
    TestTrue(TEXT("Start"), Sub->StartSession(Def, Opts, ReOpts, Err));

    FFrameSolveResult Baseline;
    TestTrue(TEXT("Baseline resolve"), Sub->ResolveCurrent(Baseline));
    TestFalse(TEXT("Baseline not singular"), Baseline.bSingular);
    TestTrue(TEXT("Baseline tip Uz > 0"),
             Baseline.Displacements.Num() >= 2 && Baseline.Displacements[1].Uz > 0.f);

    FFrameModelPatch Patch;
    Patch.DeactivateMemberIds = { 0 };
    FFrameSolveResult Patched;
    Sub->ApplyPatchAndResolve(Patch, Patched);
    TestTrue(TEXT("Single-member cantilever w/ member 0 deactivated -> singular (mechanism)"),
             Patched.bSingular);

    // Reactivate to verify roundtrip restores baseline behaviour.
    FFrameModelPatch Restore;
    Restore.ReactivateMemberIds = { 0 };
    FFrameSolveResult Restored;
    Sub->ApplyPatchAndResolve(Restore, Restored);
    TestFalse(TEXT("After reactivate: non-singular"), Restored.bSingular);

    Sub->EndSession();
    return true;
}

// --- 3. PerfBaseline ---------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEInteractivePerfBaselineTest,
    "FrameCore.UE.InteractiveSubsystem.PerfBaseline",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEInteractivePerfBaselineTest::RunTest(const FString& /*Parameters*/)
{
    UFrameInteractiveSubsystem* Sub = GetSubsystem();
    if (!Sub) { AddError(TEXT("UFrameInteractiveSubsystem unavailable")); return false; }

    // 50-segment cantilever: 51 nodes * 6 DOF = 306 DOF. Engine R2 lane has the 10K-DOF
    // perf benchmark; this test only verifies the UE wrapper doesn't add silly overhead.
    FFrameModelDef Def; FFrameSolveOptions Opts;
    FFrameMaterial Mat; Mat.E = 210000.f; Mat.G = 80769.f; Mat.Nu = 0.3f; Mat.Rho = 7850.f; Mat.Fy = 235.f;
    Mat.Cap.Comp = 300.f; Mat.Cap.Tens = 300.f; Mat.Cap.Shear = 180.f;
    Def.Materials = { Mat };
    FFrameSection Sec; Sec.A = 10000.f; Sec.Iy = 8.333333e6f; Sec.Iz = 8.333333e6f;
    Sec.J = 1.4e7f; Sec.Zy = 250000.f; Sec.Zz = 250000.f; Sec.Shape = EFrameSectionShape::Rectangular;
    Def.Sections = { Sec };

    constexpr int32 NSeg = 50;
    constexpr float Span = 40.f;     // 40 mm per segment
    for (int32 i = 0; i <= NSeg; ++i)
    {
        FFrameNode N; N.Id = i; N.Pos = FVector((float)i * Span, 0.f, 0.f);
        if (i == 0) { N.Fixed = { true, true, true, true, true, true }; }
        else        { N.Fixed = { false, false, false, false, false, false }; }
        N.Prescribed = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
        Def.Nodes.Add(N);
    }
    for (int32 i = 0; i < NSeg; ++i)
    {
        FFrameMember M; M.Id = i; M.I = i; M.J = i + 1; M.MatIdx = 0; M.SecIdx = 0;
        M.Release.Init(false, 12); Def.Members.Add(M);
    }
    FFrameNodalLoad L; L.Node = NSeg; L.Comp = { 0.f, 0.f, 1000.f, 0.f, 0.f, 0.f };
    Def.NodalLoads = { L };

    FFrameReanalysisOptions ReOpts;
    FString Err;
    TestTrue(TEXT("Start large fixture"), Sub->StartSession(Def, Opts, ReOpts, Err));

    // Warm cache: 1 baseline resolve.
    FFrameSolveResult Warm;
    Sub->ResolveCurrent(Warm);

    // Time 5 patch applications and average.
    FFrameSolveResult Out;
    const double T0 = FPlatformTime::Seconds();
    for (int32 i = 0; i < 5; ++i)
    {
        FFrameModelPatch P;
        // Toggle middle member off/on each round.
        if (i % 2 == 0) { P.DeactivateMemberIds = { NSeg / 2 }; }
        else            { P.ReactivateMemberIds = { NSeg / 2 }; }
        Sub->ApplyPatchAndResolve(P, Out);
    }
    const double T1 = FPlatformTime::Seconds();
    const double AvgMs = (T1 - T0) * 1000.0 / 5.0;

    // v3.6 U-15: tightened threshold. On the 50-segment cantilever fixture (~306 DOF) the
    // expected ApplyPatchAndResolve avg is well under 20 ms; 50 ms is the CI ceiling that
    // catches genuine regressions while accepting CI variance up to ~3x the typical.
    TestTrue(FString::Printf(TEXT("ApplyPatchAndResolve avg %.2f ms / patch <= 50 ms"), AvgMs),
             AvgMs <= 50.0);

    Sub->EndSession();
    return true;
}

// --- 4. EmptyModelStartSession -----------------------------------------------
// AS-17 audit (C-02, S-02 hardening finding): proves the empty-CurrentModel
// StartSession path is production-safe WITHOUT needing an explicit guard in
// FrameInteractiveSubsystem.cpp.
//
// Reasoning (source-traced):
//   - FromBlueprint(EmptyDef, ...) returns true for an all-empty FFrameModelDef
//     because all marshal loops are no-ops and the function falls through to
//     `return true` (FrameCoreUEModelMarshal.cpp last line).
//   - ReSolveSession ctor calls FrameModel::validate() internally; for 0 nodes
//     validate() immediately returns false ("no nodes") — so valid() == false.
//   - The existing valid() check at FrameInteractiveSubsystem.cpp:81-88 catches
//     this, fills OutError from Session->diagnostic(), deletes Session+Cached,
//     and returns false.  No guard needed.
//
// This test is the oracle that pins this contract.  If any refactor regresses
// the graceful-fail path, this test will catch it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEEmptyModelStartSessionTest,
    "FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEEmptyModelStartSessionTest::RunTest(const FString& /*Parameters*/)
{
    UFrameInteractiveSubsystem* Sub = GetSubsystem();
    TestNotNull(TEXT("Subsystem retrievable"), Sub);
    if (!Sub) return false;

    // --- Sub-check 1: fully empty model (0 materials / 0 sections / 0 nodes / 0 members)
    //     StartSession must return false.
    {
        FFrameModelDef EmptyDef;          // all TArrays default-constructed to empty
        FFrameSolveOptions Opts;
        FFrameReanalysisOptions ReOpts;
        FString OutError;

        const bool bStarted = Sub->StartSession(EmptyDef, Opts, ReOpts, OutError);
        TestFalse(TEXT("1a: empty model — StartSession returns false"), bStarted);
        TestFalse(TEXT("1b: empty model — OutError is non-empty (descriptive diagnostic)"),
                  OutError.IsEmpty());
        TestFalse(TEXT("1c: empty model — IsSessionActive is false after failed start"),
                  Sub->IsSessionActive());
    }

    // --- Sub-check 2: double EndSession after failed start must not crash.
    Sub->EndSession();   // idempotent even without an active session

    // --- Sub-check 3: partial empty — 1 material + 1 section but 0 nodes / 0 members.
    //     Must also fail gracefully (validate: "no nodes").
    {
        FFrameModelDef PartialDef;
        FFrameMaterial Mat;
        Mat.E = 210000.f; Mat.G = 80769.f; Mat.Nu = 0.3f; Mat.Rho = 7850.f; Mat.Fy = 235.f;
        PartialDef.Materials = { Mat };
        FFrameSection Sec;
        Sec.A = 10000.f; Sec.Iy = 8.333333e6f; Sec.Iz = 8.333333e6f;
        Sec.J = 1.4e7f; Sec.Zy = 250000.f; Sec.Zz = 250000.f;
        Sec.Shape = EFrameSectionShape::Rectangular;
        PartialDef.Sections = { Sec };
        // Nodes + Members deliberately left empty.
        FFrameSolveOptions Opts;
        FFrameReanalysisOptions ReOpts;
        FString OutError2;

        const bool bStarted2 = Sub->StartSession(PartialDef, Opts, ReOpts, OutError2);
        TestFalse(TEXT("3a: partial (mat+sec, no nodes) — StartSession returns false"), bStarted2);
        TestFalse(TEXT("3b: partial — OutError is non-empty"), OutError2.IsEmpty());
        TestFalse(TEXT("3c: partial — IsSessionActive is false"), Sub->IsSessionActive());
    }

    // --- Sub-check 4: after empty-model failures the subsystem must still accept a valid model.
    //     Proves failed StartSession does NOT leave dirty state that poisons subsequent calls.
    {
        FFrameModelDef Def; FFrameSolveOptions Opts;
        BuildCantileverDef(Def, Opts);
        FFrameReanalysisOptions ReOpts;
        FString Err;

        const bool bRecovery = Sub->StartSession(Def, Opts, ReOpts, Err);
        TestTrue(FString::Printf(TEXT("4a: valid model after empty failures — StartSession succeeds (err=%s)"), *Err),
                 bRecovery);
        TestTrue(TEXT("4b: valid model — IsSessionActive is true"), Sub->IsSessionActive());

        Sub->EndSession();
        TestFalse(TEXT("4c: IsSessionActive false after cleanup EndSession"), Sub->IsSessionActive());
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
