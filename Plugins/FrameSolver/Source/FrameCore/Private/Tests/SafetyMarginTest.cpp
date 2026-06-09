// UE automation mirror of the standalone F27 — collapse foundation stage 2 (C3 + C4).
// C3: structural worst Demand/Capacity + safety factor 1/maxDC (worstUtilization).
// C4: pivotMargin = min/max LDLT pivot ratio (=1 for a single DOF, 0 for a mechanism).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/ElasticAllowable.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreSafetyMarginTest,
    "FrameCore.Collapse.SafetyMargins",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreSafetyMarginTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    Section  sec = Section::Rectangular(100.0, 100.0);
    Material mat(210000.0, 80769.0, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);
    const real L = 2000.0, P = 1000.0;
    const double dcExact = (P * L / sec.Wz()) / mat.cap.bend;   // 0.04

    // C3: cantilever worst utilization = closed form, safety factor = 1/maxDC
    FrameModel m; m.materials = { mat }; m.sections = { sec };
    Node n0(0, 0, 0, 0); n0.fixAll();
    Node n1(1, L, 0, 0);
    m.nodes = { n0, n1 };
    m.members = { Member(0, 0, 1, 0, 0) };
    NodalLoad p; p.node = 1; p.comp[Uz] = -P; m.nodalLoads = { p };
    SolveResult r = solve(m);
    DemandSummary ds = worstUtilization(m, r);
    TestTrue(TEXT("worst utilization valid"), ds.valid);
    TestTrue(TEXT("max D/C = (PL/W)/cap (closed form)"), FMath::Abs(ds.maxDC - dcExact) <= 1e-9 * dcExact);
    TestTrue(TEXT("safety factor = 1/maxDC"), FMath::Abs(ds.safetyFactor - 1.0 / dcExact) <= 1e-9 * (1.0 / dcExact));

    // C4: single free DOF (axial bar) -> exactly one pivot -> pivotMargin == 1
    FrameModel ma; ma.materials = { mat }; ma.sections = { sec };
    Node a0(0, 0, 0, 0); a0.fixAll();
    Node a1(1, 1000, 0, 0);
    a1.fixed[Uy] = a1.fixed[Uz] = a1.fixed[Rx] = a1.fixed[Ry] = a1.fixed[Rz] = true;  // free: Ux only
    ma.nodes = { a0, a1 };
    ma.members = { Member(0, 0, 1, 0, 0) };
    NodalLoad pa; pa.node = 1; pa.comp[Ux] = 1000.0; ma.nodalLoads = { pa };
    TestTrue(TEXT("single-DOF axial pivotMargin = 1"), FMath::Abs(solve(ma).pivotMargin - 1.0) <= 1e-12);

    // C4: mechanism (remove the only member) -> singular and pivotMargin floored at 0
    FrameModel mS = m; mS.members[0].active = false;
    SolveResult rS = solve(mS);
    TestTrue(TEXT("mechanism -> singular & pivotMargin 0"), rS.singular && rS.pivotMargin == 0.0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
