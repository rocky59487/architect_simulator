// M8 #10 Schur static condensation (mirrors standalone F16): condensing the internal
// DOFs of a 4-node cantilever (with the load sitting on an internal node, so fEff must
// fold it onto the boundary) reproduces the full solve's boundary displacements exactly.
// Guards: an empty boundary set (K_ii = unconstrained K, 6 rigid-body modes) and an
// out-of-range boundary index are both rejected (ok=false), never NaN.
// SCOPE: both the full solve and the condensation share the same element assembly path
// (BeamColumnElement), so this oracle validates the REDUCTION math (Schur complement +
// fEff folding + the independent dense boundary solve), not the element stiffness itself
// (that is covered by F1-F12 against closed-form analytic solutions).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/StaticCondensation.h"
#include "FrameCore/Section.h"
#include "FrameCore/Material.h"
#include "FrameTestFixtures.h"

#include <vector>
#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Independent dense Gauss solve (no Eigen) so the check cross-validates the engine.
	// A is row-major n*n; returns x solving A x = b.
	std::vector<double> SchurDenseSolve(std::vector<double> A, std::vector<double> b, int n)
	{
		for (int col = 0; col < n; ++col)
		{
			int piv = col; double best = std::fabs(A[(size_t)col * n + col]);
			for (int r = col + 1; r < n; ++r)
			{
				const double v = std::fabs(A[(size_t)r * n + col]);
				if (v > best) { best = v; piv = r; }
			}
			if (piv != col)
			{
				for (int k = 0; k < n; ++k) std::swap(A[(size_t)col * n + k], A[(size_t)piv * n + k]);
				std::swap(b[(size_t)col], b[(size_t)piv]);
			}
			const double d = A[(size_t)col * n + col];
			for (int r = col + 1; r < n; ++r)
			{
				const double f = A[(size_t)r * n + col] / d;
				for (int k = col; k < n; ++k) A[(size_t)r * n + k] -= f * A[(size_t)col * n + k];
				b[(size_t)r] -= f * b[(size_t)col];
			}
		}
		std::vector<double> x((size_t)n, 0.0);
		for (int r = n - 1; r >= 0; --r)
		{
			double s = b[(size_t)r];
			for (int k = r + 1; k < n; ++k) s -= A[(size_t)r * n + k] * x[(size_t)k];
			x[(size_t)r] = s / A[(size_t)r * n + r];
		}
		return x;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreSchurCondensationTest,
	"FrameCore.Schur.EqualsFullSolve",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreSchurCondensationTest::RunTest(const FString&)
{
	using namespace frame;

	const Section sec = Section::Rectangular(100.0, 100.0);
	Material mat(210000.0, 80769.0, 7850.0);
	mat.cap = Capacity::make(300.0, 300.0, 180.0);

	const real P = 1000.0, L = 1000.0;
	FrameModel m; fixtures::condensationChain(m, P, L, mat, sec);
	const SolveResult full = solve(m);
	TestFalse(TEXT("full solve non-singular"), full.singular);

	std::vector<int> bdofs;
	for (int d = 0; d < 6; ++d) bdofs.push_back(gdof(0, d));
	for (int d = 0; d < 6; ++d) bdofs.push_back(gdof(3, d));
	const CondensationResult cr = condenseStatic(m, bdofs);
	TestTrue(TEXT("condensation ok"), cr.ok);
	TestEqual(TEXT("boundaryCount == 12"), cr.boundaryCount, 12);
	TestTrue(TEXT("kappa_D finite & < 1e10"),
		FMath::IsFinite(cr.conditionNumber) && cr.conditionNumber < 1e10);

	// node0 encastre -> reduced system is node3's 6x6 sub-block; solve independently.
	const int nb = cr.boundaryCount;
	std::vector<double> A(36), bb(6);
	for (int i = 0; i < 6; ++i)
	{
		bb[(size_t)i] = cr.fEff[(size_t)(6 + i)];
		for (int j = 0; j < 6; ++j) A[(size_t)i * 6 + j] = cr.S[(size_t)(6 + i) * nb + (6 + j)];
	}
	const std::vector<double> x = SchurDenseSolve(A, bb, 6);
	const real uzFull = full.disp(3, Uz);
	TestTrue(TEXT("condensed u_z(node3) == full"), FMath::Abs(x[(size_t)Uz] - uzFull) <= 1e-9 * FMath::Max(1.0, FMath::Abs(uzFull)));
	real maxDiff = 0;
	for (int d = 0; d < 6; ++d) maxDiff = FMath::Max(maxDiff, FMath::Abs(x[(size_t)d] - full.disp(3, d)));
	TestTrue(TEXT("condensed u(node3) == full (all 6 DOF)"), maxDiff < 1e-7 * FMath::Max(1.0, FMath::Abs(uzFull)));

	// guards: empty boundary (rigid-body internal block) and out-of-range index -> reject.
	const CondensationResult bad = condenseStatic(m, std::vector<int>{});
	TestFalse(TEXT("empty-boundary condensation rejected"), bad.ok);
	const CondensationResult oor = condenseStatic(m, std::vector<int>{ 9999 });
	TestFalse(TEXT("out-of-range boundary rejected"), oor.ok);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
