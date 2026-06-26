// v3.6 Phase 2 U-12 tests — FFrameModelPatch incremental nodal-load handling in
// UFrameInteractiveSubsystem::ApplyPatchAndResolve.
//
//   1. PatchAddLoad.Cantilever -- start with no load, add +Z load via AddNodalLoads,
//                                  resolve, assert tip Uz matches analytic P*L^3/(3EI).
//   2. PatchResetReplaces      -- with a baseline load, bResetLoads + new
//                                  SetNodalLoads -> new RHS only.

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
    UFrameInteractiveSubsystem* GetSub()
    {
        if (GEngine)
        {
            for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
            {
                if (Ctx.OwningGameInstance)
                {
                    if (UFrameInteractiveSubsystem* S =
                            Ctx.OwningGameInstance->GetSubsystem<UFrameInteractiveSubsystem>())
                    {
                        return S;
                    }
                }
            }
        }
        // AS-24: GetTransientPackage() outer suppresses ClassWithin (UGameInstance)
        // ensure() in isolated single-test runs where no real GameInstance exists.
        return NewObject<UFrameInteractiveSubsystem>(GetTransientPackage());
    }

    void BuildCantileverDef(FFrameModelDef& Def, FFrameSolveOptions& /*Opts*/)
    {
        FFrameMaterial Mat;
        Mat.E = 210000.f; Mat.G = 80769.f; Mat.Nu = 0.3f; Mat.Rho = 7850.f; Mat.Fy = 235.f;
        Mat.Cap.Comp = 300.f; Mat.Cap.Tens = 300.f; Mat.Cap.Shear = 180.f;
        Def.Materials = { Mat };

        FFrameSection Sec;
        Sec.A = 10000.f; Sec.Iy = 8.333333e6f; Sec.Iz = 8.333333e6f;
        Sec.J = 1.4e7f; Sec.Zy = 250000.f; Sec.Zz = 250000.f;
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
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUELoadPatchAddTest,
    "FrameCore.UE.LoadPatch.Add",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUELoadPatchAddTest::RunTest(const FString& /*Parameters*/)
{
    UFrameInteractiveSubsystem* Sub = GetSub();
    if (!Sub) { AddError(TEXT("Subsystem unavailable")); return false; }

    FFrameModelDef Def; FFrameSolveOptions Opts;
    BuildCantileverDef(Def, Opts);   // no nodal loads
    FFrameReanalysisOptions ReOpts;
    FString Err;
    TestTrue(TEXT("Start session"), Sub->StartSession(Def, Opts, ReOpts, Err));

    // Add 1000 N +Z at tip via AddNodalLoads.
    FFrameModelPatch P;
    FFrameNodalLoad L; L.Node = 1;
    L.Comp = { 0.f, 0.f, 1000.f, 0.f, 0.f, 0.f };
    P.AddNodalLoads = { L };
    FFrameSolveResult R;
    TestTrue(TEXT("ApplyPatchAndResolve succeeds"), Sub->ApplyPatchAndResolve(P, R));

    // Analytic tip Uz = P*L^3/(3*E*Iz)
    constexpr double Analytic = 1000.0 * 2000.0 * 2000.0 * 2000.0 /
                                (3.0 * 210000.0 * 8.333333e6);
    if (R.Displacements.Num() >= 2)
    {
        TestTrue(FString::Printf(TEXT("Tip Uz matches analytic (got %.6f, exp %.6f)"),
                                  R.Displacements[1].Uz, (float)Analytic),
                 FMath::IsNearlyEqual(R.Displacements[1].Uz, (float)Analytic, FMath::Abs((float)Analytic) * 1e-4f));
    }

    Sub->EndSession();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUELoadPatchResetTest,
    "FrameCore.UE.LoadPatch.Reset",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUELoadPatchResetTest::RunTest(const FString& /*Parameters*/)
{
    UFrameInteractiveSubsystem* Sub = GetSub();
    if (!Sub) { AddError(TEXT("Subsystem unavailable")); return false; }

    FFrameModelDef Def; FFrameSolveOptions Opts;
    BuildCantileverDef(Def, Opts);
    // Baseline load: +Y 2000 N at tip (will be reset out).
    FFrameNodalLoad Base; Base.Node = 1;
    Base.Comp = { 0.f, 2000.f, 0.f, 0.f, 0.f, 0.f };
    Def.NodalLoads = { Base };
    FFrameReanalysisOptions ReOpts;
    FString Err;
    TestTrue(TEXT("Start"), Sub->StartSession(Def, Opts, ReOpts, Err));

    // Baseline resolve to confirm Y load took effect.
    FFrameSolveResult Baseline;
    Sub->ResolveCurrent(Baseline);
    TestTrue(TEXT("Baseline Uy > 0"),
             Baseline.Displacements.Num() >= 2 && Baseline.Displacements[1].Uy > 0.f);

    // Patch: reset loads, set Z load only.
    FFrameModelPatch P;
    P.bResetLoads = true;
    FFrameNodalLoad L; L.Node = 1;
    L.Comp = { 0.f, 0.f, 1000.f, 0.f, 0.f, 0.f };
    P.SetNodalLoads = { L };
    FFrameSolveResult R;
    TestTrue(TEXT("ApplyPatch"), Sub->ApplyPatchAndResolve(P, R));

    if (R.Displacements.Num() >= 2)
    {
        // After reset+set: Uy should be ~0, Uz > 0.
        TestTrue(FString::Printf(TEXT("Post-reset Uy near 0 (got %.6f)"),
                                  R.Displacements[1].Uy),
                 FMath::Abs(R.Displacements[1].Uy) < 1e-3f);
        TestTrue(TEXT("Post-reset Uz > 0"), R.Displacements[1].Uz > 0.f);
    }

    Sub->EndSession();
    return true;
}

#endif
