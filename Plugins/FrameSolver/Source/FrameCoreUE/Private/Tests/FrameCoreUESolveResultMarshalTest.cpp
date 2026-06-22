// v3.4 Phase 2 tests -- 5 sub-checks against the F68 cantilever fixture
// (100x100 rectangular, S235-like, L = 2000 mm, +Z tip load = 1000 N):
//
//   1. CantileverDisplacement -- tip Uz matches analytic P*L^3 / (3*E*Iz) rel < 1e-5.
//   2. CantileverReaction     -- Reactions[0].Fz == -P (equilibrium) rel < 1e-5.
//   3. CantileverMemberForce  -- |MemberForces[0].EndI.Mz| == P*L (root moment) rel < 1e-5.
//   4. PivotMarginFinite      -- PivotMargin > 0 and IsFinite.
//   5. DemandSummary          -- Utilization.MaxDC > 0 (cantilever is under stress).
//
// Each test is its own IMPLEMENT_SIMPLE_AUTOMATION_TEST so the UE automation harness
// counts them individually (+5 toward run_gate.ps1 $ExpectedUeTests). The cantilever
// fixture is built directly in C++ to avoid any USTRUCT-marshal-side noise -- this is
// the OUTPUT-side test, the input USTRUCT marshal is covered by Phase 1 tests.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/FrameCoreUEResultTypes.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Load.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameSolver.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    constexpr double kP = 1000.0;
    constexpr double kL = 2000.0;
    constexpr double kE = 210000.0;
    constexpr double kIz = 100.0 * 100.0 * 100.0 * 100.0 / 12.0;   // b*d^3/12

    frame::FrameModel BuildCantilever()
    {
        using namespace frame;
        Material mat(kE, 80769.0, 7850.0);
        mat.cap = Capacity::make(300.0, 300.0, 180.0);
        Section sec = Section::Rectangular(100.0, 100.0);

        FrameModel m;
        m.materials = { mat };
        m.sections  = { sec };

        Node n0(0, 0, 0, 0);
        n0.fixAll();
        Node n1(1, kL, 0, 0);
        m.nodes   = { n0, n1 };
        m.members = { Member(0, 0, 1, 0, 0) };

        NodalLoad nl;
        nl.node     = 1;
        nl.comp[Uz] = kP;
        m.nodalLoads = { nl };
        return m;
    }

    bool NearlyEqualRel(double A, double B, double Tol)
    {
        const double Denom = FMath::Max(FMath::Abs(B), 1e-9);
        return FMath::Abs(A - B) / Denom < Tol;
    }
}

// Test 1 -- displacement -------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUECantileverDisplacementTest,
    "FrameCore.UE.CantileverDisplacement",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUECantileverDisplacementTest::RunTest(const FString& /*Parameters*/)
{
    const frame::FrameModel M = BuildCantilever();
    const frame::SolveResult R = frame::solve(M);
    TestFalse(TEXT("solve returned non-singular"), R.singular);

    const FFrameSolveResult BP = FrameCoreUE::ToBlueprint(M, R);
    TestEqual(TEXT("Displacements has 2 entries"), BP.Displacements.Num(), 2);

    // Analytic tip Uz = P * L^3 / (3 * E * Iz). Note Iz (about local z, deflects in local y;
    // local y = +Z global for this fixture because refVec = +Z and local x = +X).
    const double Analytic = kP * (kL * kL * kL) / (3.0 * kE * kIz);
    if (BP.Displacements.Num() >= 2)
    {
        const float UzTip = BP.Displacements[1].Uz;
        TestTrue(FString::Printf(TEXT("Tip Uz matches P*L^3/(3*E*Iz) (got %.6f, exp %.6f)"),
                                 UzTip, Analytic),
                 NearlyEqualRel(UzTip, Analytic, 1e-5));
    }
    // Fixed end should be ~zero displacement.
    if (BP.Displacements.Num() >= 1)
    {
        TestTrue(TEXT("Fixed-end Uz < 1e-6"), FMath::Abs(BP.Displacements[0].Uz) < 1e-6f);
    }
    return true;
}

// Test 2 -- reaction -----------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUECantileverReactionTest,
    "FrameCore.UE.CantileverReaction",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUECantileverReactionTest::RunTest(const FString& /*Parameters*/)
{
    const frame::FrameModel M = BuildCantilever();
    const frame::SolveResult R = frame::solve(M);
    const FFrameSolveResult BP = FrameCoreUE::ToBlueprint(M, R);

    TestEqual(TEXT("Reactions has 2 entries"), BP.Reactions.Num(), 2);
    if (BP.Reactions.Num() >= 1)
    {
        // Sum of forces in z = 0: load = +P at node 1, so reaction Fz at node 0 must equal -P.
        TestTrue(FString::Printf(TEXT("Reactions[0].Fz == -P (got %.6f)"), BP.Reactions[0].Fz),
                 NearlyEqualRel(BP.Reactions[0].Fz, -kP, 1e-5));
        TestTrue(TEXT("Fixed end has constrained DOF flag set"),
                 BP.Reactions[0].bHasConstrainedDof);
    }
    if (BP.Reactions.Num() >= 2)
    {
        // Free end has no constrained DOFs.
        TestFalse(TEXT("Free end has no constrained DOF"), BP.Reactions[1].bHasConstrainedDof);
    }
    return true;
}

// Test 3 -- member force -------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUECantileverMemberForceTest,
    "FrameCore.UE.CantileverMemberForce",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUECantileverMemberForceTest::RunTest(const FString& /*Parameters*/)
{
    const frame::FrameModel M = BuildCantilever();
    const frame::SolveResult R = frame::solve(M);
    const FFrameSolveResult BP = FrameCoreUE::ToBlueprint(M, R);

    TestEqual(TEXT("MemberForces has 1 entry"), BP.MemberForces.Num(), 1);
    if (BP.MemberForces.Num() >= 1)
    {
        const FFrameMemberInternalForces& F = BP.MemberForces[0];
        TestEqual(TEXT("MemberId == 0"), F.MemberId, 0);
        TestEqual(TEXT("MemberIdx == 0"), F.MemberIdx, 0);

        // Axial N along the member axis (no axial load) ~ 0.
        TestTrue(FString::Printf(TEXT("EndI.N ~ 0 (got %.6f)"), F.EndI.N),
                 FMath::Abs(F.EndI.N) < 1e-3f);

        // Root moment: |Mz| at end-I = P * L = 2e6. Sign depends on engine convention;
        // assert magnitude only.
        const double AnalyticRoot = kP * kL;
        TestTrue(FString::Printf(TEXT("|EndI.Mz| ~ P*L (got %.6f, exp %.6f)"),
                                 FMath::Abs(F.EndI.Mz), AnalyticRoot),
                 NearlyEqualRel(FMath::Abs(F.EndI.Mz), AnalyticRoot, 1e-5));
    }
    return true;
}

// Test 4 -- pivot margin -------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEPivotMarginTest,
    "FrameCore.UE.PivotMarginFinite",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEPivotMarginTest::RunTest(const FString& /*Parameters*/)
{
    const frame::FrameModel M = BuildCantilever();
    const frame::SolveResult R = frame::solve(M);
    const FFrameSolveResult BP = FrameCoreUE::ToBlueprint(M, R);

    TestFalse(TEXT("bSingular == false"), BP.bSingular);
    TestTrue(TEXT("PivotMargin > 0"), BP.PivotMargin > 0.f);
    TestTrue(TEXT("PivotMargin is finite"), FMath::IsFinite(BP.PivotMargin));
    return true;
}

// Test 5 -- demand summary -----------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEDemandSummaryTest,
    "FrameCore.UE.DemandSummary",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEDemandSummaryTest::RunTest(const FString& /*Parameters*/)
{
    const frame::FrameModel M = BuildCantilever();
    const frame::SolveResult R = frame::solve(M);
    const FFrameSolveResult BP = FrameCoreUE::ToBlueprint(M, R);

    TestTrue(TEXT("Utilization.bValid"), BP.Utilization.bValid);
    TestTrue(TEXT("Utilization.MaxDC > 0"), BP.Utilization.MaxDC > 0.f);
    TestTrue(TEXT("SafetyFactor > 0"),     BP.Utilization.SafetyFactor > 0.f);
    TestEqual(TEXT("GoverningMemberId == 0"), BP.Utilization.GoverningMemberId, 0);
    TestEqual(TEXT("GoverningMemberIdx resolved to slot 0"), BP.Utilization.GoverningMemberIdx, 0);
    TestNotEqual(TEXT("Mode != None"), (uint8)BP.Utilization.Mode, (uint8)EFrameFailMode::None);

    // No shells in this fixture; bShellValid should be false.
    TestFalse(TEXT("bShellValid == false (no shells)"), BP.Utilization.bShellValid);

    // Per-member utilization populated.
    TestEqual(TEXT("MemberUtilization has 1 entry"), BP.MemberUtilization.Num(), 1);
    if (BP.MemberUtilization.Num() >= 1)
    {
        TestTrue(TEXT("Member peak risk > 0"), BP.MemberUtilization[0].Peak.Risk > 0.f);
    }
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
