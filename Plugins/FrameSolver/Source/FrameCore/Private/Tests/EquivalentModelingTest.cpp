// M7 #5 equivalent modeling (mirrors standalone F13): ACI effective flange width,
// composite T-section properties, equivalent diagonal-brace area round-trip, and the
// tributary load-conservation building block.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/EquivalentModeling.h"
#include "FrameCore/Section.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreEquivalentModelingTest,
	"FrameCore.Equivalent.Modeling",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreEquivalentModelingTest::RunTest(const FString&)
{
	using namespace frame;
	using namespace frame::equiv;

	// ACI 318-19 Table 6.3.2.1: be = 300 + 2*min(8*100, 2000/2, 6000/8) = 300 + 2*750 = 1800
	const real be = effectiveFlangeWidthACI(300.0, 100.0, 2000.0, 6000.0);
	TestTrue(TEXT("ACI effective flange be = 1800"), FMath::Abs(be - 1800.0) < 1e-9);

	const Section tee = compositeTeeSection(300.0, 600.0, be, 100.0);
	TestTrue(TEXT("T-section area = 330000"), FMath::Abs(tee.A - 330000.0) < 1e-3);
	TestTrue(TEXT("T-section extreme fibre cz ~ 413.636"), FMath::Abs(tee.cz - 413.6364) < 1e-2);
	TestTrue(TEXT("T-section composite Iz ~ 1.06386e10"),
		FMath::Abs(tee.Iz - 1.0638636e10) < 1e-4 * 1.0638636e10);
	TestTrue(TEXT("composite I > bare web I"), tee.Iz > Section::Rectangular(300.0, 600.0).Iz);

	// equivalent brace: A = K Ld^3 / (E L^2); recomputing K from A must round-trip
	const real E = 210000.0, L = 4000.0, H = 3000.0, Ktar = 5000.0;
	const real A = equivalentBraceArea(Ktar, E, L, H);
	const real Ld = std::sqrt(L * L + H * H);
	TestTrue(TEXT("equivalent brace K round-trip"),
		FMath::Abs(E * A * L * L / (Ld * Ld * Ld) - Ktar) < 1e-6 * Ktar);

	// tributary load is conserved (sum of widths == panel width)
	const real q = 0.01, w1 = 2000.0, w2 = 4000.0;
	TestTrue(TEXT("tributary load conservation"),
		FMath::Abs((tributaryLineLoad(q, w1) + tributaryLineLoad(q, w2)) - q * (w1 + w2)) < 1e-9);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
