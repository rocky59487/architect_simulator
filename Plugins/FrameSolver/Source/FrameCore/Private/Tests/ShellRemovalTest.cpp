// UE automation mirror of the standalone F28 — shell element removal (ShellQuad::active), the
// facet counterpart of FrameCore.Member.ElementRemoval. Re-proves the SAME solver path the
// standalone gate validates: deactivating a clamp-adjacent facet of a 2x2 cantilever plate is
// identical to physically omitting it; the inactive facet recovers zero forces and its pressure
// is dropped (reactions balance the active facets only); removing the sole facet at a corner
// node is flagged as a mechanism (singular).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreShellRemovalTest,
    "FrameCore.Shell.ElementRemoval",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreShellRemovalTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    const real Es = 30000.0, nu = 0.3, Gs = Es / (2.0 * (1.0 + nu));
    Material smat(Es, Gs); smat.nu = nu;
    const real e = 500.0, t = 10.0, p = 0.01;

    auto buildPlate = [&](FrameModel& m)
    {
        m = FrameModel{};
        m.materials = { smat };
        for (int j = 0; j <= 2; ++j)
            for (int i = 0; i <= 2; ++i)
            {
                Node n(j * 3 + i, i * e, j * e, 0.0);
                if (i == 0) n.fixAll();          // clamped edge x=0
                m.nodes.push_back(n);
            }
        m.shells = { ShellQuad(0, 0, 1, 4, 3, 0, t),     // q0: clamp-adjacent
                     ShellQuad(1, 1, 2, 5, 4, 0, t),
                     ShellQuad(2, 3, 4, 7, 6, 0, t),
                     ShellQuad(3, 4, 5, 8, 7, 0, t) };   // q3: sole owner of corner node 8
        for (int s = 0; s < 4; ++s) { ShellPressure sp; sp.shell = s; sp.p = p; m.shellPressures.push_back(sp); }
    };

    FrameModel mFull; buildPlate(mFull);
    SolveResult rFull = solve(mFull);
    TestFalse(TEXT("full plate not singular"), rFull.singular);

    // deactivating q0 must equal physically omitting it (facet AND its pressure)
    FrameModel mCut = mFull; mCut.shells[0].active = false;
    SolveResult rCut = solve(mCut);
    TestFalse(TEXT("not singular after removing q0"), rCut.singular);

    FrameModel mOmit; buildPlate(mOmit);
    mOmit.shells.erase(mOmit.shells.begin());
    mOmit.shellPressures.erase(mOmit.shellPressures.begin());
    SolveResult rOmit = solve(mOmit);
    double duMax = 0.0;
    for (int k = 0; k < (int)rCut.u.size() && k < (int)rOmit.u.size(); ++k)
        duMax = FMath::Max(duMax, FMath::Abs(rCut.u[k] - rOmit.u[k]));
    TestTrue(TEXT("shell active=false identical to omitted facet"), duMax <= 1e-9);

    // inactive facet recovers zero forces
    double fMax = 0.0;
    {
        const ShellElementForces& sf = rCut.shellForces[0];
        for (real v : { sf.Mxx, sf.Myy, sf.Mxy, sf.Qx, sf.Qy, sf.Nxx, sf.Nyy, sf.Nxy })
            fMax = FMath::Max(fMax, FMath::Abs(v));
        for (int c = 0; c < 4; ++c)
        {
            fMax = FMath::Max(fMax, FMath::Abs(sf.MxxC[c]));
            fMax = FMath::Max(fMax, FMath::Abs(sf.MyyC[c]));
            fMax = FMath::Max(fMax, FMath::Abs(sf.MxyC[c]));
        }
    }
    TestTrue(TEXT("inactive facet forces stay zero"), fMax <= 1e-12);

    // the dropped pressure does not leak: reactions balance the 3 ACTIVE facets only
    const double Ftot = p * 3.0 * e * e;   // 7500 N along +z
    double sumRz = 0.0;
    for (int k = 0; k < 9; ++k) sumRz += rCut.reaction(k, Uz);
    TestTrue(TEXT("reactions balance active-facet pressure only"),
             FMath::Abs(sumRz + Ftot) <= 1e-9 * Ftot);

    // removing the far-corner facet isolates node 8 -> mechanism
    FrameModel mMech = mFull; mMech.shells[3].active = false;
    SolveResult rMech = solve(mMech);
    TestTrue(TEXT("isolated node after facet removal -> mechanism"), rMech.singular);

    // flipped shell.active after factoring must reject a stale reuse
    FrameModel mFp; buildPlate(mFp);
    PreparedSystem ps = assembleAndFactor(mFp);
    mFp.shells[0].active = false;
    SolveResult rStale = solveLoad(ps, mFp);
    TestTrue(TEXT("flipped shell.active rejects stale factor"), rStale.singular);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
