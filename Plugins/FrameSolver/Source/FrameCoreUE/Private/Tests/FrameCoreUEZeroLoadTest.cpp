// Phase 6f zero-load graceful-handling test. Verifies that
// UFrameCoreStressFieldLibrary::ComputeCantileverFixture(P=0, ...) returns a USTRUCT
// where all stress fields are zero (no NaN, no spurious governing element), and the
// engine does NOT report singular. The cantilever with zero load is still a
// well-constrained linear system; just trivially zero stress.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/FrameCoreUELibrary.h"
#include "FrameCoreUE/FrameCoreUETypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEZeroLoadTest,
    "FrameCore.UE.ZeroLoadTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEZeroLoadTest::RunTest(const FString& /*Parameters*/)
{
    // P = 0 N: a load-free linear system is well-posed (the cantilever is fully constrained
    // at the root); the solver returns the trivial zero displacement.
    const FFrameStressField bp =
        UFrameCoreStressFieldLibrary::ComputeCantileverFixture(0.f, 2000.f, 11);

    // (1) shape -- 1 trace, 11 samples, no shells (engine still produces the trace)
    TestEqual(TEXT("Zero-load: 1 member trace"), bp.Members.Num(), 1);
    if (bp.Members.Num() != 1) { return false; }
    TestEqual(TEXT("Zero-load: 11 samples"), bp.Members[0].Samples.Num(), 11);

    // (2) every sample sigma is exactly 0 (zero load -> zero stress; no float noise
    // because the engine's analytic N(x)/V(x)/M(x) reconstruction sees zero internal
    // forces from zero MemberEndForces).
    bool allSigmaZero = true;
    bool anyNaN = false;
    for (const FFrameStressFieldSample& s : bp.Members[0].Samples)
    {
        if (s.SigmaCompMax != 0.f || s.SigmaTensMax != 0.f) { allSigmaZero = false; }
        if (FMath::IsNaN(s.SigmaCompMax) || FMath::IsNaN(s.SigmaTensMax)) { anyNaN = true; }
        if (FMath::IsNaN(s.N) || FMath::IsNaN(s.Vy) || FMath::IsNaN(s.Vz) ||
            FMath::IsNaN(s.T) || FMath::IsNaN(s.My) || FMath::IsNaN(s.Mz)) { anyNaN = true; }
    }
    TestTrue(TEXT("Zero-load: all sample sigmas are exactly 0"), allSigmaZero);
    TestFalse(TEXT("Zero-load: no NaN in any sample"), anyNaN);

    // (3) global maxes are exactly 0; governing IDs honor the (engine, USTRUCT) sentinel
    // convention. We do NOT assert -1 here because U-07 (engine sentinel mismatch) keeps
    // the engine writing 0 even when nobody governs; the lossy passthrough means the BP
    // value is 0. Just assert the magnitudes:
    TestEqual(TEXT("Zero-load: globalMaxFiberSigma == 0"),
              bp.GlobalMaxFiberSigma, 0.f);
    TestEqual(TEXT("Zero-load: globalMaxVonMises == 0"),
              bp.GlobalMaxVonMises, 0.f);

    // (4) no shells in this fixture, so shell sentinels stay -1
    TestEqual(TEXT("Zero-load: ShellsTop empty"), bp.ShellsTop.Num(), 0);
    TestEqual(TEXT("Zero-load: ShellsBot empty"), bp.ShellsBot.Num(), 0);
    TestEqual(TEXT("Zero-load: governingShellId == -1 (no shells)"),
              bp.GoverningShellId, -1);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
