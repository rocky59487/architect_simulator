// UE automation mirror of the standalone F37 — full-basis self-consistency of the dynamic-collapse
// driver. A portal frame whose brace trips at t=0+ free-vibrates; with a FULL basis the Ritz path
// and the pure-eigenmode path span the same space, so every replay frame agrees to round-off and
// the per-event truncation residual vanishes (the inheritance projection is exact).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/DynamicCollapse.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreDynCollapseEquivalenceTest,
    "FrameCore.DynamicCollapse.InheritanceEquivalence",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreDynCollapseEquivalenceTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    const real E = 200000.0, Gs = 80000.0, rho = 7850.0, W = 3000.0, Hh = 3000.0, P = 50000.0;
    Section sec = Section::Rectangular(200.0, 200.0);
    Material strong(E, Gs, rho);   strong.cap   = Capacity::make(1e9, 1e9, 1e9);
    Material braceMat(E, Gs, rho); braceMat.cap = Capacity::make(0.5, 0.5, 1e9);
    FrameModel m; m.materials = { strong, braceMat }; m.sections = { sec };
    Node n0(0, 0, 0, 0); n0.fixAll();
    Node n1(1, W, 0, 0); n1.fixAll();
    Node n2(2, 0, 0, Hh); Node n3(3, W, 0, Hh);
    m.nodes   = { n0, n1, n2, n3 };
    m.members = { Member(0, 0, 2, 0, 0), Member(1, 1, 3, 0, 0), Member(2, 2, 3, 0, 0), Member(3, 0, 3, 1, 0) };
    NodalLoad nl; nl.node = 2; nl.comp[Ux] = P; m.nodalLoads = { nl };

    auto run = [&](bool ritz) {
        DynCollapseOptions o; o.dt = 1e-5; o.maxTime = 40 * o.dt; o.basisSize = 200;
        o.useRitzVectors = ritz; o.screenEvery = 1; o.frameStride = 1; o.removeThreshold = 1.0; o.maxEvents = 3;
        return runDynamicCollapse(m, o);
    };
    const DynCollapseHistory hm = run(false), hr = run(true);

    TestTrue(TEXT("brace tripped first"), !hm.events.empty() && hm.events[0].removedMembers.size() == 1 && hm.events[0].removedMembers[0] == 3);
    TestTrue(TEXT("same frame count (Ritz vs modes)"), hm.frames.size() == hr.frames.size());
    real maxTrunc = 0; for (const auto& ev : hm.events) maxTrunc = FMath::Max(maxTrunc, ev.truncationResidual);
    TestTrue(TEXT("full-basis zero truncation residual"), maxTrunc < 1e-9);
    double num = 0.0, den = 1e-30;
    for (size_t k = 0; k < hm.frames.size() && k < hr.frames.size(); ++k)
        for (size_t i = 0; i < hm.frames[k].u.size() && i < hr.frames[k].u.size(); ++i) {
            num = FMath::Max(num, FMath::Abs(hm.frames[k].u[i] - hr.frames[k].u[i]));
            den = FMath::Max(den, FMath::Abs(hm.frames[k].u[i]));
        }
    TestTrue(TEXT("Ritz frames == mode frames (full basis)"), num / den < 1e-8);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
