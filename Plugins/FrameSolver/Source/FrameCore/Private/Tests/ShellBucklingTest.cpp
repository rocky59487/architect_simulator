// UE automation mirror of standalone F57 -- the opt-in shell geometric stiffness
// (SolveOptions::shellGeometricStiffness) gives a model containing MITC4 shells a meaningful
// LINEAR buckling factor. A simply-supported square plate under uniform uniaxial in-plane
// compression buckles at the classical Kirchhoff load  N_cr = 4 pi^2 D / a^2  (square plate,
// lowest mode m=n=1 -> factor k=4); the MITC4 flat facet converges to it as O(1/N^2) (~0.5% at
// n=16, matching the standalone F57 sweep). The opt-in-OFF case has no shell Kg -> no compression
// source -> singular, proving the flag (not some incidental path) is what produces the factor.
// Axis invariance and sparse==dense on the shell Kg are covered by standalone F57, not duplicated.
// NOTE: do NOT name any local constant IN / OUT here -- those are Windows SAL macros pulled in
// through CoreMinimal.h and would not compile.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/BucklingAnalysis.h"
#include "FrameCore/SolveOptions.h"
#include "FrameCore/Material.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace {
// Simply-supported (w=0 on the 4 edges) n x n MITC4 square plate in the global z=0 plane under
// uniform uniaxial in-plane compression along x (consistent edge nodal loads). Mirrors the
// plate-normal=z branch of standalone F57's buildPlate: reaction edge i=0 gives a uniform sigma
// along x, one corner pins the 2nd in-plane axis, all drilling DOFs are fixed.
inline void ssPlateUniaxial(frame::FrameModel& m, int n, frame::real a, frame::real t,
                            frame::real Pref, const frame::Material& smat)
{
    using namespace frame;
    m = FrameModel{};
    m.materials.push_back(smat);
    const real hh = a / n;
    auto gid = [n](int i, int j) { return j * (n + 1) + i; };
    for (int j = 0; j <= n; ++j)
        for (int i = 0; i <= n; ++i) {
            Node nd(gid(i, j), i * hh, j * hh, 0.0);
            nd.fixed[5] = true;                              // drilling (Rz, rotation about the facet normal)
            const bool edge = (i == 0 || i == n || j == 0 || j == n);
            if (edge) nd.fixed[2] = true;                    // simple support: w = 0 on all 4 edges
            if (i == 0) nd.fixed[0] = true;                  // reaction edge -> uniform sigma along x
            if (i == 0 && j == 0) nd.fixed[1] = true;        // pin the 2nd in-plane axis (rigid body + spin)
            m.nodes.push_back(nd);
        }
    int sid = 0;
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
            m.shells.push_back(ShellQuad(sid++, gid(i, j), gid(i + 1, j), gid(i + 1, j + 1), gid(i, j + 1), 0, t));
    for (int j = 0; j <= n; ++j) {                           // uniform uniaxial compression on the i=n edge
        const real trib = (j == 0 || j == n) ? 0.5 * hh : hh;
        NodalLoad nl; nl.node = gid(n, j);
        nl.comp[0] = -Pref * trib;                           // compression along -x
        m.nodalLoads.push_back(nl);
    }
}
}  // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreShellBucklingTest,
    "FrameCore.Buckling.ShellGeometricStiffness",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreShellBucklingTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    const real E = 200000.0, nu = 0.3;
    Material smat(E, E / (2.0 * (1.0 + nu))); smat.nu = nu;
    const real a = 1000.0, t = 5.0, Pref = 1.0;             // thin plate: t/a = 0.005 (Kirchhoff limit)
    const real D   = E * t * t * t / (12.0 * (1.0 - nu * nu));
    const real Ncr = 4.0 * kPi * kPi * D / (a * a);         // SS square plate, uniaxial, k=4

    // (a) opt-in ON: shell Kg -> buckling factor matches the analytic Kirchhoff load (rel < 3% at n=16).
    {
        SolveOptions so; so.shellGeometricStiffness = true;
        FrameModel m; ssPlateUniaxial(m, 16, a, t, Pref, smat);
        PreparedSystem ps = assembleAndFactor(m, so);
        const BucklingResult b = solveBuckling(ps, m);
        TestFalse(TEXT("shell buckling non-singular"), b.singular);
        const real NcrNum = b.criticalFactor * Pref;
        TestTrue(TEXT("shell Kg buckling == 4 pi^2 D/a^2 (rel < 3%)"),
                 FMath::Abs(NcrNum - Ncr) < 3e-2 * Ncr);
    }

    // (b) opt-in OFF: shells contribute no Kg -> no compression source -> singular. Proves the flag,
    //     not some incidental path, is what produces the buckling factor (and that the default stays
    //     today's beam-column-only behavior, bit-for-bit).
    {
        SolveOptions so; so.shellGeometricStiffness = false;
        FrameModel m; ssPlateUniaxial(m, 16, a, t, Pref, smat);
        PreparedSystem ps = assembleAndFactor(m, so);
        const BucklingResult b = solveBuckling(ps, m);
        TestTrue(TEXT("opt-in OFF -> no shell Kg -> singular"), b.singular);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
