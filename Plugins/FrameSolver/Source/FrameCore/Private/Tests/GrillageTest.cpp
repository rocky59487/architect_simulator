// M8 #9 grillage idealization (mirrors standalone F17): a simply-supported isotropic
// square plate is woven into a longitudinal+transverse beam grid. With the nu-matched
// strip rigidities (Dx=Dy=2H=D) the center deflection tracks Kirchhoff plate theory
// (w_c = 0.00406 q a^4 / D) within a few percent and is mesh-converged; the lumped
// pressure conserves total load exactly.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/Grillage.h"
#include "FrameCore/Material.h"

#include <string>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreGrillageTest,
	"FrameCore.Grillage.BridgeDeck",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreGrillageTest::RunTest(const FString&)
{
	using namespace frame;
	using namespace frame::grillage;

	const real Epl = 30000.0, nu = 0.3;
	const real Gpl = Epl / (2.0 * (1.0 + nu));
	Material pmat(Epl, Gpl, 2400.0);

	PlateSpec sp; sp.a = 4000.0; sp.b = 4000.0; sp.t = 250.0; sp.q = 0.025;
	const real D  = Epl * sp.t * sp.t * sp.t / (12.0 * (1.0 - nu * nu));
	const real wc = 0.00406 * sp.q * sp.a * sp.a * sp.a * sp.a / D;

	auto centerDeflection = [&](int n, bool& ok) -> real {
		FrameModel m; std::string why; PlateSpec s = sp; s.nx = n; s.ny = n;
		if (!buildGrillage(m, s, pmat, why)) { ok = false; return 0; }
		const SolveResult r = solve(m);
		if (r.singular) { ok = false; return 0; }
		ok = true;
		return FMath::Abs(r.disp(gridNode(n / 2, n / 2, n), Uz));
	};

	// build + non-singular + load conservation at N=8
	{
		FrameModel m; std::string why; PlateSpec s = sp; s.nx = 8; s.ny = 8;
		TestTrue(TEXT("grillage builds"), buildGrillage(m, s, pmat, why));
		const SolveResult r = solve(m);
		TestFalse(TEXT("grillage non-singular"), r.singular);
		real sumRz = 0;
		for (int k = 0; k < (int)m.nodes.size(); ++k) sumRz += r.reaction(k, Uz);
		TestTrue(TEXT("load conservation sum Rz = q a b"),
			FMath::Abs(FMath::Abs(sumRz) - sp.q * sp.a * sp.b) <= 1e-6 * sp.q * sp.a * sp.b);
	}

	bool ok4 = false, ok8 = false, ok12 = false;
	const real d4 = centerDeflection(4, ok4);
	const real d8 = centerDeflection(8, ok8);
	const real d12 = centerDeflection(12, ok12);
	TestTrue(TEXT("all meshes solved"), ok4 && ok8 && ok12);
	const real e4 = FMath::Abs(d4 - wc) / wc, e8 = FMath::Abs(d8 - wc) / wc, e12 = FMath::Abs(d12 - wc) / wc;
	TestTrue(TEXT("grillage within 5% of plate theory (all meshes)"), e4 < 0.05 && e8 < 0.05 && e12 < 0.05);
	TestTrue(TEXT("grillage mesh-converged (|d12-d8| < 2% d8)"), FMath::Abs(d12 - d8) < 0.02 * d8);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
