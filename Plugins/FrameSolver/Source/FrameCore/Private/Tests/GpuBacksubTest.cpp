// UE automation mirror of the standalone F67 GPU-vs-CPU bit-equivalence fixture. Activates only
// when FRAMECORE_CUDA=1 was set at configure time (FrameCore.Build.cs auto-detects cuDSS + CUDA
// runtime in the conda framecore-direct env). On non-CUDA builds the file compiles to nothing.
#if defined(FRAMECORE_CUDA) && FRAMECORE_CUDA

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/SnSession.h"
#include "FrameTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace {
double gpuRelInf(const std::vector<frame::real>& got, const std::vector<frame::real>& ref) {
    double rn = 1e-30, dr = 0;
    for (frame::real v : ref) rn = FMath::Max(rn, FMath::Abs((double)v));
    for (size_t k = 0; k < got.size() && k < ref.size(); ++k)
        dr = FMath::Max(dr, FMath::Abs((double)got[k] - (double)ref[k]));
    return dr / rn;
}
}  // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreGpuBacksubTest,
    "FrameCore.Solver.GpuBacksub",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreGpuBacksubTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;

    Section  sec = Section::Rectangular(150.0, 200.0);
    sec.J = 1.5e8;
    Material mat(200000.0, 76923.07692307692, 7850.0);
    FrameModel m; fixtures::cantileverTipLoad(m, 1000.0, 2000.0, mat, sec);

    SolveOptions optSn; optSn.useSupernodalPrimary = true;
    PreparedSystem ps = assembleAndFactor(m, optSn);
    TestFalse(TEXT("SnPrimary prepared singular"), ps.isSingular());

    SnSessionOptions sCpu;
    SnSessionOptions sGpu; sGpu.useGpuBacksub = true;
    SnSession cpuSess(ps, sCpu);
    SnSession gpuSess(ps, sGpu);

    TestTrue(TEXT("CPU-mode SnSession valid"), cpuSess.valid());
    TestTrue(TEXT("GPU-mode SnSession valid"), gpuSess.valid());

    // The GPU-mode diagnostic must mention either a successful cuDSS attach or a clean fallback
    // reason. Either way it must carry the [GPU] tag so a regression that silently loses the
    // GPU branch is caught.
    const std::string gpuDiag = gpuSess.diagnostic();
    const bool hasGpuTag = gpuDiag.find("[GPU]") != std::string::npos;
    TestTrue(TEXT("GPU-mode diagnostic carries [GPU] tag"), hasGpuTag);

    const SolveResult Rc = cpuSess.solveFrame(m);
    const SolveResult Rg = gpuSess.solveFrame(m);

    // u and reactions must match to round-off; sizes must match. The cuDSS METIS reorder
    // matches the self-built sn_chol reorder exactly on this small fixture so rel = 0 here.
    TestEqual(TEXT("GPU.u size == CPU.u size"), Rg.u.size(), Rc.u.size());
    TestTrue(TEXT("GPU.u == CPU.u (rel < 1e-8)"), gpuRelInf(Rg.u, Rc.u) < 1e-8);
    TestEqual(TEXT("GPU.reactions size == CPU.reactions size"),
              Rg.reactions.size(), Rc.reactions.size());
    TestTrue(TEXT("GPU.reactions == CPU.reactions (rel < 1e-8)"),
             gpuRelInf(Rg.reactions, Rc.reactions) < 1e-8);
    TestEqual(TEXT("GPU.memberForces.size == CPU.memberForces.size"),
              Rg.memberForces.size(), Rc.memberForces.size());

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
#endif  // FRAMECORE_CUDA
