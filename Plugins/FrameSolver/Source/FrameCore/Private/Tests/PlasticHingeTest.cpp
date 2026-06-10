// UE automation mirror of the standalone F32 — plastic hinge mechanics (collapse stage 4a).
// Re-proves the SAME oracles the standalone gate validates on the fixed-fixed two-member
// beam (Mp = fy*Zz = 7.5e7): yield-point continuity at a support hinge (the sign-pinning
// check for both channels: element-side Qf injection + node-side moment), the hinged end
// recovering zero, and the past-yield closed forms |M_far| = wL^2/8 - Mp/2 and
// |M_mid| = wL^2/8 - Mp.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/MemberGeometry.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCorePlasticHingeTest,
    "FrameCore.Collapse.PlasticHinge",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCorePlasticHingeTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    Material mat(210000.0, 80769.0, 7850.0);
    mat.fy = 300.0;
    Section sec = Section::Rectangular(100.0, 100.0);   // Zz = 250000 -> Mp = 7.5e7
    const real Lh = 2000.0, Ltot = 2.0 * Lh;
    const real Mp = mat.fy * sec.Zz;

    auto buildBeam = [&](FrameModel& m, real w)
    {
        m = FrameModel{};
        m.materials = { mat }; m.sections = { sec };
        Node n0(0, 0.0, 0.0, 0.0);  n0.fixAll();
        Node n2(2, Ltot, 0.0, 0.0); n2.fixAll();
        m.nodes = { n0, Node(1, Lh, 0.0, 0.0), n2 };
        m.members = { Member(0, 0, 1, 0, 0), Member(1, 1, 2, 0, 0) };
        MemberUDL u0; u0.member = 0; u0.w_local = { 0.0, -w, 0.0 };
        MemberUDL u1; u1.member = 1; u1.w_local = { 0.0, -w, 0.0 };
        m.memberUDLs = { u0, u1 };
    };
    auto addHinge = [&](FrameModel& m, MemberId memId, int dof, real mp)
    {
        m.hinges.push_back(PlasticHinge{ memId, dof, mp });
        const Member* mem = nullptr;
        for (const auto& mm : m.members) if (mm.id == memId) { mem = &mm; break; }
        const Vec3 pi = m.nodes[(size_t)m.nodeIndex(mem->i)].pos;
        const Vec3 pj = m.nodes[(size_t)m.nodeIndex(mem->j)].pos;
        Vec3 ax, ay, az;
        memberLocalAxes(pi, pj, mem->refVec, ax, ay, az);
        const Vec3 e = (dof == 4 || dof == 10) ? ay : az;
        NodalLoad nl; nl.node = (dof < 6) ? mem->i : mem->j;
        nl.comp[Rx] = -mp * e.x; nl.comp[Ry] = -mp * e.y; nl.comp[Rz] = -mp * e.z;
        m.nodalLoads.push_back(nl);
    };

    // yield-point continuity: hinged solution == elastic solution at |M_end| == Mp
    const real wy = 12.0 * Mp / (Ltot * Ltot);   // 56.25
    FrameModel mE; buildBeam(mE, wy);
    SolveResult rE = solve(mE);
    TestFalse(TEXT("elastic beam not singular"), rE.singular);
    const real MzI0 = rE.memberForces[0].endI.Mz;
    TestTrue(TEXT("|elastic end moment| at w_y = fy*Zz"), FMath::Abs(FMath::Abs(MzI0) - Mp) <= 1e-9 * Mp);

    FrameModel mH; buildBeam(mH, wy);
    addHinge(mH, 0, 5, MzI0);
    SolveResult rH = solve(mH);                  // enableReleases stays false: hinges are state
    TestFalse(TEXT("hinged beam not singular"), rH.singular);
    double duMax = 0.0, uMax = 1e-30;
    for (int k = 0; k < (int)rE.u.size(); ++k)
    {
        duMax = FMath::Max(duMax, FMath::Abs(rH.u[k] - rE.u[k]));
        uMax  = FMath::Max(uMax, FMath::Abs(rE.u[k]));
    }
    TestTrue(TEXT("hinge at yield == elastic (sign-pinning continuity)"), duMax / uMax < 1e-12);
    TestTrue(TEXT("hinged end recovers Mz = 0"), FMath::Abs(rH.memberForces[0].endI.Mz) <= 1e-9);

    // past yield: one support hinge -> |M_far| = wL^2/8 - Mp/2
    const real w = 60.0;
    const real sgn = (MzI0 >= 0) ? 1.0 : -1.0;
    FrameModel m1; buildBeam(m1, w);
    addHinge(m1, 0, 5, sgn * Mp);
    SolveResult r1 = solve(m1);
    TestFalse(TEXT("propped state not singular"), r1.singular);
    const double MBexp = w * Ltot * Ltot / 8.0 - Mp / 2.0;   // 8.25e7
    TestTrue(TEXT("one hinge: |M_far| = wL^2/8 - Mp/2"),
             FMath::Abs(FMath::Abs(r1.memberForces[1].endJ.Mz) - MBexp) <= 1e-9 * MBexp);

    // both support hinges -> |M_mid| = wL^2/8 - Mp
    FrameModel m2; buildBeam(m2, w);
    addHinge(m2, 0, 5, sgn * Mp);
    const real sgnJ = (rE.memberForces[1].endJ.Mz >= 0) ? 1.0 : -1.0;
    addHinge(m2, 1, 11, sgnJ * Mp);
    SolveResult r2 = solve(m2);
    TestFalse(TEXT("two-hinge state not singular"), r2.singular);
    const double MMexp = w * Ltot * Ltot / 8.0 - Mp;         // 4.5e7
    TestTrue(TEXT("two hinges: |M_mid| = wL^2/8 - Mp"),
             FMath::Abs(FMath::Abs(r2.memberForces[0].endJ.Mz) - MMexp) <= 1e-9 * MMexp);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
