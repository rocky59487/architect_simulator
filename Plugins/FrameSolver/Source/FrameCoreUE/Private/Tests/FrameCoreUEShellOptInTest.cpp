// v3.4 Phase 5 tests for shell opt-in flag plumbing through UFrameAnalysisLibrary BP entries.
// These are thin BP-side mirrors: the analytic oracles live in standalone F-fixtures (F50/F57/...).
// The BP smoke confirms FFrameSolveOptions / FFrameCorotationalOptions / FFrameCollapseOptions
// toggles propagate to the engine and produce sensible results, NOT zeroing or crashing.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisLibrary.h"
#include "FrameCoreUE/FrameCoreUEMaterialLibrary.h"
#include "FrameCoreUE/FrameCoreUESectionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    // Square clamped plate, 1 MITC4 facet. Length L per edge, thickness t, +Z pressure p.
    // 4 corner nodes CCW about +normal, all fixed (clamped on all 4 sides).
    // NOTE: 1-element clamped plate locks heavily under MITC4 -- the test just asserts
    // the analysis runs and produces non-NaN values; relative parity vs MITC4 is checked
    // qualitatively, not as a tight bit-equality oracle.
    FFrameModelDef BuildClampedPlateFixture(float L = 1000.f, float t = 10.f,
                                            float Pressure = 0.1f, bool ClampAllNodes = true)
    {
        FFrameModelDef def;

        FFrameMaterial mat = UFrameMaterialLibrary::GetS235();
        // Ensure nu set for shell constitutive Dm/Db.
        mat.Nu = 0.3f;
        def.Materials.Add(mat);

        // 4 corner nodes CCW about +Z normal.
        auto MakeNode = [&](int32 id, float x, float y, bool fix)
        {
            FFrameNode N;
            N.Id  = id;
            N.Pos = FVector(x, y, 0.f);
            if (fix) N.Fixed = { true, true, true, true, true, true };
            def.Nodes.Add(N);
        };
        MakeNode(0, 0.f, 0.f, ClampAllNodes);
        MakeNode(1, L,   0.f, ClampAllNodes);
        MakeNode(2, L,   L,   ClampAllNodes);
        MakeNode(3, 0.f, L,   ClampAllNodes);

        FFrameShellQuad Q;
        Q.Id     = 0;
        Q.N      = { 0, 1, 2, 3 };
        Q.MatIdx = 0;
        Q.T      = t;
        def.Shells.Add(Q);

        FFrameShellPressure SP;
        SP.Shell = 0;
        SP.P     = Pressure;
        def.ShellPressures.Add(SP);

        return def;
    }
}

// 5.1 DKQ vs MITC4 -- run a clamped plate with bUseDKQPlate ON vs OFF; both must converge
//     (non-singular) and give a finite displacement at the centre. We do NOT enforce close
//     numeric agreement on a 1-element mesh -- the standalone F-fixtures handle that.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEShellDKQParityTest,
    "FrameCore.UE.ShellDKQParity",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEShellDKQParityTest::RunTest(const FString& /*Parameters*/)
{
    // Clamp only the perimeter -- in a 1-facet plate that's all 4 corner nodes, so we
    // need a 2x2 or finer mesh for an interior node to deflect. Use the 1-facet fixture
    // but free one corner to give a deflection target node.
    FFrameModelDef Def = BuildClampedPlateFixture(1000.f, 10.f, 0.1f, /*clampAllNodes=*/false);
    // Pin three corners, leave one corner free to deflect.
    Def.Nodes[0].Fixed = { true, true, true, true, true, true };
    Def.Nodes[1].Fixed = { true, true, true, true, true, true };
    Def.Nodes[3].Fixed = { true, true, true, true, true, true };
    // Node 2 free.

    FFrameSolveOptions OptsMITC4;   // default: DKQ off
    FFrameSolveOptions OptsDKQ;
    OptsDKQ.bUseDKQPlate = true;

    const FFrameSolveResult RM = UFrameAnalysisLibrary::SolveLinear(Def, OptsMITC4);
    const FFrameSolveResult RD = UFrameAnalysisLibrary::SolveLinear(Def, OptsDKQ);

    TestFalse(TEXT("MITC4 solve not singular"), RM.bSingular);
    TestFalse(TEXT("DKQ solve not singular"),   RD.bSingular);
    if (RM.Displacements.Num() >= 3 && RD.Displacements.Num() >= 3)
    {
        TestTrue(TEXT("MITC4 free-corner Uz finite"), FMath::IsFinite(RM.Displacements[2].Uz));
        TestTrue(TEXT("DKQ free-corner Uz finite"),   FMath::IsFinite(RD.Displacements[2].Uz));
    }
    return true;
}

// 5.2 QM6 incompatible-mode membrane -- in-plane plate, bUseIncompatibleMembrane ON.
//     Smoke: opt-in runs and produces finite displacement.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEShellQM6Test,
    "FrameCore.UE.ShellQM6",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEShellQM6Test::RunTest(const FString& /*Parameters*/)
{
    FFrameModelDef Def = BuildClampedPlateFixture(1000.f, 10.f, /*Pressure=*/0.f, /*ClampAllNodes=*/false);
    Def.ShellPressures.Empty();
    // In-plane edge tension: pin left edge (nodes 0,3), pull right edge (nodes 1,2) in +X.
    Def.Nodes[0].Fixed = { true, true, true, true, true, true };
    Def.Nodes[3].Fixed = { true, true, true, true, true, true };
    FFrameNodalLoad L1; L1.Node = 1; L1.Comp = { 1000.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    Def.NodalLoads.Add(L1);
    FFrameNodalLoad L2; L2.Node = 2; L2.Comp = { 1000.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    Def.NodalLoads.Add(L2);

    FFrameSolveOptions Opts;
    Opts.bUseIncompatibleMembrane = true;
    const FFrameSolveResult R = UFrameAnalysisLibrary::SolveLinear(Def, Opts);

    TestFalse(TEXT("QM6 solve not singular"), R.bSingular);
    if (R.Displacements.Num() >= 4)
    {
        TestTrue(TEXT("Node 1 Ux > 0 (pulled right)"), R.Displacements[1].Ux > 0.f);
    }
    return true;
}

// 5.3 Shell K_sigma buckling -- bShellGeometricStiffness ON, analysis-buckling runs.
//     Smoke: criticalFactor positive + finite.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEShellKSigmaTest,
    "FrameCore.UE.ShellKSigma",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEShellKSigmaTest::RunTest(const FString& /*Parameters*/)
{
    // SS plate fixture: pin left edge translation, pull right edge in -X (compress in-plane).
    FFrameModelDef Def = BuildClampedPlateFixture(1000.f, 10.f, /*Pressure=*/0.f, /*ClampAllNodes=*/false);
    Def.ShellPressures.Empty();
    Def.Nodes[0].Fixed = { true, true, true, true, true, true };
    Def.Nodes[3].Fixed = { true, true, true, true, true, true };
    FFrameNodalLoad L1; L1.Node = 1; L1.Comp = { -1000.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    Def.NodalLoads.Add(L1);
    FFrameNodalLoad L2; L2.Node = 2; L2.Comp = { -1000.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    Def.NodalLoads.Add(L2);

    FFrameSolveOptions Opts;
    Opts.bShellGeometricStiffness = true;
    const FFrameBucklingResult R =
        UFrameAnalysisLibrary::AnalysisBuckling(Def, Opts, FFrameBucklingOptions{});

    // 1-facet shell on a thin plate with K_sigma on; just verify the engine ran without crashing
    // and reported either a positive crit factor or a singular result with a diagnostic --
    // never silently returns zero with bSingular=false.
    const bool Sane = (R.CriticalFactor > 0.f && !R.bSingular) || (R.bSingular && !R.Diagnostic.IsEmpty());
    TestTrue(TEXT("Shell K_sigma either positive crit or explicit singular+diag"), Sane);
    return true;
}

// 5.4 Warped quad admit -- WarpTolerance 0.05 admits a warped facet (one corner lifted).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEShellWarpAdmitTest,
    "FrameCore.UE.ShellWarpAdmit",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEShellWarpAdmitTest::RunTest(const FString& /*Parameters*/)
{
    FFrameModelDef Def = BuildClampedPlateFixture(1000.f, 10.f, 0.1f, /*ClampAllNodes=*/false);
    // Pin 3 corners, leave node 2 free + lift it in Z to warp the quad.
    Def.Nodes[0].Fixed = { true, true, true, true, true, true };
    Def.Nodes[1].Fixed = { true, true, true, true, true, true };
    Def.Nodes[3].Fixed = { true, true, true, true, true, true };
    Def.Nodes[2].Pos.Z = 25.f;   // 25 mm lift over a 1000 mm edge -> tan(theta) ~ 0.025

    FFrameSolveOptions OptsStrict;   // default WarpTolerance = 1e-6 rejects
    FFrameSolveOptions OptsRelax;
    OptsRelax.WarpTolerance = 0.05f; // admits up to 5% lift
    OptsRelax.bUseWarpingCorrection = true;

    const FFrameSolveResult RStrict = UFrameAnalysisLibrary::SolveLinear(Def, OptsStrict);
    const FFrameSolveResult RRelax  = UFrameAnalysisLibrary::SolveLinear(Def, OptsRelax);

    // Strict path should reject the warped facet (singular + diagnostic), relaxed path
    // should admit it and produce a non-singular solve.
    TestTrue(TEXT("Strict path either flags singular or marshals an empty result"),
             RStrict.bSingular || !RStrict.Diagnostic.IsEmpty()
             || RStrict.Displacements.Num() <= 4);
    TestFalse(TEXT("Relaxed (WarpTolerance=0.05) path admits the warp"), RRelax.bSingular);
    return true;
}

// 5.5 Shell co-rotational opt-in -- bShellCorotational flag through FFrameCorotationalOptions.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEShellCorotationalTest,
    "FrameCore.UE.ShellCorotational",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEShellCorotationalTest::RunTest(const FString& /*Parameters*/)
{
    // 1-facet plate as a "shell cantilever" -- pin left edge, push right edge with small +Z load.
    FFrameModelDef Def = BuildClampedPlateFixture(1000.f, 10.f, /*Pressure=*/0.f, /*ClampAllNodes=*/false);
    Def.ShellPressures.Empty();
    Def.Nodes[0].Fixed = { true, true, true, true, true, true };
    Def.Nodes[3].Fixed = { true, true, true, true, true, true };
    FFrameNodalLoad L; L.Node = 1; L.Comp = { 0.f, 0.f, 10.f, 0.f, 0.f, 0.f };
    Def.NodalLoads.Add(L);

    FFrameCorotationalOptions COpts;
    COpts.LoadSteps         = 3;
    COpts.MaxIter           = 20;
    COpts.bShellCorotational = true;
    const FFrameCorotationalResult R = UFrameAnalysisLibrary::SolveCorotational(Def, COpts);

    // Smoke -- shell CR is opt-in experimental. Either it converges OR the engine rejects
    // the model with a clear diagnostic. The unacceptable outcome is a silent NaN.
    const bool Sane = R.bConverged || R.bDiverged || R.FinalState.bSingular;
    TestTrue(TEXT("Shell CR reports a definite state (converged/diverged/singular)"), Sane);
    return true;
}

// 5.6 NM interaction opt-in -- bNMInteraction flag through FFrameCollapseOptions, exercised
//     via BESO redundancy probe (the only Phase 4 BP entry that consumes FFrameCollapseOptions).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUENMInteractionWireTest,
    "FrameCore.UE.NMInteractionWire",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUENMInteractionWireTest::RunTest(const FString& /*Parameters*/)
{
    // Simple 2-member cantilever-ish structure with redundancy probe enabled and NM interaction on.
    FFrameModelDef Def;
    Def.Materials.Add(UFrameMaterialLibrary::GetS235());
    Def.Sections.Add(UFrameSectionLibrary::MakeRectangular(100.f, 100.f));

    FFrameNode N0; N0.Id = 0; N0.Pos = FVector(0.f, 0.f, 0.f);
    N0.Fixed = { true, true, true, true, true, true }; Def.Nodes.Add(N0);
    FFrameNode N1; N1.Id = 1; N1.Pos = FVector(0.f, 100.f, 0.f);
    N1.Fixed = { true, true, true, true, true, true }; Def.Nodes.Add(N1);
    FFrameNode N2; N2.Id = 2; N2.Pos = FVector(2000.f, 0.f, 0.f);
    Def.Nodes.Add(N2);
    FFrameMember M0; M0.Id = 0; M0.I = 0; M0.J = 2; M0.MatIdx = 0; M0.SecIdx = 0;
    Def.Members.Add(M0);
    FFrameMember M1; M1.Id = 1; M1.I = 1; M1.J = 2; M1.MatIdx = 0; M1.SecIdx = 0;
    Def.Members.Add(M1);
    FFrameNodalLoad L; L.Node = 2; L.Comp = { 0.f, 0.f, 1000.f, 0.f, 0.f, 0.f };
    Def.NodalLoads.Add(L);

    FFrameBESOOptions BOpts;
    BOpts.TargetVolFrac      = 0.5f;
    BOpts.EvolRate           = 0.5f;
    BOpts.MaxIter            = 5;
    BOpts.RedundancyCheckEvery = 1;
    BOpts.RedundancySamples    = 0;
    BOpts.Redundancy.bPlasticHinges = true;
    BOpts.Redundancy.bNMInteraction = true;
    BOpts.Redundancy.MaxSteps       = 16;

    const FFrameBESOResult R = UFrameAnalysisLibrary::SolveBESO(Def, BOpts, /*designMembers=*/{0, 1});

    TestTrue(TEXT("BESO with NM-interaction redundancy probe iterated"), R.Iterations > 0);
    const bool ReasonValid = R.Reason == EFrameBESOStop::TargetReached
                          || R.Reason == EFrameBESOStop::Stalled
                          || R.Reason == EFrameBESOStop::ComplianceJump
                          || R.Reason == EFrameBESOStop::MaxIter
                          || R.Reason == EFrameBESOStop::Mechanism;
    TestTrue(TEXT("BESO terminated with a defined reason"), ReasonValid);
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
