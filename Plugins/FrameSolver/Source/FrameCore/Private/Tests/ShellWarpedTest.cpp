// UE mirror of standalone F61 -- the opt-in warped-quad handling (SolveOptions::warpTolerance +
// useWarpingCorrection). warpTolerance relaxes validate()'s hard rejection of non-coplanar quads so a
// warped free-surface mesh can solve at all; useWarpingCorrection projects the corners onto the best-fit
// (Newell / centroid) plane. A FLAT patch stays exact (default-not-broken); a WARPED patch solves with a
// bounded error that shrinks as the warp shrinks -- a warped free-surface mesh reaches accuracy by mesh
// refinement (MITC4 stays a flat facet; this is not a magic per-element fix). Honest scope: best-fit
// projection alone does NOT improve a regular warped quad (Newell == diagonal normal there); the full
// MacNeal/Sabir rotation-coupling warping correction is a later phase. See docs/specs/shell_warping.md.
// NOTE: do NOT name any local constant IN / OUT here -- Windows SAL macros via CoreMinimal.h.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/SolveOptions.h"
#include "FrameCore/Material.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreShellWarpedPatchTest,
    "FrameCore.Shell.WarpedPatch",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreShellWarpedPatchTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    const real Es = 30000.0, nus = 0.3;
    Material smat(Es, Es / (2.0 * (1.0 + nus))); smat.nu = nus;
    const real a = 1000.0, t = 10.0, gx = 1e-4;
    const real NxxExp = t * (Es / (1.0 - nus * nus)) * gx;     // constant-strain membrane: Nxx = t*E/(1-nu^2)*gx
    auto gid = [](int i, int j) { return j * 3 + i; };
    auto buildWarpedPatch = [&](FrameModel& m, real warp) {
        m = FrameModel{};
        m.materials.push_back(smat);
        const real h = a / 2;
        for (int j = 0; j < 3; ++j)
            for (int i = 0; i < 3; ++i) {
                const real z = (i == 1 && j == 1) ? warp : 0.0;        // interior node lifted -> warped quads
                Node nd(gid(i, j), i * h, j * h, z);
                nd.fixed[2] = nd.fixed[3] = nd.fixed[4] = nd.fixed[5] = true;   // pure membrane (no w / rotations)
                const bool bnd = (i == 0 || i == 2 || j == 0 || j == 2);
                if (bnd) { nd.fixed[0] = nd.fixed[1] = true; nd.prescribed[0] = gx * (i * h); nd.prescribed[1] = 0.0; }
                m.nodes.push_back(nd);
            }
        int sid = 0;
        for (int j = 0; j < 2; ++j)
            for (int i = 0; i < 2; ++i)
                m.shells.push_back(ShellQuad(sid++, gid(i, j), gid(i + 1, j), gid(i + 1, j + 1), gid(i, j + 1), 0, t));
    };
    auto maxNxxErr = [&](const SolveResult& r) { real e = 0; for (const auto& sf : r.shellForces) e = FMath::Max(e, (real)FMath::Abs(sf.Nxx - NxxExp)); return e / NxxExp; };

    SolveOptions soOn; soOn.warpTolerance = 0.2; soOn.useWarpingCorrection = true;

    // (a) warped quad admitted + solves (warpTolerance relaxes validate); flat patch stays exact (best-fit).
    {
        FrameModel mw; buildWarpedPatch(mw, 0.03 * a);
        const SolveResult rw = solve(mw, soOn);
        TestFalse(TEXT("warped patch admitted + non-singular"), rw.singular);
        TestTrue(TEXT("warped patch bounded Nxx error (<10%)"), maxNxxErr(rw) < 0.1);
        FrameModel mf; buildWarpedPatch(mf, 0.0);
        const SolveResult rf = solve(mf, soOn);
        TestTrue(TEXT("flat patch exact constant Nxx (best-fit path)"), maxNxxErr(rf) < 1e-10);
    }

    // (b) warp error shrinks with the warp magnitude -> refine the mesh (smaller per-element warp) to converge.
    {
        FrameModel m4; buildWarpedPatch(m4, 0.04 * a);
        FrameModel m1; buildWarpedPatch(m1, 0.01 * a);
        const real e4 = maxNxxErr(solve(m4, soOn));
        const real e1 = maxNxxErr(solve(m1, soOn));
        TestTrue(TEXT("warp error shrinks with warp magnitude (refine to converge)"), e1 < e4);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
