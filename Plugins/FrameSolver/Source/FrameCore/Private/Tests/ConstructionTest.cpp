// M9 #12 construction-discipline builders (mirrors standalone F19-F22): each building
// element maps to its structural idealization and reproduces a closed-form oracle —
//  * Column  : cantilever beam-column, u_x = H L^3/3EI, u_z = P L/EA.
//  * Beam    : clamped-clamped UDL, |M_end|=wL^2/12, |M_mid|=wL^2/24, delta=wL^4/384EI.
//  * Wall    : equivalent diagonal brace sized to a target sway stiffness K -> H/u_x == K.
//  * Slab    : delegates to the grillage idealization (center deflection ~ plate theory).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/Construction.h"
#include "FrameCore/Grillage.h"
#include "FrameCore/Section.h"
#include "FrameCore/Material.h"

#include <string>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreConstructionTest,
	"FrameCore.Construct.Discipline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreConstructionTest::RunTest(const FString&)
{
	using namespace frame;
	const real E = 210000.0, G = 80769.0;
	const Section sec = Section::Rectangular(100.0, 100.0);
	Material mat(E, G, 7850.0);
	mat.cap = Capacity::make(300.0, 300.0, 180.0);

	auto close = [&](real got, real exp, real tol) { return FMath::Abs(got - exp) <= tol * FMath::Max(FMath::Abs(exp), 1e-30); };

	// COLUMN
	{
		const real h = 3000.0, P = 50000.0, H = 2000.0;
		FrameModel m; construct::buildColumn(m, h, P, H, mat, sec);
		const SolveResult r = solve(m);
		TestFalse(TEXT("column non-singular"), r.singular);
		TestTrue(TEXT("column lateral u_x = H h^3/3EI"), close(FMath::Abs(r.disp(1, Ux)), H * h * h * h / (3.0 * E * sec.Iz), 1e-6));
		TestTrue(TEXT("column axial u_z = P h/EA"), close(FMath::Abs(r.disp(1, Uz)), P * h / (E * sec.A), 1e-6));
	}

	// BEAM (clamped-clamped UDL)
	{
		const real w = 5.0, L = 4000.0; const int nseg = 4;
		FrameModel m; construct::buildFixedBeam(m, L, w, nseg, mat, sec);
		const SolveResult r = solve(m);
		TestFalse(TEXT("fixed beam non-singular"), r.singular);
		TestTrue(TEXT("fixed-end moment wL^2/12"), close(FMath::Abs(r.memberForces[0].endI.Mz), w * L * L / 12.0, 1e-6));
		TestTrue(TEXT("midspan moment wL^2/24"), close(FMath::Abs(r.memberForces[nseg / 2 - 1].endJ.Mz), w * L * L / 24.0, 1e-5));
		TestTrue(TEXT("midspan deflection wL^4/384EI"), close(FMath::Abs(r.disp(nseg / 2, Uz)), w * L * L * L * L / (384.0 * E * sec.Iz), 1e-6));
	}

	// WALL (equivalent brace, K round-trip)
	{
		const real bay = 5000.0, hgt = 3000.0, Ktar = 1000.0, H = 10000.0;
		FrameModel m; construct::buildEquivalentBraceWall(m, bay, hgt, Ktar, H, mat);
		SolveOptions opt; opt.enableReleases = true;
		const SolveResult r = solve(m, opt);
		TestFalse(TEXT("wall brace non-singular"), r.singular);
		TestTrue(TEXT("wall sway stiffness round-trip K=H/u_x"), close(H / FMath::Abs(r.disp(1, Ux)), Ktar, 1e-6));
	}

	// SLAB (-> grillage)
	{
		using namespace frame::grillage;
		const real Epl = 30000.0, nu = 0.3; const real Gpl = Epl / (2.0 * (1.0 + nu));
		Material pmat(Epl, Gpl, 2400.0);
		PlateSpec sp; sp.a = 4000.0; sp.b = 4000.0; sp.t = 250.0; sp.q = 0.025; sp.nx = 8; sp.ny = 8;
		FrameModel m; std::string why;
		const bool ok = construct::buildSlab(m, sp, pmat, why);
		const SolveResult r = solve(m);
		const real D  = Epl * sp.t * sp.t * sp.t / (12.0 * (1.0 - nu * nu));
		const real wc = 0.00406 * sp.q * sp.a * sp.a * sp.a * sp.a / D;
		TestTrue(TEXT("slab builds"), ok);
		TestFalse(TEXT("slab non-singular"), r.singular);
		TestTrue(TEXT("slab center deflection ~ plate theory (<5%)"), close(FMath::Abs(r.disp(gridNode(4, 4, 8), Uz)), wc, 0.05));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
