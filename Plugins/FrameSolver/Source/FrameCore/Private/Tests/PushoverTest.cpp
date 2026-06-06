// Plastic-hinge pushover automation, mirroring the standalone F9. A tip-loaded
// cantilever collapses after a single hinge at the fixed end; the collapse load factor
// must equal Mp/(P*L) and the mechanism must be detected (reusing releases + the pivot
// singularity check).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/Pushover.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCorePushoverTest, "FrameCore.Pushover.CantileverCollapse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCorePushoverTest::RunTest(const FString&)
{
	using namespace frame;
	Section  sec = Section::Rectangular(100.0, 100.0);
	Material mat(210000.0, 80769.0, 7850.0); mat.cap = Capacity::make(300.0, 300.0, 180.0);

	const real P = 1000.0, L = 2000.0;
	FrameModel m; fixtures::cantileverTipLoad(m, P, L, mat, sec);
	const PushoverResult pr = pushover(m, SolveOptions{}, 20);

	const real Mp = sec.Wz() * mat.cap.bend;
	const real expLam = Mp / (P * L);
	TestTrue(TEXT("pushover ok"), pr.ok);
	TestTrue(TEXT("collapse lambda = Mp/(P*L)"),
		FMath::Abs(pr.collapseLambda - expLam) < 1e-6 * expLam);
	TestTrue(TEXT("exactly one hinge -> mechanism"), pr.steps.size() == 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
