// Member-end release (truss/hinge) automation, mirroring the standalone F6/F7. The
// propped-cantilever case is the regression guard for the Qf-condensation bug: a
// loaded member with a released end must report ZERO moment there (not the fixed-end
// moment), and the fixed end must read the propped-cantilever wL^2/8.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreReleaseProppedTest, "FrameCore.Release.ProppedCantilever",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreReleaseProppedTest::RunTest(const FString&)
{
	using namespace frame;
	Section  sec = Section::Rectangular(100.0, 100.0);
	Material mat(210000.0, 80769.0, 7850.0); mat.cap = Capacity::make(300.0, 300.0, 180.0);

	const real w = 5.0, L = 4000.0;
	FrameModel m; fixtures::proppedCantileverRelease(m, w, L, mat, sec);
	SolveOptions opt; opt.enableReleases = true;
	const SolveResult r = solve(m, opt);

	TestFalse(TEXT("not singular"), r.singular);
	const double Mscale = w * L * L / 8.0;
	const double Mj = FMath::Sqrt(r.memberForces[1].endJ.My * r.memberForces[1].endJ.My
	                            + r.memberForces[1].endJ.Mz * r.memberForces[1].endJ.Mz);
	TestTrue(TEXT("released-end moment ~ 0 (Qf condensation)"), Mj < 1e-6 * Mscale);
	TestTrue(TEXT("fixed-end moment = wL^2/8"),
		FMath::Abs(FMath::Abs(r.memberForces[0].endI.Mz) - Mscale) < 1e-6 * Mscale);
	TestTrue(TEXT("reaction R_z(node0) = 5wL/8"),
		FMath::Abs(FMath::Abs(r.reaction(0, Uz)) - 5.0 * w * L / 8.0) < 1e-6 * (5.0 * w * L / 8.0));
	TestTrue(TEXT("reaction R_z(node2) = 3wL/8"),
		FMath::Abs(FMath::Abs(r.reaction(2, Uz)) - 3.0 * w * L / 8.0) < 1e-6 * (3.0 * w * L / 8.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreReleaseGuardTest, "FrameCore.Release.SingularGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreReleaseGuardTest::RunTest(const FString&)
{
	using namespace frame;
	Section  sec = Section::Rectangular(100.0, 100.0);
	Material mat(210000.0, 80769.0, 7850.0); mat.cap = Capacity::make(300.0, 300.0, 180.0);

	FrameModel m; fixtures::torsionReleaseMechanism(m, mat, sec);
	SolveOptions opt; opt.enableReleases = true;
	const SolveResult r = solve(m, opt);

	TestTrue(TEXT("singular detected"), r.singular);
	TestTrue(TEXT("diagnostic mentions release"),
		FString(r.diagnostic.c_str()).Contains(TEXT("release")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
