// Phase 2 BP smoke test for FrameCoreUE — drives the UBlueprintFunctionLibrary entry
// `UFrameCoreStressFieldLibrary::ComputeCantileverFixture` and verifies the marshalled
// USTRUCT carries the same numbers (within float-lossy budget) as the engine POD POD
// produced by `frame::computeStressField` on an equivalent fixture.
//
// Two oracles:
//   (a) C++ POD cross-check — call the engine directly on the same fixture, compare
//       sample[0].SigmaCompMax against POD .sigmaCompMax at rel<1e-5 (float-lossy budget;
//       engine-side double->USTRUCT float is the only divergence path)
//   (b) Analytic |P|*(L-x)/Wz at 11 samples — rel<1e-4 (looser than (a) because we now
//       also pay analytic-vs-numerical roundoff plus the float lossy cast)
//
// The fixture mirrors F68 standalone (cantileverTipLoad, 100x100 rectangular, +Z tip load)
// so any drift against the v3.1.0 oracle would also show up against StressFieldTest.

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

namespace {

// Local copy of the fixture used by FrameCoreUELibrary::BuildCantileverFixture so the
// test has its own engine-POD reference without reaching into Library.cpp internals.
frame::FrameModel BuildCantileverFixtureLocal(double P, double L)
{
    using namespace frame;
    Material mat(210000.0, 80769.0, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);
    Section sec = Section::Rectangular(100.0, 100.0);

    FrameModel m;
    m.materials = { mat };
    m.sections  = { sec };

    Node n0(0, 0, 0, 0); n0.fixAll();
    Node n1(1, L, 0, 0);
    m.nodes   = { n0, n1 };
    m.members = { Member(0, 0, 1, 0, 0) };

    NodalLoad nl;
    nl.node     = 1;
    nl.comp[Uz] = P;
    m.nodalLoads = { nl };

    return m;
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEBlueprintSmokeTest,
    "FrameCore.UE.BlueprintSmokeTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEBlueprintSmokeTest::RunTest(const FString& /*Parameters*/)
{
    const float P = 1000.f;
    const float L = 2000.f;
    const int32 N = 11;

    // (1) BP entry point — exact path a BP designer would hit.
    const FFrameStressField BpField =
        UFrameCoreStressFieldLibrary::ComputeCantileverFixture(P, L, N);

    TestEqual(TEXT("BP marshal: 1 member trace"), BpField.Members.Num(), 1);
    if (BpField.Members.Num() != 1) { return false; }
    TestEqual(TEXT("BP marshal: 11 samples per span"),
              BpField.Members[0].Samples.Num(), N);

    // Governing member id propagates. v3.2 audit A-2 (MEDIUM) note: the engine POD
    // convention is `0 if no member governs` (StressField.h L78); the USTRUCT default
    // v3.3 (U-07): unified -1 sentinel + INDEX semantics. The cantilever fixture has
    // its sole member at slot 0, so the governing index is 0. The per-member record's
    // MemberId (also 0 here, by fixture coincidence) is recoverable via lookup. The
    // two values are no longer aliased.
    TestTrue(TEXT("BP marshal: someone governs (idx >= 0)"),
             UFrameCoreStressFieldLibrary::GetGoverningMemberIdx(BpField) >= 0);
    TestEqual(TEXT("BP marshal: governing member idx == 0 (cantilever slot 0)"),
              UFrameCoreStressFieldLibrary::GetGoverningMemberIdx(BpField), 0);
    TestEqual(TEXT("BP marshal: per-member record carries the user id (== 0 by fixture)"),
              BpField.Members.Num() > 0 ? BpField.Members[0].MemberId : -999, 0);
    TestTrue(TEXT("BP marshal: globalMaxFiberSigma > 0"),
             UFrameCoreStressFieldLibrary::GetGlobalMaxFiberSigma(BpField) > 0.f);

    // (2) Oracle (a) — engine POD cross-check at sample[0]. Budget rel<1e-5 (only float
    // lossy from engine-side double; engine-side is bit-identical to F68).
    const frame::FrameModel mRef = BuildCantileverFixtureLocal((double)P, (double)L);
    const frame::SolveResult rRef = frame::solve(mRef);
    TestFalse(TEXT("oracle: ref solve not singular"), rRef.singular);
    const frame::StressField fldRef = frame::computeStressField(mRef, rRef, N);

    if (fldRef.members.empty()) { return false; }
    const double sigPodRoot = (double)fldRef.members[0].samples.front().sigmaCompMax;
    const double sigBpRoot  = (double)BpField.Members[0].Samples[0].SigmaCompMax;
    const double relRoot    =
        FMath::Abs(sigBpRoot - sigPodRoot) / FMath::Max(sigPodRoot, 1e-12);
    TestTrue(TEXT("oracle (a): BP sample[0].SigmaCompMax == POD .sigmaCompMax (rel<1e-5)"),
             relRoot < 1e-5);

    // (3) Oracle (b) — analytic |P|*(L-x)/Wz at 11 samples; tip is sigExp=0, so fall
    // back to absolute diff there (matches v3.1.0 F68 audit fix — never scale machine
    // epsilon noise by a floor). Budget rel<1e-4 (analytic + float cast).
    const frame::Section secRef = mRef.sections[0];
    const double Wz = (double)secRef.Wz();
    double worstRel = 0;
    for (int32 k = 0; k < BpField.Members[0].Samples.Num(); ++k)
    {
        const double xk      = (double)BpField.Members[0].Samples[k].X;
        const double sigExp  = FMath::Abs((double)P) * ((double)L - xk) / Wz;
        const double sigGot  = (double)BpField.Members[0].Samples[k].SigmaCompMax;
        const double rel     = (sigExp > 0)
            ? FMath::Abs(sigGot - sigExp) / sigExp
            : FMath::Abs(sigGot - sigExp);
        if (rel > worstRel) { worstRel = rel; }
    }
    TestTrue(TEXT("oracle (b): analytic |P|*(L-x)/Wz at 11 samples (rel<1e-4 float-lossy)"),
             worstRel < 1e-4);

    // (4) BP convenience accessors return the same data.
    const TArray<FFrameStressFieldSample> Samples =
        UFrameCoreStressFieldLibrary::GetMemberSamples(BpField, 0);
    TestEqual(TEXT("BP GetMemberSamples: 11 entries"), Samples.Num(), N);
    if (Samples.Num() > 0)
    {
        TestEqual(TEXT("BP GetMemberSamples: sample[0].X == 0"), Samples[0].X, 0.f);
    }

    // Out-of-range guard.
    const TArray<FFrameStressFieldSample> Empty =
        UFrameCoreStressFieldLibrary::GetMemberSamples(BpField, 42);
    TestEqual(TEXT("BP GetMemberSamples: out-of-range returns empty"), Empty.Num(), 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
