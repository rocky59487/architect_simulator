// UE automation mirror of the standalone F44 -- fully-stressed design of the classic 10-bar truss.
// Stress-ratio resizing drives every sized bar to D/C = 1; the four low-force bars bottom out at the
// A_min bound; the converged weight matches the literature optimum (~1593 lb; the combined-stress
// screen lands just above the pin-jointed value because the gated-pin members carry minor bending).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/SizeOpt.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreSizeOptTest,
    "FrameCore.SizeOpt.FullyStressed",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreSizeOptTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;
    // NB: 'IN'/'OUT' are Windows SAL macros (pulled in via CoreMinimal.h), so the unit constants
    // must NOT be named IN -- use kIn/kLb/kPsi. (The standalone F44 can use IN; it never sees <windows.h>.)
    const double kIn = 25.4, kLb = 4.4482216152605, kPsi = 0.0068947572931684;
    const double Empa = 1.0e7 * kPsi, sigA = 25000.0 * kPsi;
    const double bay = 360.0 * kIn, A0 = 10.0 * kIn * kIn, Amin = 0.1 * kIn * kIn, P = 1.0e5 * kLb;
    Material tmat(Empa, Empa / 2.6, 0.0); tmat.cap = Capacity::make(sigA, sigA, sigA);
    FrameModel m; fixtures::tenBarTruss(m, bay, A0, P, tmat);

    SizeOptOptions o; o.maxIter = 100; o.dcTol = 1e-10; o.Amin = Amin;
    const SizeOptResult R = runSizeOptimization(m, o);
    TestTrue(TEXT("converged"), R.converged && !R.singular);

    // weight from the final areas (imperial): W = sum 0.1 lb/in^3 * A_in2 * L_in
    double weightLb = 0;
    for (int k = 0; k < (int)m.members.size(); ++k) {
        const int ni = m.nodeIndex(m.members[(size_t)k].i), nj = m.nodeIndex(m.members[(size_t)k].j);
        const double L = norm(m.nodes[(size_t)nj].pos - m.nodes[(size_t)ni].pos);
        weightLb += 0.1 * (R.finalAreas[(size_t)k] / (kIn * kIn)) * (L / kIn);
    }
    TestTrue(TEXT("weight >= pin-jointed optimum 1593 lb"), weightLb >= 1593.0);
    TestTrue(TEXT("weight within 1.5% of literature 1593.2 lb"),
             FMath::Abs(weightLb - 1593.2) / 1593.2 < 1.5e-2);

    // fully-stressed: every sized bar sits at D/C = 1 to machine precision
    bool fs = true; int nSized = 0;
    for (int k = 0; k < (int)m.members.size(); ++k)
        if (R.finalAreas[(size_t)k] > Amin * 1.01) {
            ++nSized;
            if (FMath::Abs(R.finalDC[(size_t)k] - 1.0) > 1e-6) fs = false;
        }
    TestTrue(TEXT("sized bars fully stressed (|D/C-1|<1e-6)"), fs && nSized > 0);

    // the four low-force bars (2,5,6,10 -> idx 1,4,5,9) bottom out at A_min exactly (clamp)
    bool atMin = true;
    for (int k : { 1, 4, 5, 9 })
        if (FMath::Abs(R.finalAreas[(size_t)k] - Amin) / Amin > 1e-9) atMin = false;
    TestTrue(TEXT("bars 2/5/6/10 at A_min bound"), atMin);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
