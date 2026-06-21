// Phase 6f Principal-stress theta range invariant test. Verifies that every
// FFrameShellStressPoint.ThetaRad falls inside (-π/2, π/2] as documented in
// frame::StressKernel.h (v3.1.0 audit A-09 comment fix). atan2-based principal axis
// returns a value in (-π/2, π/2] for any well-defined Mohr's circle; the marshal must
// preserve that range.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/FrameCoreUETypes.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Shell.h"
#include "FrameCore/Material.h"
#include "FrameCore/Load.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/StressField.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FrameCoreUE
{
    FFrameStressField ToBlueprint(const frame::StressField& field);
}

namespace {

// Tilted clamped plate (orientation provoking non-trivial principal angle).
// 3x3 grid of nodes, 2x2 = 4 shells, clamped at edges, uniform pressure.
frame::FrameModel BuildTiltedPlateFixtureLocal(double a, double t, double q)
{
    using namespace frame;
    Material mat(210000.0, 80769.0, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);

    FrameModel m;
    m.materials = { mat };

    const int n = 2;  // 3x3 nodes, 2x2 shells
    const double h = a / (double)n;

    auto gid = [n](int i, int j) { return j * (n + 1) + i; };

    for (int j = 0; j <= n; ++j)
    {
        for (int i = 0; i <= n; ++i)
        {
            Node nd(gid(i, j), (real)(i * h), (real)(j * h), 0);
            const bool edge = (i == 0 || i == n || j == 0 || j == n);
            if (edge) { nd.fixAll(); }
            m.nodes.push_back(nd);
        }
    }

    int sid = 0;
    for (int j = 0; j < n; ++j)
    {
        for (int i = 0; i < n; ++i)
        {
            m.shells.push_back(ShellQuad(sid,
                gid(i, j), gid(i + 1, j),
                gid(i + 1, j + 1), gid(i, j + 1),
                0, (real)t));
            ShellPressure sp; sp.shell = sid; sp.p = (real)(-q);
            m.shellPressures.push_back(sp);
            ++sid;
        }
    }

    return m;
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEThetaRangeTest,
    "FrameCore.UE.ThetaRangeTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEThetaRangeTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;

    const FrameModel m = BuildTiltedPlateFixtureLocal(1000.0, 10.0, 0.01);
    const SolveResult r  = solve(m);
    TestFalse(TEXT("Tilted plate solve not singular"), r.singular);
    if (r.singular) { return false; }

    const StressField fld = computeStressField(m, r, 11);
    const FFrameStressField bp = FrameCoreUE::ToBlueprint(fld);

    // Sweep all shell sample points (4 shells × {center + 4 corners} × {Top, Bot}).
    // Every ThetaRad must satisfy -π/2 < theta <= π/2 (PrincipalStress contract;
    // atan2-based axis cannot return -π/2 since that maps to +π/2).
    const float halfPi = (float)(3.14159265358979323846 / 2.0);
    const float epsTol = 1e-5f;  // accommodate float roundoff at boundary

    int totalChecked = 0;
    bool allInRange = true;
    auto checkThetaRange = [&](const FFrameShellStressPoint& p)
    {
        ++totalChecked;
        if (p.ThetaRad <= -halfPi - epsTol || p.ThetaRad > halfPi + epsTol)
        {
            allInRange = false;
        }
    };

    for (int s = 0; s < bp.ShellsTop.Num(); ++s)
    {
        checkThetaRange(bp.ShellsTop[s].Center);
        for (int c = 0; c < bp.ShellsTop[s].Corners.Num(); ++c) { checkThetaRange(bp.ShellsTop[s].Corners[c]); }
    }
    for (int s = 0; s < bp.ShellsBot.Num(); ++s)
    {
        checkThetaRange(bp.ShellsBot[s].Center);
        for (int c = 0; c < bp.ShellsBot[s].Corners.Num(); ++c) { checkThetaRange(bp.ShellsBot[s].Corners[c]); }
    }

    // 4 shells × 5 points × 2 layers = 40 points
    TestEqual(TEXT("Theta range: 40 shell sample points swept"), totalChecked, 40);
    TestTrue(TEXT("Theta range: every ThetaRad in (-π/2, π/2]"), allInRange);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
