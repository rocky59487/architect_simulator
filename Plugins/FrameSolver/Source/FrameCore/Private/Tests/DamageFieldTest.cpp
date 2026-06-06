// M8 #11 irreversible damage history (mirrors standalone F18): the per-element history
// variable H is the running max of the demand psi (Kuhn-Tucker), and the linear-softening
// damage d(H) is monotone non-decreasing — a partial unload (smaller psi) never reduces
// H, d, or the secant factor (no healing).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/DamageField.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreDamageFieldTest,
	"FrameCore.Damage.Monotonic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreDamageFieldTest::RunTest(const FString&)
{
	using namespace frame;
	using namespace frame::damage;

	DamageLaw law; law.kappa0 = 1.0; law.kappaf = 5.0;

	TestTrue(TEXT("d(kappa0) = 0"), FMath::Abs(damageOf(1.0, law) - 0.0) < 1e-12);
	TestTrue(TEXT("d(kappaf) = 1"), FMath::Abs(damageOf(5.0, law) - 1.0) < 1e-12);
	TestTrue(TEXT("d(2.0) = 0.625"), FMath::Abs(damageOf(2.0, law) - 0.625) < 1e-12);
	TestTrue(TEXT("d = 0 below onset"), damageOf(0.5, law) == 0.0);
	TestTrue(TEXT("d clamped to 1 above kappaf"), damageOf(9.0, law) == 1.0);

	DamageState st;
	const real seq[7] = { 0.5, 1.5, 1.2, 2.0, 0.3, 4.0, 1.0 };
	real prevD = -1.0; bool monotone = true;
	real dPeak2 = 0, dUnload2 = 0, dPeak4 = 0, dUnload4 = 0;
	for (int k = 0; k < 7; ++k)
	{
		const real d = st.update(seq[k], law);
		if (d < prevD - 1e-15) monotone = false;
		prevD = d;
		if (k == 3) dPeak2 = d;
		if (k == 4) dUnload2 = d;
		if (k == 5) dPeak4 = d;
		if (k == 6) dUnload4 = d;
	}
	TestTrue(TEXT("damage monotone non-decreasing"), monotone);
	TestTrue(TEXT("H = running max (= 4.0)"), FMath::Abs(st.H - 4.0) < 1e-12);
	TestTrue(TEXT("unloading does not heal (0.3 after 2.0)"), dUnload2 == dPeak2);
	TestTrue(TEXT("unloading does not heal (1.0 after 4.0)"), dUnload4 == dPeak4);
	TestTrue(TEXT("peak damage d(H=4) = 0.9375"), FMath::Abs(dPeak4 - 0.9375) < 1e-12);
	TestTrue(TEXT("secant factor (1-d)"), FMath::Abs(st.secant(law) - (1.0 - 0.9375)) < 1e-12);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
