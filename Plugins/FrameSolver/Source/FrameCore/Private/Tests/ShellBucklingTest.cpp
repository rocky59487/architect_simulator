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
#include "FrameCore/Shell.h"

#include <string>

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

// R2.1 audit AC-06 mirror (BLDG SLV-NEW-4): the shell-buckling design workflow with EN 1993-1-6
// / NASA SP-8007 knockdown alpha. Verifies: (i) raw and design eigenvalues are reported separately;
// (ii) design = alpha * raw; (iii) default alpha=0 leaves raw unchanged (bit-identical to v2.0);
// (iv) out-of-range alpha surfaces a clear diagnostic (NLL-NEW-2 fix).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreShellBucklingKnockdownTest,
    "FrameCore.Buckling.ShellKnockdown",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreShellBucklingKnockdownTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    const real E = 200000.0, nu = 0.3;
    Material smat(E, E / (2.0 * (1.0 + nu))); smat.nu = nu;
    const real a = 1000.0, t = 5.0, Pref = 1.0;
    SolveOptions so; so.shellGeometricStiffness = true;
    FrameModel m; ssPlateUniaxial(m, 12, a, t, Pref, smat);
    PreparedSystem ps = assembleAndFactor(m, so);

    BucklingOptions optRaw;
    BucklingOptions optK;   optK.shellBucklingKnockdown   = 0.65;
    BucklingOptions optBad; optBad.shellBucklingKnockdown = 1.5;       // out of (0,1] range
    const BucklingResult bRaw = solveBuckling(ps, m, optRaw);
    const BucklingResult bK   = solveBuckling(ps, m, optK);
    const BucklingResult bBad = solveBuckling(ps, m, optBad);
    TestFalse(TEXT("raw non-singular"),       bRaw.singular);
    TestFalse(TEXT("knockdown non-singular"), bK.singular);
    TestFalse(TEXT("out-of-range solves"),    bBad.singular);
    TestTrue(TEXT("raw eigenvalue stable across alphas"),
             FMath::Abs(bK.reportedCriticalFactor - bRaw.reportedCriticalFactor) < 1e-9 * bRaw.reportedCriticalFactor);
    TestTrue(TEXT("design value = 0.65 * raw"),
             FMath::Abs(bK.criticalFactor - 0.65 * bRaw.criticalFactor) < 1e-9 * bRaw.criticalFactor);
    TestTrue(TEXT("default knockdownFactor = 1.0 (bit-identical to v2.0)"),
             FMath::Abs(bRaw.knockdownFactor - 1.0) < 1e-12);
    TestTrue(TEXT("out-of-range alpha clamps to 1.0"),
             FMath::Abs(bBad.knockdownFactor - 1.0) < 1e-12);
    // R2.1 audit FINAL-NLLNEW2-1: standalone F64a-shell checks BOTH 'shellBucklingKnockdown='
    // and 'out of range'. Mirror the two-substring check here so a future diagnostic refactor
    // that drops the field-name prefix fails both gate legs, not just standalone.
    const std::string diag = bBad.diagnostic;
    TestTrue(TEXT("diagnostic names the field 'shellBucklingKnockdown='"),
             diag.find("shellBucklingKnockdown=") != std::string::npos);
    TestTrue(TEXT("diagnostic includes 'out of range'"),
             diag.find("out of range") != std::string::npos);
    // R2.1 audit FINAL-UE-1: mirror the standalone F64a-shell analytic-oracle check
    // (raw eigenvalue agrees with Kirchhoff N_cr = 4 pi^2 D / a^2 to within 5% at n=12).
    const real D    = E * t * t * t / (12.0 * (1.0 - nu * nu));
    const real Ncr  = 4.0 * kPi * kPi * D / (a * a);
    const real rel  = FMath::Abs(bRaw.reportedCriticalFactor - Ncr) / FMath::Max(real(1e-30), Ncr);
    TestTrue(TEXT("raw eigenvalue agrees with Kirchhoff N_cr = 4 pi^2 D/a^2 (rel<5% at n=12)"),
             rel < 5e-2);
    return true;
}

// R2.1 audit AC-07 mirror (BLDG SLV-NEW-4): the opt-in coarse-curved-shell mesh guard.
// Verifies: (i) default flag=0 admits any mesh (bit-identical to v2.0); (ii) tightening the flag
// rejects an 8-facet cylinder (45 deg per facet > 22.5 deg tol) with a diagnostic naming the angle;
// (iii) a refined 32-facet cylinder (11.25 deg per facet) is admitted; (iv) the BLDG SLV-NEW-1 fix
// keeps an inactive shell out of the angle scan.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreShellCurvatureGuardTest,
    "FrameCore.Validation.ShellCurvatureGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FFrameCoreShellCurvatureGuardTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    const real R_cyl = 1000.0, L_cyl = 1000.0, tCyl = 5.0;
    Material smat(200000.0, 76923.0, 7850.0); smat.nu = 0.3;

    auto buildCyl = [&](FrameModel& m, int N) {
        m = FrameModel{};
        m.materials.push_back(smat);
        for (int j = 0; j < 2; ++j)
            for (int i = 0; i < N; ++i) {
                const real ang = 2.0 * kPi * real(i) / real(N);
                Node nd(j * N + i, R_cyl * FMath::Cos(ang), R_cyl * FMath::Sin(ang), j * L_cyl);
                if (j == 0) nd.fixAll();
                m.nodes.push_back(nd);
            }
        for (int i = 0; i < N; ++i) {
            const int i1 = (i + 1) % N;
            ShellQuad sh;
            sh.id = i;
            sh.n[0] = (NodeId)i;
            sh.n[1] = (NodeId)i1;
            sh.n[2] = (NodeId)(N + i1);
            sh.n[3] = (NodeId)(N + i);
            sh.matIdx = 0; sh.t = tCyl;
            m.shells.push_back(sh);
        }
        ShellPressure sp; sp.shell = 0; sp.p = -0.01;
        m.shellPressures.push_back(sp);
    };

    // (a) default flag = 0: any mesh is admitted (v2.0 bit-identical regression guard).
    {
        FrameModel m; buildCyl(m, 8);
        PreparedSystem ps = assembleAndFactor(m);
        TestFalse(TEXT("default flag=0 admits 8-facet cylinder (v2.0 regression)"), ps.isSingular());
    }
    // (b) flag = 22.5 deg rejects 8-facet (45 deg per facet) with a clear diagnostic.
    {
        SolveOptions so; so.shellCurvatureMaxAngleDeg = 22.5;
        FrameModel m; buildCyl(m, 8);
        PreparedSystem ps = assembleAndFactor(m, so);
        TestTrue(TEXT("22.5deg guard rejects 8-facet cylinder"), ps.isSingular());
        const FString diag = FString(ANSI_TO_TCHAR(ps.diagnostic()));
        TestTrue(TEXT("diagnostic names max-angle"), diag.Contains(TEXT("max adjacent-facet angle")));
    }
    // (c) refined 32-facet cylinder (11.25 deg per facet) is admitted at the same tol.
    {
        SolveOptions so; so.shellCurvatureMaxAngleDeg = 22.5;
        FrameModel m; buildCyl(m, 32);
        PreparedSystem ps = assembleAndFactor(m, so);
        TestFalse(TEXT("22.5deg guard admits 32-facet cylinder"), ps.isSingular());
    }
    // (d) BLDG SLV-NEW-1 fix: an inactive shell does not contribute to the angle scan.
    {
        SolveOptions so; so.shellCurvatureMaxAngleDeg = 22.5;
        FrameModel m; buildCyl(m, 32);
        // Set every OTHER shell inactive to halve the active facet count; geometry is otherwise
        // a 32-faceted cylinder so the active subset still has ~11.25 deg per active boundary.
        // After the fix the inactive shells are skipped during normal-pair angle measurement,
        // so the guard should still admit the model.
        for (size_t s = 0; s < m.shells.size(); ++s)
            if ((s % 2) == 1) m.shells[s].active = false;
        PreparedSystem ps = assembleAndFactor(m, so);
        TestFalse(TEXT("inactive shells skipped (no false-rejection in progressive collapse)"),
                  ps.isSingular());
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
