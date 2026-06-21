// UE module entry point (UE-only TU; excluded from the standalone build).
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if FRAMECORE_SUPERNODAL
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

// When the supernodal lane is compiled in (conda OpenBLAS found at configure time, see
// FrameCore.Build.cs), sn_chol.h DELAY-loads openblas.dll on its first cblas/LAPACKE call. The DLL
// lives in the conda framecore-direct env, which is NOT on the editor/game DLL search path, so the
// delay-load helper would fault. We preload it here by full path; the delay-load thunk then resolves
// against the already-loaded module (matched by base name "openblas.dll"). Path resolution mirrors
// FrameCore.Build.cs (SUPERNODAL_CONDA env var, else the default anaconda3 location). If the DLL is
// absent the handle stays null -- acceptable in dev where Build.cs only sets FRAMECORE_SUPERNODAL=1
// when the env exists; a shipping build must instead stage openblas.dll next to the executable.
//
// Phase 7 (v2.11): when FRAMECORE_CUDA=1, the same StartupModule preloads the cuDSS /
// cuSPARSE / cudart runtime DLL group from the same conda env. Build.cs registers them as
// delay-load DLLs, so a missing DLL in a packaged game lets the GPU lane gracefully fall
// back to the CPU one rather than failing to load FrameCore.dll.
class FFrameCoreModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FString libraryRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("SUPERNODAL_CONDA"));
        if (libraryRoot.IsEmpty())
        {
            const FString home = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
            libraryRoot = FPaths::Combine(home, TEXT("anaconda3"), TEXT("envs"), TEXT("framecore-direct"), TEXT("Library"));
        }
        // openblas (supernodal CPU)
        const FString openblasDll = FPaths::Combine(libraryRoot, TEXT("bin"), TEXT("openblas.dll"));
        if (FPaths::FileExists(openblasDll))
        {
            OpenBlasHandle = FPlatformProcess::GetDllHandle(*openblasDll);
            if (!OpenBlasHandle)
            {
                UE_LOG(LogTemp, Warning, TEXT("[FrameCore] openblas.dll exists at '%s' but GetDllHandle returned null; supernodal lane will delay-fault on first use."), *openblasDll);
            }
        }
#if FRAMECORE_CUDA
        // cuDSS / cuSPARSE / cudart (GPU lane). The conda env keeps cudart64_12.dll under the
        // env root's bin/, while cuDSS + cuSPARSE + transitive deps sit under Library/bin/.
        // Mirror the standalone build_sn_cuda.bat + Build.cs resolution.
        // v2.11.1 (D-01b audit): derive envRoot from SUPERNODAL_CONDA when set instead of always
        // pinning %USERPROFILE%\anaconda3\envs\framecore-direct -- otherwise Miniconda or a
        // custom env name silently breaks the CUDA preload while the supernodal preload works.
        FString envRoot;
        if (!libraryRoot.IsEmpty())
        {
            FString trimmed = libraryRoot;
            trimmed.RemoveFromEnd(TEXT("/"));
            trimmed.RemoveFromEnd(TEXT("\\"));
            if (FPaths::GetCleanFilename(trimmed).Equals(TEXT("Library"), ESearchCase::IgnoreCase))
            {
                envRoot = FPaths::GetPath(trimmed);
            }
            else
            {
                envRoot = trimmed;
            }
        }
        else
        {
            const FString home = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
            envRoot = FPaths::Combine(home, TEXT("anaconda3"), TEXT("envs"), TEXT("framecore-direct"));
        }
        const TCHAR* dllNames[] = {
            TEXT("nvJitLink_120_0.dll"),
            TEXT("cublasLt64_12.dll"),
            TEXT("cublas64_12.dll"),
            TEXT("cusparse64_12.dll"),
            TEXT("cudart64_12.dll"),
            TEXT("cudss_mtlayer_vcomp14064_0.dll"),
            TEXT("cudss64_0.dll"),
        };
        for (const TCHAR* name : dllNames)
        {
            const FString libBin = FPaths::Combine(libraryRoot, TEXT("bin"), name);
            const FString envBin = FPaths::Combine(envRoot, TEXT("bin"), name);
            const FString src = FPaths::FileExists(libBin) ? libBin : (FPaths::FileExists(envBin) ? envBin : FString());
            if (!src.IsEmpty())
            {
                void* h = FPlatformProcess::GetDllHandle(*src);
                if (!h)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[FrameCore] CUDA DLL '%s' found at '%s' but GetDllHandle returned null; GPU lane will delay-fault."), name, *src);
                }
                CudaHandles.Add(h);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[FrameCore] CUDA DLL '%s' not found under '%s' or '%s'; GPU lane will fall back to CPU."), name, *libraryRoot, *envRoot);
            }
        }
#endif
    }

    virtual void ShutdownModule() override
    {
        if (OpenBlasHandle)
        {
            FPlatformProcess::FreeDllHandle(OpenBlasHandle);
            OpenBlasHandle = nullptr;
        }
#if FRAMECORE_CUDA
        for (void* h : CudaHandles)
        {
            if (h) FPlatformProcess::FreeDllHandle(h);
        }
        CudaHandles.Empty();
#endif
    }

private:
    void* OpenBlasHandle = nullptr;
#if FRAMECORE_CUDA
    TArray<void*> CudaHandles;
#endif
};

IMPLEMENT_MODULE(FFrameCoreModule, FrameCore);
#else
IMPLEMENT_MODULE(FDefaultModuleImpl, FrameCore);
#endif
