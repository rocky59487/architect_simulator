// Phase 6a multi-member marshal test — exercises FrameCoreUE::ToBlueprint on a 3-member
// cantilever (4 nodes, 3 members in series) under tip load. Verifies:
//   1. USTRUCT carries 3 member traces (vs 1 in cantilever, 2 in SS beam)
//   2. MemberId is the user-set engine ID (100/200/300), NOT the array index
//   3. v3.3 BREAKING (U-07): GoverningMemberIdx is the slot index (0 for root in this
//      fixture); the resolved user id (Members[Idx].MemberId == 100) is looked up
//      explicitly so an off-by-one engine pick can't slip through aliasing.
//   4. The governing trace contains the worst sample (sigCompMax == GlobalMaxFiberSigma
//      within float-lossy budget)

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

// 3-segment cantilever along +X with USER-SET member IDs 100 / 200 / 300.
// Geometry: nodes at x = 0, L/3, 2L/3, L; encastre at node 0; tip load P at node 3.
// All members share the same section so the root member (id 100) carries the most
// moment and should govern.
frame::FrameModel BuildMultiMemberFixtureLocal(double P, double L)
{
    using namespace frame;
    Material mat(210000.0, 80769.0, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);
    Section sec = Section::Rectangular(100.0, 100.0);

    FrameModel m;
    m.materials = { mat };
    m.sections  = { sec };

    Node n0(0, 0,         0, 0); n0.fixAll();
    Node n1(1, L / 3.0,   0, 0);
    Node n2(2, 2.0 * L / 3.0, 0, 0);
    Node n3(3, L,         0, 0);
    m.nodes = { n0, n1, n2, n3 };

    m.members = {
        Member(100, 0, 1, 0, 0),  // user id 100, root segment
        Member(200, 1, 2, 0, 0),  // user id 200, middle segment
        Member(300, 2, 3, 0, 0),  // user id 300, tip segment
    };

    NodalLoad nl; nl.node = 3; nl.comp[Uz] = P;
    m.nodalLoads = { nl };

    return m;
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEMarshalMultiMemberTest,
    "FrameCore.UE.MarshalMultiMemberTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEMarshalMultiMemberTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;

    const double P = 1000.0;
    const double L = 3000.0;
    const int    N = 11;

    const FrameModel m = BuildMultiMemberFixtureLocal(P, L);
    const SolveResult r  = solve(m);
    TestFalse(TEXT("Multi-member solve not singular"), r.singular);
    if (r.singular) { return false; }

    const StressField fld = computeStressField(m, r, N);
    const FFrameStressField bp = FrameCoreUE::ToBlueprint(fld);

    // (1) shape — 3 member traces
    TestEqual(TEXT("Multi-member: 3 member traces"), bp.Members.Num(), 3);
    if (bp.Members.Num() != 3) { return false; }

    // (2) MemberId preserved as user-set ID (100/200/300), NOT array index
    TestEqual(TEXT("Multi-member: Members[0].MemberId == 100"),
              bp.Members[0].MemberId, 100);
    TestEqual(TEXT("Multi-member: Members[1].MemberId == 200"),
              bp.Members[1].MemberId, 200);
    TestEqual(TEXT("Multi-member: Members[2].MemberId == 300"),
              bp.Members[2].MemberId, 300);

    // (3) MemberIdx tracks the array index (0/1/2) so consumers can both look up by
    // user ID (.MemberId) and by traversal order (.MemberIdx).
    TestEqual(TEXT("Multi-member: Members[0].MemberIdx == 0"),
              bp.Members[0].MemberIdx, 0);
    TestEqual(TEXT("Multi-member: Members[2].MemberIdx == 2"),
              bp.Members[2].MemberIdx, 2);

    // (4) v3.3 (U-07): GoverningMemberIdx resolves to a slot index, not a user id.
    // Root member sits at slot 0 (first push_back in BuildCantileverFixtureLocal),
    // and the per-member record carries its real id (100). Both are checked so a
    // mistaken engine-side off-by-one (idx in {1,2} mapping to wrong user id) would
    // fail the lookup chain, not just be alias-masked.
    TestTrue(TEXT("Multi-member: globalMaxFiberSigma > 0"),
             bp.GlobalMaxFiberSigma > 0.f);
    TestEqual(TEXT("Multi-member: governingMemberIdx == 0 (root member is slot 0)"),
              bp.GoverningMemberIdx, 0);
    TestEqual(TEXT("Multi-member: lookup Members[GoverningMemberIdx].MemberId == 100"),
              bp.GoverningMemberIdx >= 0 && bp.GoverningMemberIdx < bp.Members.Num()
              ? bp.Members[bp.GoverningMemberIdx].MemberId : -1, 100);

    // (5) The governing trace contains the GlobalMaxFiberSigma sample (sample[0] of
    // member id 100 is the root x=0 location with max sigma).
    const double bpMaxFiber = (double)bp.GlobalMaxFiberSigma;
    double traceMax = 0;
    for (const FFrameStressFieldSample& s : bp.Members[0].Samples)
    {
        const double sigMax = FMath::Max((double)s.SigmaCompMax, (double)s.SigmaTensMax);
        if (sigMax > traceMax) { traceMax = sigMax; }
    }
    const double relGov = FMath::Abs(traceMax - bpMaxFiber) /
                          FMath::Max(bpMaxFiber, 1e-12);
    TestTrue(TEXT("Multi-member: max sample in governing trace == GlobalMaxFiberSigma (rel<1e-5)"),
             relGov < 1e-5);

    // (6) -1 sentinels for absent governing categories (v3.3 rename, semantics unchanged)
    TestEqual(TEXT("Multi-member: governingShellIdx == -1 (no shells)"),
              bp.GoverningShellIdx, -1);
    TestEqual(TEXT("Multi-member: governingShellCorner == -1"),
              bp.GoverningShellCorner, -1);
    TestEqual(TEXT("Multi-member: ShellsTop empty"), bp.ShellsTop.Num(), 0);
    TestEqual(TEXT("Multi-member: ShellsBot empty"), bp.ShellsBot.Num(), 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
