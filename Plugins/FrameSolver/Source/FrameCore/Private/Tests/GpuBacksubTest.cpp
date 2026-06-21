// UE automation mirror of the standalone F67 / F67s GPU-vs-CPU fixtures. Activates only
// when FRAMECORE_CUDA=1 was set at configure time (FrameCore.Build.cs auto-detects cuDSS +
// CUDA runtime in the conda framecore-direct env). On non-CUDA builds the file compiles to
// nothing.
//
// Two tests are emitted side by side, mirroring the standalone split (v2.11.1-RC):
//   FFrameCoreGpuBacksubTest        smoke   -- tolerates silent CPU fallback (any "[GPU]"
//                                              tag passes). Lets devs compile-test the
//                                              CUDA lane on any box.
//   FFrameCoreGpuBacksubStrictTest  strict  -- runs only when FRAMECORE_GPU_STRICT=1 in
//                                              the env (run_gpu_gate.ps1 sets this when
//                                              cuDSS DLLs resolve). Requires the success
//                                              substring "[GPU] cuDSS factor ready" in
//                                              the diagnostic; any fallback FAILS rather
//                                              than green-washes.
#if defined(FRAMECORE_CUDA) && FRAMECORE_CUDA

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformMisc.h"
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

// ---- strict variant: silent CPU fallback is a FAIL ----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreGpuBacksubStrictTest,
    "FrameCore.Solver.GpuBacksubStrict",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreGpuBacksubStrictTest::RunTest(const FString& /*Parameters*/)
{
    using namespace frame;

    // Gate on the env flag set by run_gpu_gate.ps1 when cuDSS DLLs resolve. We use
    // FPlatformMisc::GetEnvironmentVariable so the test sees the env the editor was
    // launched with (UnrealEditor-Cmd inherits its parent's env).
    const FString StrictFlag = FPlatformMisc::GetEnvironmentVariable(TEXT("FRAMECORE_GPU_STRICT"));
    if (StrictFlag != TEXT("1")) {
        AddInfo(TEXT("FRAMECORE_GPU_STRICT != 1 -- strict GPU-attached check skipped. "
                     "Run via Scripts/run_gpu_gate.ps1 on a box with cuDSS to enforce."));
        return true;
    }

    Section  sec = Section::Rectangular(150.0, 200.0);
    sec.J = 1.5e8;
    Material mat(200000.0, 76923.07692307692, 7850.0);
    FrameModel m; fixtures::cantileverTipLoad(m, 1000.0, 2000.0, mat, sec);

    SolveOptions optSn; optSn.useSupernodalPrimary = true;
    PreparedSystem ps = assembleAndFactor(m, optSn);
    TestFalse(TEXT("SnPrimary prepared singular (strict)"), ps.isSingular());

    SnSessionOptions sGpu; sGpu.useGpuBacksub = true;
    SnSession gpuSess(ps, sGpu);
    TestTrue(TEXT("GPU-mode SnSession valid (strict)"), gpuSess.valid());

    // Success substring -- emitted only when cuDSS analysis/factor succeeded on device.
    // Any fallback path emits "[GPU] ... CPU lane used" or similar; this check FAILs on
    // silent fallback instead of green-washing it.
    //
    // D-01 audit: cuDSS factor success does NOT guarantee Phase-1 cuSPARSE SpMV reactions
    // succeeded. SnSession.cpp:333/337 emit "reactions on CPU" when SpMV setup fails AFTER
    // cuDSS factor is ready -- the diag then contains BOTH substrings, and a factor-only
    // check would silently green-wash the reactions fallback. Strict mode rejects that too.
    const std::string diag = gpuSess.diagnostic();
    const bool factorOnGpu = diag.find("[GPU] cuDSS factor ready") != std::string::npos;
    const bool reactionsOnCpu = diag.find("reactions on CPU") != std::string::npos;
    const bool reallyOnGpu = factorOnGpu && !reactionsOnCpu;
    TestTrue(TEXT("GPU strict-attached (cuDSS factor + SpMV reactions on device, no CPU fallback)"),
             reallyOnGpu);
    // Diagnostic context only on failure -- TestTrue itself records the failure; this
    // AddInfo gives the integrator the raw diag string without producing a duplicate error
    // entry (D-02 audit: avoid double-failure entries from AddError + TestTrue(false)).
    if (!reallyOnGpu) {
        AddInfo(FString::Printf(TEXT("GPU strict diagnostic: %hs"), diag.c_str()));
    }

    SnSessionOptions sCpuS;
    SnSession cpuSess(ps, sCpuS);

    // F-05 audit: only run the physics oracle when the strict attach really succeeded.
    // Otherwise the rel-check against CPU is vacuous (gpuSess fell back to LDLT internally
    // and matches CPU trivially), masking the real failure mode.
    if (!reallyOnGpu) { return false; }

    const SolveResult Rc = cpuSess.solveFrame(m);
    const SolveResult Rg = gpuSess.solveFrame(m);
    TestTrue(TEXT("GPU.u == CPU.u (rel<1e-8) under strict attach"),
             gpuRelInf(Rg.u, Rc.u) < 1e-8);
    // D-08 audit: smoke test verifies reactions equality too; strict must mirror that
    // since strict mode now also enforces SpMV reactions on device.
    TestEqual(TEXT("GPU.reactions size == CPU.reactions size (strict)"),
              Rg.reactions.size(), Rc.reactions.size());
    TestTrue(TEXT("GPU.reactions == CPU.reactions (rel<1e-8) under strict attach"),
             gpuRelInf(Rg.reactions, Rc.reactions) < 1e-8);

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
#endif  // FRAMECORE_CUDA
