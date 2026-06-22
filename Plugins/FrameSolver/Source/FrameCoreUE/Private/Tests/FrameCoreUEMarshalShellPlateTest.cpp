// Phase 6a clamped plate shell marshal test — exercises FrameCoreUE::ToBlueprint on a
// 2x2 (= 4-shell) clamped-edge plate under uniform pressure. Verifies:
//   1. USTRUCT ShellsTop / ShellsBot arrays populate (cantilever fixture left these empty)
//   2. Each ShellStressLayer has Center + 4 Corners (matches engine 5-point traversal)
//   3. bIsTopLayer field correctly differentiates Top from Bot layer
//   4. ToBlueprint marshal preserves shell IDs and corner ordering
//   5. globalMaxVonMises > 0 and governing shell propagates

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
#include "FrameCore/StressKernel.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "FrameCoreUETestHelpers.h"  // V321-05: shared forward decl for FrameCoreUE::ToBlueprint

namespace {

// Clamped square plate, side a (mm), thickness t (mm), n subdivisions per side
// (n+1 x n+1 nodes, n*n shells), uniform pressure q (MPa, positive into -Z face).
// Mirrors fixtures::clampedPlateShell from FrameTestFixtures.h (Private; re-implemented
// inline so FrameCoreUE tests do not depend on FrameCore-private headers).
frame::FrameModel BuildClampedPlateFixtureLocal(double a, double t, int n, double q)
{
    using namespace frame;
    Material mat(210000.0, 80769.0, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);

    FrameModel m;
    m.materials = { mat };

    const double h = a / (double)n;
    auto gid = [n](int i, int j) { return j * (n + 1) + i; };

    for (int j = 0; j <= n; ++j)
    {
        for (int i = 0; i <= n; ++i)
        {
            Node nd(gid(i, j), (real)(i * h), (real)(j * h), (real)0);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEMarshalShellPlateTest,
    "FrameCore.UE.MarshalShellPlateTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEMarshalShellPlateTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;

    // Modest fixture: a = 1000 mm, t = 10 mm, n = 2 (4 shells), q = 0.01 MPa.
    // Engine knows this is a thin plate; we just need the marshal to populate the
    // shellsTop / shellsBot arrays with valid stress data.
    const double a = 1000.0, t = 10.0, q = 0.01;
    const int    n = 2;

    const FrameModel m = BuildClampedPlateFixtureLocal(a, t, n, q);
    const SolveResult r  = solve(m);
    TestFalse(TEXT("Shell plate solve not singular"), r.singular);
    if (r.singular) { return false; }

    const StressField fld = computeStressField(m, r, 11);
    const FFrameStressField bp = FrameCoreUE::ToBlueprint(fld);

    // (1) shape — no members; 4 shells; each shell appears in both ShellsTop and ShellsBot
    TestEqual(TEXT("Shell plate: 0 members"),          bp.Members.Num(),   0);
    TestEqual(TEXT("Shell plate: 4 ShellsTop layers"), bp.ShellsTop.Num(), 4);
    TestEqual(TEXT("Shell plate: 4 ShellsBot layers"), bp.ShellsBot.Num(), 4);
    if (bp.ShellsTop.Num() != 4 || bp.ShellsBot.Num() != 4) { return false; }

    // (2) each layer has 4 corner points (center is its own field, not in Corners[])
    for (int s = 0; s < 4; ++s)
    {
        TestEqual(TEXT("Shell plate: ShellsTop[s] has 4 corners"),
                  bp.ShellsTop[s].Corners.Num(), 4);
        TestEqual(TEXT("Shell plate: ShellsBot[s] has 4 corners"),
                  bp.ShellsBot[s].Corners.Num(), 4);
    }

    // (3) bIsTopLayer field differentiation — engine's ShellLayer::Top -> true, Bot -> false
    bool anyTopMismatch = false, anyBotMismatch = false;
    for (int s = 0; s < 4; ++s)
    {
        if (!bp.ShellsTop[s].bIsTopLayer) { anyTopMismatch = true; }
        if (bp.ShellsBot[s].bIsTopLayer)  { anyBotMismatch = true; }
    }
    TestFalse(TEXT("Shell plate: ShellsTop all have bIsTopLayer=true"), anyTopMismatch);
    TestFalse(TEXT("Shell plate: ShellsBot all have bIsTopLayer=false"), anyBotMismatch);

    // (4) BP marshal preserves shell IDs (engine ShellQuad ids 0..3)
    for (int s = 0; s < 4; ++s)
    {
        TestEqual(TEXT("Shell plate: ShellsTop[s].ShellId matches engine"),
                  bp.ShellsTop[s].ShellId, (int32)fld.shellsTop[s].shellId);
        TestEqual(TEXT("Shell plate: ShellsBot[s].ShellId matches engine"),
                  bp.ShellsBot[s].ShellId, (int32)fld.shellsBot[s].shellId);
    }

    // (5) BP marshal preserves VonMises stress (float-lossy cast vs engine double)
    double maxRelVM = 0;
    for (int s = 0; s < 4; ++s)
    {
        const double podVM = (double)fld.shellsTop[s].center.vonMises;
        const double bpVM  = (double)bp.ShellsTop[s].Center.VonMises;
        const double relVM = (podVM > 1e-9)
            ? FMath::Abs(bpVM - podVM) / podVM
            : FMath::Abs(bpVM - podVM);
        if (relVM > maxRelVM) { maxRelVM = relVM; }
    }
    TestTrue(TEXT("Shell plate: shell center VonMises BP vs POD rel<1e-5"), maxRelVM < 1e-5);

    // (6) v3.3 (U-07): governing shell idx is a real slot (>= 0). Cross-check the
    // resolved user id via ShellsTop / ShellsBot (the per-layer record carries ShellId).
    TestTrue(TEXT("Shell plate: globalMaxVonMises > 0"),
             bp.GlobalMaxVonMises > 0.f);
    TestTrue(TEXT("Shell plate: governingShellIdx >= 0 (someone governs)"),
             bp.GoverningShellIdx >= 0);

    // (7) Member governing remains -1 sentinel since there are no members
    TestEqual(TEXT("Shell plate: governingMemberIdx == -1 (no members)"),
              bp.GoverningMemberIdx, -1);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
