// UE automation mirror of the standalone F38 — momentum-preserving debris handoff. A portal frame
// brace trips at t=0+, the frame sways, and a chain hung off the moving corner is shaken until its
// link trips dynamically -> the chain detaches WHILE MOVING. The handoff vel/angVel must be finite,
// nonzero, and respect the x-z symmetry of the motion (vel_y ~ 0).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/DynamicCollapse.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreDynCollapseMomentumTest,
    "FrameCore.DynamicCollapse.MomentumHandoff",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreDynCollapseMomentumTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    const real E = 200000.0, Gs = 80000.0, rho = 7850.0, W = 3000.0, H = 3000.0, D = 2000.0, P = 150000.0;
    Section sec = Section::Rectangular(200.0, 200.0);
    Material strong(E, Gs, rho);   strong.cap   = Capacity::make(1e9, 1e9, 1e9);
    Material braceMat(E, Gs, rho); braceMat.cap = Capacity::make(0.5, 0.5, 1e9);
    Material linkMat(E, Gs, rho);  linkMat.cap  = Capacity::make(12.0, 12.0, 1e9);
    FrameModel m; m.materials = { strong, braceMat, linkMat }; m.sections = { sec };
    Node n0(0, 0, 0, 0); n0.fixAll();
    Node n1(1, W, 0, 0); n1.fixAll();
    Node n2(2, 0, 0, H); Node n3(3, W, 0, H);
    Node n4(4, 0, 0, H + D); Node n5(5, 0, 0, H + 2 * D);
    m.nodes   = { n0, n1, n2, n3, n4, n5 };
    m.members = { Member(0, 0, 2, 0, 0), Member(1, 1, 3, 0, 0), Member(2, 2, 3, 0, 0), Member(3, 0, 3, 1, 0),
                  Member(4, 2, 4, 2, 0), Member(5, 4, 5, 0, 0) };
    NodalLoad nl; nl.node = 2; nl.comp[Ux] = P; m.nodalLoads = { nl };

    DynCollapseOptions o; o.dt = 1e-5; o.maxTime = 600 * o.dt; o.basisSize = 200; o.useRitzVectors = false;
    o.screenEvery = 2; o.frameStride = 2; o.removeThreshold = 1.0; o.maxEvents = 6;
    const DynCollapseHistory h = runDynamicCollapse(m, o);

    int di = -1; for (size_t i = 0; i < h.events.size(); ++i) if (!h.events[i].detached.empty()) { di = (int)i; break; }
    TestTrue(TEXT("a fragment detached while moving"), di >= 0);
    if (di >= 0) {
        const FragmentCluster& fc = h.events[(size_t)di].detached[0];
        const real speed = FMath::Sqrt(dot(fc.vel, fc.vel));
        TestTrue(TEXT("fragment has mass"), fc.mass > 0);
        TestTrue(TEXT("handoff velocity finite & nonzero"), FMath::IsFinite(speed) && speed > 1e-9);
        TestTrue(TEXT("handoff respects x-z symmetry (vel_y ~ 0)"), FMath::Abs(fc.vel.y) < 1e-6 * speed);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
