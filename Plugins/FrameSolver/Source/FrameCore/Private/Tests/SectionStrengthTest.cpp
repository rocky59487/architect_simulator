// Guards the ElasticAllowable torsion-stress UNITS. The old code used |T|/J
// (N/mm^3) and compared it against an MPa shear capacity -- dimensionally wrong.
// Fixed to |T|*c/J (MPa). This test pins the magnitude and the failure mode.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/ElasticAllowable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreTorsionTest,
    "FrameCore.SectionStrength.Torsion",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreTorsionTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    Section  sec = Section::Rectangular(100.0, 100.0);   // J = 1.40833e7, c = hypot(50,50)
    Capacity cap = Capacity::make(300.0, 300.0, 180.0);  // tors proxy = shear = 180

    MemberEndForces f;            // pure torsion, everything else zero
    f.T = 1.0e6;                  // N*mm

    ElasticAllowable strength;
    const DemandResult d = strength.checkSection(f, sec, cap);

    const double cTor    = FMath::Sqrt(sec.cy * sec.cy + sec.cz * sec.cz);
    const double sTorExp = 1.0e6 * cTor / sec.J;          // ~= 5.021 MPa

    // Units fix: stress must be ~5 MPa, NOT the old |T|/J = ~0.071 N/mm^3.
    TestTrue(TEXT("sTor is MPa-scale, not the old |T|/J"), d.sTor > 1.0);
    TestTrue(TEXT("sTor == |T|*c/J"),
             FMath::Abs(d.sTor - sTorExp) <= 1e-4 * sTorExp);
    TestTrue(TEXT("governing mode is Torsion"), d.mode == FailMode::Torsion);
    TestTrue(TEXT("risk == sTor / tors-capacity"),
             FMath::Abs(d.risk - sTorExp / cap.tors) <= 1e-4 * (sTorExp / cap.tors));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
