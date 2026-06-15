// UE automation mirror of the standalone F55 supernodal opt-in lane. FRAMECORE_SUPERNODAL is set at
// build time by FrameCore.Build.cs (1 when the conda OpenBLAS/METIS env is present, else 0). At 1 this
// exercises the real self-built supernodal factor; at 0 the lane compiles out and solveLoadSupernodal
// falls back to LDLT. Either way both the disabled and enabled paths must equal solveLoad (the oracle).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/SnSolver.h"
#include "FrameCore/SnSolveOptions.h"
#include "FrameCore/SnSession.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

// Shared L-infinity relative error vs the LDLT oracle, used by both supernodal tests.
// File-local (anonymous namespace); FrameCore's UE module builds with bUseUnity=false.
namespace {
double oracleRel(const std::vector<frame::real>& got, const std::vector<frame::real>& ref) {
    double rn = 1e-30, dr = 0;
    for (frame::real v : ref) rn = FMath::Max(rn, FMath::Abs((double)v));
    for (size_t k = 0; k < got.size() && k < ref.size(); ++k)
        dr = FMath::Max(dr, FMath::Abs((double)got[k] - (double)ref[k]));
    return dr / rn;
}
}  // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreSupernodalTest,
    "FrameCore.Solver.Supernodal",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreSupernodalTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;

    Section  sec = Section::Rectangular(100.0, 100.0);
    Material mat(210000.0, 80769.0, 7850.0);
    const real kP = 1000.0, kL = 2000.0;
    FrameModel m; fixtures::cantileverTipLoad(m, kP, kL, mat, sec);
    PreparedSystem ps = assembleAndFactor(m);
    const SolveResult ref = solveLoad(ps, m);   // LDLT oracle

    // (A) disabled => bit-exact drop-in equal to solveLoad.
    SnSolveOptions off; off.enabled = false;
    const SolveResult snOff = solveLoadSupernodal(ps, m, off);
    TestFalse(TEXT("disabled not singular"), snOff.singular);
    TestTrue(TEXT("disabled supernodal == solveLoad (drop-in)"), oracleRel(snOff.u, ref.u) <= 1e-12);

    // (B) enabled => equals the LDLT oracle. With FRAMECORE_SUPERNODAL=1 (standalone / stage 3) this
    //     runs the self-built supernodal factor; with =0 (current UE) it safely falls back to LDLT.
    //     Either way the numeric result must match the direct solve.
    SnSolveOptions on; on.enabled = true;
    const SolveResult snOn = solveLoadSupernodal(ps, m, on);
    TestFalse(TEXT("enabled not singular"), snOn.singular);
    TestTrue(TEXT("enabled supernodal == LDLT oracle"), oracleRel(snOn.u, ref.u) <= 1e-10);
    return true;
}

// SnSession: factor-once + solve-many (the supernodal production payoff). The session factors in the
// ctor; each solveFrame reuses that factor and must match the LDLT oracle. With FRAMECORE_SUPERNODAL=1
// this is the real supernodal factor; at 0 it transparently falls back to LDLT.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreSnSessionTest,
    "FrameCore.Solver.SnSession",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreSnSessionTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    Section  sec = Section::Rectangular(100.0, 100.0);
    Material mat(210000.0, 80769.0, 7850.0);
    const real kP = 1000.0, kL = 2000.0;
    FrameModel m; fixtures::cantileverTipLoad(m, kP, kL, mat, sec);
    PreparedSystem ps = assembleAndFactor(m);
    const SolveResult ref = solveLoad(ps, m);

    SnSession sess(ps);   // enabled default true -> factors once in the ctor
    for (int frame = 0; frame < 3; ++frame)
    {
        const SolveResult got = sess.solveFrame(m);   // reuses the same factor each frame
        TestFalse(TEXT("SnSession frame not singular"), got.singular);
        TestTrue(TEXT("SnSession reused factor == LDLT oracle (rel<1e-10)"), oracleRel(got.u, ref.u) <= 1e-10);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
