// Phase 6 final fixture diversity — vertical axial column (F4 fixture pattern).
// Exercises the refVec degeneracy fallback (member axis along +Z, where the default
// refVec(0,0,1) collinear with the axis triggers the engine's fallback ordering).
// Verifies the marshal layer carries axial-N correctly and the column does not show
// bending response under pure axial load.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/FrameCoreUETypes.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Load.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/StressField.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "FrameCoreUETestHelpers.h"  // V321-05: shared forward decl for FrameCoreUE::ToBlueprint

namespace {

// F4 vertical column: node0 encastre at origin, node1 at (0,0,h). Gravity P downward
// at node1. Tests the refVec(0,0,1) degeneracy fallback ordering in the engine.
frame::FrameModel BuildAxialColumnFixtureLocal(double P, double h)
{
    using namespace frame;
    Material mat(210000.0, 80769.0, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);
    Section sec = Section::Rectangular(100.0, 100.0);

    FrameModel m;
    m.materials = { mat };
    m.sections  = { sec };

    Node n0(0, 0, 0, 0); n0.fixAll();
    Node n1(1, 0, 0, (real)h);
    m.nodes = { n0, n1 };
    m.members = { Member(0, 0, 1, 0, 0) };

    NodalLoad nl; nl.node = 1; nl.comp[Uz] = (real)(-P);
    m.nodalLoads = { nl };

    return m;
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEAxialColumnTest,
    "FrameCore.UE.AxialColumnTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEAxialColumnTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;

    const double P = 1000.0;     // N downward (compressive)
    const double h = 3000.0;     // 3 m column

    const FrameModel m = BuildAxialColumnFixtureLocal(P, h);
    const SolveResult r = solve(m);
    TestFalse(TEXT("Axial column: solve not singular"), r.singular);
    if (r.singular) { return false; }

    const StressField fld = computeStressField(m, r, 11);
    const FFrameStressField bp = FrameCoreUE::ToBlueprint(fld);

    // (1) shape -- 1 trace, 11 samples (vertical orientation, refVec fallback worked)
    TestEqual(TEXT("Axial column: 1 member trace"), bp.Members.Num(), 1);
    if (bp.Members.Num() != 1) { return false; }
    TestEqual(TEXT("Axial column: 11 samples"), bp.Members[0].Samples.Num(), 11);

    // (2) Axial response under pure tip compression: |N(x)| = P throughout, transverse
    // Vy / Vz / Mz / My should all be ~0 (within float roundoff). The engine's local
    // axis convention has compression as positive N for an upward-pointing member
    // taking a downward load (sign depends on local axis orientation; absolute value
    // is what matters here).
    const FFrameStressFieldSample& s0 = bp.Members[0].Samples[0];
    const FFrameStressFieldSample& s5 = bp.Members[0].Samples[5];
    const FFrameStressFieldSample& s10 = bp.Members[0].Samples[10];

    const double absN0  = FMath::Abs((double)s0.N);
    const double absN5  = FMath::Abs((double)s5.N);
    const double absN10 = FMath::Abs((double)s10.N);
    const double relN05  = FMath::Abs(absN0 - absN5) / FMath::Max(absN0, 1e-12);
    const double relN510 = FMath::Abs(absN5 - absN10) / FMath::Max(absN5, 1e-12);
    TestTrue(TEXT("Axial column: |N| constant along member (rel<1e-4 between samples 0/5)"),
             relN05 < 1e-4);
    TestTrue(TEXT("Axial column: |N| constant along member (rel<1e-4 between samples 5/10)"),
             relN510 < 1e-4);
    TestTrue(TEXT("Axial column: |N| approximately equals tip load P (rel<1e-3)"),
             FMath::Abs(absN0 - P) / P < 1e-3);

    // (2b) Sign convention -- F4 standalone fixture asserts N > 0 (compression-positive).
    // The UE test uses FMath::Abs() to compare magnitudes so a sign flip would silently
    // pass; assert the sign explicitly to mirror the standalone F4 contract.
    TestTrue(TEXT("Axial column: N > 0 (compression-positive, mirrors standalone F4)"),
             s0.N > 0.f && s5.N > 0.f && s10.N > 0.f);

    // (3) Transverse shear and bending should be essentially zero for pure axial load.
    // Use absolute tolerance scaled to the engine's natural precision.
    const double absTol = 1e-3;  // (engine outputs in N or N*mm magnitudes; 1e-3 is well below the axial signal of 1000)
    TestTrue(TEXT("Axial column: |Vy| ~ 0 at midspan"),
             FMath::Abs((double)s5.Vy) < absTol);
    TestTrue(TEXT("Axial column: |Vz| ~ 0 at midspan"),
             FMath::Abs((double)s5.Vz) < absTol);
    TestTrue(TEXT("Axial column: |Mz| ~ 0 at midspan"),
             FMath::Abs((double)s5.Mz) < absTol);

    // (4) Marshal sanity -- shell counters stay at sentinel for member-only model
    // v3.3 (U-07): renamed from GoverningShellId; sentinel value unchanged (-1).
    TestEqual(TEXT("Axial column: governingShellIdx == -1 (no shells)"),
              bp.GoverningShellIdx, -1);
    TestEqual(TEXT("Axial column: ShellsTop empty"), bp.ShellsTop.Num(), 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
