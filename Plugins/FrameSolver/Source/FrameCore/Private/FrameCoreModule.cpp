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
class FFrameCoreModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FString root = FPlatformMisc::GetEnvironmentVariable(TEXT("SUPERNODAL_CONDA"));
        if (root.IsEmpty())
        {
            const FString home = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
            root = FPaths::Combine(home, TEXT("anaconda3"), TEXT("envs"), TEXT("framecore-direct"), TEXT("Library"));
        }
        const FString dll = FPaths::Combine(root, TEXT("bin"), TEXT("openblas.dll"));
        if (FPaths::FileExists(dll))
        {
            OpenBlasHandle = FPlatformProcess::GetDllHandle(*dll);
        }
    }

    virtual void ShutdownModule() override
    {
        if (OpenBlasHandle)
        {
            FPlatformProcess::FreeDllHandle(OpenBlasHandle);
            OpenBlasHandle = nullptr;
        }
    }

private:
    void* OpenBlasHandle = nullptr;
};

IMPLEMENT_MODULE(FFrameCoreModule, FrameCore);
#else
IMPLEMENT_MODULE(FDefaultModuleImpl, FrameCore);
#endif
