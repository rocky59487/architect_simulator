// ArchSim - UArchSimGameInstance implementation.
// Sprint S-02, unit AS-02a. Spec: docs/logs/S-02/plan_2026-06-26T1033.md § AS-02a.
//
// This TU owns:
//   1. DEFINE_LOG_CATEGORY(LogArchSim) — module-level category shared by all
//      game-body TUs that include ArchSimGameInstance.h.
//   2. FTickableGameObject virtuals (Tick / IsTickable / GetStatId).
//   3. Init / Shutdown lifetime ordering.
//
// AS-02b will add the member-sync + RequestSolve loop inside Tick() without
// changing any declaration or include in this file.

#include "ArchSimGameInstance.h"

// One and only definition of the module-level log category.
// All other game-body TUs use DECLARE_LOG_CATEGORY_EXTERN (in the header).
DEFINE_LOG_CATEGORY(LogArchSim);

// ----------------------------------------------------------------------------
// UGameInstance overrides
// ----------------------------------------------------------------------------

void UArchSimGameInstance::Init()
{
    Super::Init();

    // bIsActive is set AFTER Super::Init() so any reentrant IsTickable() query
    // triggered inside the super chain sees "inactive" and returns false early.
    // This prevents phantom ticks before the World is properly initialised.
    bIsActive = true;

    UE_LOG(LogArchSim, Display,
           TEXT("ArchSimGameInstance: Tick driver active"));
}

void UArchSimGameInstance::Shutdown()
{
    // Set bIsActive BEFORE Super::Shutdown() so any concurrent Tick() query
    // immediately bails out. Super::Shutdown() tears down subsystems; if Tick()
    // were to run while the Registry pointer is half-torn-down we'd crash.
    bIsActive = false;

    UE_LOG(LogArchSim, Display,
           TEXT("ArchSimGameInstance: Tick driver shutting down "
                "(final TickCount=%d, AccumulatedSeconds=%.2f)"),
           TickCount, AccumulatedSeconds);

    Super::Shutdown();
}

// ----------------------------------------------------------------------------
// FTickableGameObject implementation
// ----------------------------------------------------------------------------

void UArchSimGameInstance::Tick(float DeltaSeconds)
{
    // AS-02b will insert member-sync + RequestSolve here.
    // For AS-02a this body only accumulates telemetry so AS-02c can assert
    // "Tick fired at least once" without any allocations or subsystem calls.
    // NO per-frame heap allocs: ++int32 and += float are plain register ops.
    ++TickCount;
    AccumulatedSeconds += DeltaSeconds;
}

bool UArchSimGameInstance::IsTickable() const
{
    // Three conditions must ALL be true:
    //   1. bIsActive  — Init() done, Shutdown() not yet called.
    //   2. GetWorld() — filter CDO (no World) and during-teardown (World gone).
    //   3. !IsTemplate() — this is not the Class Default Object itself.
    //
    // FTickableGameObject queries IsTickable() on *every* registered instance
    // each frame, so the body must be O(1) and branch-predictor-friendly.
    // All three checks are cheap pointer / bool comparisons.
    return bIsActive && GetWorld() != nullptr && !IsTemplate();
}

TStatId UArchSimGameInstance::GetStatId() const
{
    // RETURN_QUICK_DECLARE_CYCLE_STAT declares a local static TStatId and returns
    // it. STATGROUP_Tickables is the FTickableGameObject convention group; using
    // it ensures the stat shows up under "Tickables" in Unreal Insights.
    RETURN_QUICK_DECLARE_CYCLE_STAT(UArchSimGameInstance, STATGROUP_Tickables);
}
