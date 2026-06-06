// FiberSection RC P-M capacity automation, mirroring the standalone F8. Validates the
// fiber integration against the Whitney closed form (pure-moment Mn, pure-axial P0).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FiberSection.h"
#include "FrameCore/Section.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreFiberRCTest, "FrameCore.SectionStrength.FiberRC",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreFiberRCTest::RunTest(const FString&)
{
	using namespace frame;
	RCSectionParams rc; rc.fc = 28.0; rc.fy = 420.0; rc.Es = 200000.0; rc.cover = 60.0;
	rc.AsTop = 0.0; rc.AsBot = 1000.0;                    // singly reinforced
	Section sec = Section::Rectangular(300.0, 500.0);     // b=300, d=500
	FiberSection fib(rc);
	const Capacity dummy = Capacity::make(1.0, 1.0, 1.0);

	const double dEff = 500.0 - 60.0;
	const double aW   = rc.AsBot * rc.fy / (0.85 * rc.fc * 300.0);
	const double MnW  = rc.AsBot * rc.fy * (dEff - aW / 2.0);   // Whitney Mn

	MemberEndForces fm; fm.Mz = MnW;
	const DemandResult d1 = fib.checkSection(fm, sec, dummy);
	TestTrue(TEXT("D/C at M = Whitney Mn ~ 1.0"), FMath::Abs(d1.risk - 1.0) < 5e-3);
	TestTrue(TEXT("governing mode is Bending"), d1.mode == FailMode::Bending);

	MemberEndForces fh; fh.Mz = 0.5 * MnW;
	TestTrue(TEXT("D/C at M = Mn/2 ~ 0.5"), FMath::Abs(fib.checkSection(fh, sec, dummy).risk - 0.5) < 5e-3);

	const double Ast = rc.AsTop + rc.AsBot;
	const double P0  = 0.85 * rc.fc * (300.0 * 500.0 - Ast) + rc.fy * Ast;
	MemberEndForces fa; fa.N = P0;
	const DemandResult d3 = fib.checkSection(fa, sec, dummy);
	TestTrue(TEXT("D/C at N = P0 ~ 1.0"), FMath::Abs(d3.risk - 1.0) < 5e-3);
	TestTrue(TEXT("governing mode is Crush"), d3.mode == FailMode::Crush);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
