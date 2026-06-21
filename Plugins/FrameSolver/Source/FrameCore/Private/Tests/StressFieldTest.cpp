// UE automation mirror of standalone F68/F70. Re-proves the StressField numerical
// layer (StressKernel.h single source of truth) under the UE build path.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/ElasticAllowable.h"
#include "FrameCore/StressField.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreStressFieldTest,
    "FrameCore.StressField.MemberOracle",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreStressFieldTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    Section  sec = Section::Rectangular(100.0, 100.0);
    Material mat(210000.0, 80769.0, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);

    const real P = 1000.0, L = 2000.0;
    FrameModel m; fixtures::cantileverTipLoad(m, P, L, mat, sec);
    SolveResult r = solve(m);
    TestFalse(TEXT("not singular"), r.singular);

    const StressField fld = computeStressField(m, r, 11);
    TestEqual(TEXT("one trace"), (int)fld.members.size(), 1);
    if (fld.members.empty()) return false;
    const MemberStressTrace& tr = fld.members[0];
    TestEqual(TEXT("11 samples per span"), (int)tr.samples.size(), 11);

    // D/C interlock at the root: StressField sigmaCompMax at sample[0] must equal
    // ElasticAllowable::checkSection(endI).sComp bit-exact (shared StressKernel).
    ElasticAllowable screen;
    const DemandResult dI = screen.checkSection(r.memberForces[0].endI, sec, mat.cap);
    const double sigRoot = (double)tr.samples.front().sigmaCompMax;
    const double sigExp  = (double)dI.sComp;
    TestTrue(TEXT("sample[0].sigmaCompMax == ElasticAllowable(endI).sComp (rel<1e-12)"),
             FMath::Abs(sigRoot - sigExp) <= 1e-12 * FMath::Max(sigExp, 1e-12));

    // Analytic |M(x)| = P*(L-x), sigma = |M|/Wz over 11 samples (worst rel < 1e-9).
    // Tip sample at x = L has sigExpK = 0; fall back to absolute diff there so we don't
    // scale machine-epsilon noise by a floor and falsely fail. Matches standalone F68.
    const real Wz = sec.Wz();
    double worstRel = 0;
    for (const MemberStressSample& s : tr.samples) {
        const double xk = (double)s.x;
        const double sigExpK = (double)FMath::Abs((double)P) * ((double)L - xk) / (double)Wz;
        const double sigGotK = (double)s.sigmaCompMax;
        const double rel = (sigExpK > 0) ? FMath::Abs(sigGotK - sigExpK) / sigExpK
                                         : FMath::Abs(sigGotK - sigExpK);
        if (rel > worstRel) worstRel = rel;
    }
    TestTrue(TEXT("analytic sigma(x) = |P|*(L-x)/Wz at 11 samples (rel<1e-9)"),
             worstRel < 1e-9);

    // Governing id propagates.
    TestEqual(TEXT("governingMemberId == 0"), (int)fld.governingMemberId, 0);
    TestTrue(TEXT("globalMaxFiberSigma > 0"), fld.globalMaxFiberSigma > 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
