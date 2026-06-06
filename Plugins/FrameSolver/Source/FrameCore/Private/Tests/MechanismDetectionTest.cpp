// UE automation mirror of the standalone F3 (deferred build).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreMechanismTest,
    "FrameCore.Mechanism.Detection",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreMechanismTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    Section  sec = Section::Rectangular(100.0, 100.0);
    Material mat(210000.0, 80769.0, 7850.0);

    FrameModel m; fixtures::mechanism(m, mat, sec);
    SolveResult r = solve(m);

    TestTrue(TEXT("mechanism detected (singular)"), r.singular);
    TestFalse(TEXT("diagnostic is non-empty"), r.diagnostic.empty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
