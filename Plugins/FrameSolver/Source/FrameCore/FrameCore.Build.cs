using UnrealBuildTool;
using System;
using System.IO;

// FrameCore — engine-agnostic structural solver. The Private *.cpp files use Eigen
// only through Private/FrameEigen.h; Public headers stay Eigen-free (POD + std).
// The same sources are gated by the standalone C++17 harness and by UE automation;
// UBT compiles this module as C++20 because the current UE toolchain requires it.
public class FrameCore : ModuleRules
{
    public FrameCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage    = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;   // core remains C++17-compatible; UE target uses C++20
        bEnableExceptions = true;   // Eigen may throw / assert

        // Shared helpers are hoisted to header-only inlines (FrameTypes.h, PreparedSystemImpl.h,
        // FrameEigen.h, MemberQuery.h, CollapseSupport.h, ElementFactory.h) — no anonymous-
        // namespace name collisions remain across TUs, so unity build is safe.
        bUseUnity = true;

        PublicDependencyModuleNames.AddRange(new string[] { "Core" });

        // UE-bundled Eigen 3.4.0 (header-only, MPL2). Private so consumers of the
        // FrameCore public API never see Eigen.
        AddEngineThirdPartyPrivateStaticDependencies(Target, "Eigen");

        // Flip FrameEigen.h to the UE-guarded include path (THIRD_PARTY_INCLUDES_*).
        PublicDefinitions.Add("FRAMECORE_UE=1");
        PublicDefinitions.Add("EIGEN_MPL2_ONLY");

        // ---- Opt-in supernodal Cholesky lane (stage 3): conda OpenBLAS + METIS on Win64 ----
        // dumpbin confirms conda openblas.dll depends ONLY on VCRUNTIME140 + UCRT (MSVC-clean, NOT
        // MinGW — the libgcc/libgomp/libquadmath/libwinpthread DLLs in the env belong to other conda
        // packages, not openblas). conda metis is a static lib with IDXTYPEWIDTH==32, matching
        // sn_chol's idx_t=int32. We point at the conda framecore-direct env (the same dependency the
        // standalone gate already takes) rather than vendoring a ~28 MB DLL into the repo. If the env
        // is absent the lane stays OFF: FRAMECORE_SUPERNODAL is left undefined -> 0 in FrameSnChol.h
        // -> SnSolver.cpp routes to the LDLT fallback, so the UE build still succeeds without it.
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string condaSS = Environment.GetEnvironmentVariable("SUPERNODAL_CONDA");
            if (string.IsNullOrEmpty(condaSS))
                condaSS = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                    "anaconda3", "envs", "framecore-direct", "Library");

            string incOpenBlas = Path.Combine(condaSS, "include", "openblas");
            string incRoot     = Path.Combine(condaSS, "include");
            string libDir      = Path.Combine(condaSS, "lib");
            string binDir      = Path.Combine(condaSS, "bin");

            if (File.Exists(Path.Combine(libDir, "openblas.lib")) &&
                File.Exists(Path.Combine(incOpenBlas, "cblas.h")) &&
                File.Exists(Path.Combine(incRoot, "metis.h")) &&
                File.Exists(Path.Combine(binDir, "openblas.dll")))
            {
                PrivateIncludePaths.Add(incOpenBlas);   // cblas.h (SnSolver.cpp / sn_chol.h only)
                PrivateIncludePaths.Add(incRoot);       // metis.h
                PublicAdditionalLibraries.Add(Path.Combine(libDir, "openblas.lib"));
                PublicAdditionalLibraries.Add(Path.Combine(libDir, "metis.lib"));   // static, IDXTYPEWIDTH=32
                RuntimeDependencies.Add("$(TargetOutputDir)/openblas.dll",
                                        Path.Combine(binDir, "openblas.dll"));
                PublicDelayLoadDLLs.Add("openblas.dll");
                PrivateDefinitions.Add("FRAMECORE_SUPERNODAL=1");
            }
            else
            {
                System.Console.WriteLine("[FrameCore] conda OpenBLAS/METIS not found at " + condaSS +
                                         "; supernodal lane stays OFF in UE (LDLT fallback).");
            }

            // ---- Opt-in cuDSS GPU lane (Phase 7, v2.11): same conda env, headers + libs in
            // %CONDA_ROOT% rather than %CONDA_ROOT%/Library. cuDSS is NVIDIA-only and the
            // runtime DLL set is large (~600 MB), so the educational-game packaged build does
            // NOT bundle them automatically -- v2.12 will add a UE packaging recipe. For now
            // the lane is dev-only: enable bCudaEnabled if cuDSS is present in the conda env,
            // copy the runtime DLLs into TargetOutputDir for the Editor, and use PublicDelayLoad
            // so a missing DLL in a packaged game is a graceful runtime fallback to the CPU lane.
            string cudaRoot = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                "anaconda3", "envs", "framecore-direct");
            string cudssHdr   = Path.Combine(condaSS, "include", "cudss.h");
            string cudssLib   = Path.Combine(condaSS, "lib",     "cudss.lib");
            string cudartLib  = Path.Combine(cudaRoot, "lib", "x64", "cudart.lib");
            string cusparseLib= Path.Combine(cudaRoot, "lib", "x64", "cusparse.lib");
            string cudartHdr  = Path.Combine(cudaRoot, "include", "cuda_runtime.h");
            string cudssDll   = Path.Combine(condaSS, "bin", "cudss64_0.dll");
            string cudartDll  = Path.Combine(cudaRoot, "bin", "cudart64_12.dll");

            if (File.Exists(cudssHdr) && File.Exists(cudssLib) && File.Exists(cudartLib) &&
                File.Exists(cusparseLib) && File.Exists(cudartHdr) &&
                File.Exists(cudssDll) && File.Exists(cudartDll))
            {
                PrivateIncludePaths.Add(Path.Combine(cudaRoot, "include"));     // cuda_runtime.h
                PrivateIncludePaths.Add(Path.Combine(condaSS, "include"));      // cudss.h (already added above)
                PublicAdditionalLibraries.Add(cudartLib);
                PublicAdditionalLibraries.Add(cudssLib);
                PublicAdditionalLibraries.Add(cusparseLib);
                // Runtime DLLs that must sit next to FrameCore.dll in the Editor / packaged build.
                // Listed in dependency order (dependers first); UBT copies them per-target.
                string[] runtimeDlls = new string[] {
                    "cudss64_0.dll", "cudss_mtlayer_vcomp14064_0.dll",
                    "cudart64_12.dll", "cusparse64_12.dll",
                    "cublas64_12.dll", "cublasLt64_12.dll",
                    "nvJitLink_120_0.dll"
                };
                foreach (string dll in runtimeDlls) {
                    string libBin = Path.Combine(condaSS, "bin", dll);
                    string envBin = Path.Combine(cudaRoot, "bin", dll);
                    string src = File.Exists(libBin) ? libBin : (File.Exists(envBin) ? envBin : null);
                    if (src != null) {
                        RuntimeDependencies.Add("$(TargetOutputDir)/" + dll, src);
                        PublicDelayLoadDLLs.Add(dll);
                    }
                }
                PrivateDefinitions.Add("FRAMECORE_CUDA=1");
            }
            else
            {
                System.Console.WriteLine("[FrameCore] cuDSS / CUDA runtime not found under " + condaSS +
                                         " + " + cudaRoot + "; GPU lane stays OFF in UE (CPU sn_chol.h used).");
            }
        }
    }
}
