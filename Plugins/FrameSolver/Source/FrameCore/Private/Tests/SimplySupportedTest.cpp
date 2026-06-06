// UE automation mirror of the standalone F2 (deferred build).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreSimplySupportedTest,
    "FrameCore.SimplySupported.UDL",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreSimplySupportedTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    Section  sec = Section::Rectangular(100.0, 100.0);
    Material mat(210000.0, 80769.0, 7850.0);

    const real w = 5.0, L = 3000.0;
    FrameModel m; fixtures::simplySupportedUDL(m, w, L, mat, sec);
    SolveResult r = solve(m);

    TestFalse(TEXT("not singular"), r.singular);
    const double dExp = 5.0 * w * L * L * L * L / (384.0 * mat.E * sec.Iz);
    TestTrue(TEXT("midspan deflection = 5wL^4/384EI"),
             FMath::Abs(FMath::Abs(r.disp(1, Uz)) - dExp) <= 1e-6 * dExp);

    const double Mmid = FMath::Abs(r.memberForces[0].endJ.Mz);
    TestTrue(TEXT("midspan moment = wL^2/8"),
             FMath::Abs(Mmid - w * L * L / 8.0) <= 1e-5 * (w * L * L / 8.0));
    TestTrue(TEXT("support reaction = wL/2"),
             FMath::Abs(FMath::Abs(r.reaction(0, Uz)) - w * L / 2.0) <= 1e-6 * (w * L / 2.0));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
