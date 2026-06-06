// UE automation mirror of the standalone F1. Compiles only inside a host project
// (deferred). Re-proves the SAME solver path the standalone gate validates.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/ElasticAllowable.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreCantileverTest,
    "FrameCore.Cantilever.TipLoad",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreCantileverTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    Section  sec = Section::Rectangular(100.0, 100.0);
    Material mat(210000.0, 80769.0, 7850.0);
    mat.cap = Capacity::make(300.0, 300.0, 180.0);

    const real P = 1000.0, L = 2000.0;
    FrameModel m; fixtures::cantileverTipLoad(m, P, L, mat, sec);
    SolveResult r = solve(m);

    TestFalse(TEXT("not singular"), r.singular);
    const double dExp = P * L * L * L / (3.0 * mat.E * sec.Iz);
    TestTrue(TEXT("tip deflection = PL^3/3EI"),
             FMath::Abs(FMath::Abs(r.disp(1, Uz)) - dExp) <= 1e-6 * dExp);

    const MemberForcePair& mf = r.memberForces[0];
    const double Mroot = FMath::Sqrt(mf.endI.My * mf.endI.My + mf.endI.Mz * mf.endI.Mz);
    TestTrue(TEXT("root moment = PL"), FMath::Abs(Mroot - P * L) <= 1e-6 * (P * L));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
