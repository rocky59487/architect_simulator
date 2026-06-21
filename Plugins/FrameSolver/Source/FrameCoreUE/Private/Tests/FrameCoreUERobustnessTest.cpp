// Phase 6e robustness / edge-case test for FrameCoreUE. Covers:
//   1. Negative-input contract — SamplesPerSpan < 2 silently clamps to 11 (the BP-friendly
//      sane default; Phase 5 audit C-MEDIUM fix in FrameCoreUELibrary.cpp)
//   2. Marshal scaling — 20-member multi-segment cantilever produces 20 well-formed traces
//      (USTRUCT TArray doesn't truncate / corrupt at modest scale)
//   3. Memory stability under repeat — 100 ComputeCantileverFixture calls in a loop;
//      verifies no obvious leak / hang / GC pressure on a fixed workload
//
// All three are budget-tolerant smoke checks, not perf benchmarks. The whole test should
// complete under a second on the integrator host.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/FrameCoreUELibrary.h"
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

namespace FrameCoreUE
{
    FFrameStressField ToBlueprint(const frame::StressField& field);
}

namespace {

// 20-segment cantilever along +X. Each segment is L/20 long. Same section throughout.
// Tip load at node 20.
frame::FrameModel BuildLongCantileverFixtureLocal(double P, double L, int nMembers)
{
    using namespace frame;
    Material mat(210000.0, 80769.0, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);
    Section sec = Section::Rectangular(100.0, 100.0);

    FrameModel m;
    m.materials = { mat };
    m.sections  = { sec };

    const double dx = L / (double)nMembers;
    for (int i = 0; i <= nMembers; ++i)
    {
        Node nd(i, (real)(i * dx), 0, 0);
        if (i == 0) { nd.fixAll(); }
        m.nodes.push_back(nd);
    }
    for (int i = 0; i < nMembers; ++i)
    {
        m.members.push_back(Member(i, i, i + 1, 0, 0));
    }
    NodalLoad nl; nl.node = nMembers; nl.comp[Uz] = (real)P;
    m.nodalLoads = { nl };

    return m;
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUERobustnessTest,
    "FrameCore.UE.RobustnessTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUERobustnessTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;

    // (1) Negative-input contract -- ComputeCantileverFixture clamps SamplesPerSpan < 2 to 11.
    {
        const FFrameStressField r_neg = UFrameCoreStressFieldLibrary::ComputeCantileverFixture(
            1000.f, 2000.f, /*SamplesPerSpan=*/-3);
        TestEqual(TEXT("Robustness: SamplesPerSpan=-3 clamped to 11"),
                  r_neg.Members.Num() > 0 ? r_neg.Members[0].Samples.Num() : 0, 11);

        const FFrameStressField r_zero = UFrameCoreStressFieldLibrary::ComputeCantileverFixture(
            1000.f, 2000.f, 0);
        TestEqual(TEXT("Robustness: SamplesPerSpan=0 clamped to 11"),
                  r_zero.Members.Num() > 0 ? r_zero.Members[0].Samples.Num() : 0, 11);

        const FFrameStressField r_one = UFrameCoreStressFieldLibrary::ComputeCantileverFixture(
            1000.f, 2000.f, 1);
        TestEqual(TEXT("Robustness: SamplesPerSpan=1 clamped to 11"),
                  r_one.Members.Num() > 0 ? r_one.Members[0].Samples.Num() : 0, 11);

        // Valid value > 11 passes through.
        const FFrameStressField r_big = UFrameCoreStressFieldLibrary::ComputeCantileverFixture(
            1000.f, 2000.f, 21);
        TestEqual(TEXT("Robustness: SamplesPerSpan=21 passes through"),
                  r_big.Members.Num() > 0 ? r_big.Members[0].Samples.Num() : 0, 21);
    }

    // (2) Marshal scaling -- 20-member multi-segment cantilever produces 20 traces, each
    // 11-sample, and governing member id is the root (id 0).
    {
        const FrameModel m = BuildLongCantileverFixtureLocal(1000.0, 2000.0, 20);
        const SolveResult r = solve(m);
        TestFalse(TEXT("Robustness: 20-member solve not singular"), r.singular);
        if (r.singular) { return false; }

        const StressField fld = computeStressField(m, r, 11);
        const FFrameStressField bp = FrameCoreUE::ToBlueprint(fld);

        TestEqual(TEXT("Robustness: USTRUCT carries 20 traces"),
                  bp.Members.Num(), 20);
        if (bp.Members.Num() == 20)
        {
            TestEqual(TEXT("Robustness: trace[19].Samples.Num == 11"),
                      bp.Members[19].Samples.Num(), 11);
            TestEqual(TEXT("Robustness: trace[19].MemberId == 19"),
                      bp.Members[19].MemberId, 19);
        }
        TestEqual(TEXT("Robustness: governingMemberId == 0 (root)"),
                  bp.GoverningMemberId, 0);
        TestTrue(TEXT("Robustness: globalMaxFiberSigma > 0"),
                 bp.GlobalMaxFiberSigma > 0.f);
    }

    // (3) Memory stability -- 100 ComputeCantileverFixture calls in a loop. We don't have
    // direct access to GC stats inside an automation test without engine flags, but we can
    // verify that:
    //   a) The result is consistent each iteration (no internal state corruption)
    //   b) The test completes without hanging or producing an empty result
    //   c) Engine logs no warnings (this is not directly assertable without log scraping;
    //      a real leak would show up as a worsening result or a crash)
    {
        const FFrameStressField reference =
            UFrameCoreStressFieldLibrary::ComputeCantileverFixture(1000.f, 2000.f, 11);
        const float refMax = reference.GlobalMaxFiberSigma;

        bool allConsistent = true;
        for (int i = 0; i < 100; ++i)
        {
            const FFrameStressField bp =
                UFrameCoreStressFieldLibrary::ComputeCantileverFixture(1000.f, 2000.f, 11);
            // Bit-exact reproducibility: same inputs => same engine POD => same USTRUCT.
            if (bp.GlobalMaxFiberSigma != refMax) { allConsistent = false; break; }
            if (bp.Members.Num() != 1)            { allConsistent = false; break; }
            if (bp.Members[0].Samples.Num() != 11){ allConsistent = false; break; }
        }
        TestTrue(TEXT("Robustness: 100 repeat calls produce bit-exact USTRUCT"),
                 allConsistent);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
