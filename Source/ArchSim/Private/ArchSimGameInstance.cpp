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
#include "Subsystems/ArchSimModelRegistry.h"   // AS-02b: GetRegisteredCount(), RequestSolve()
#include "FrameCoreUE/FrameCoreUEVisualTypes.h" // AS-02b: FFrameModelPatch (empty-patch ctor)

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
    // Telemetry (preserved from AS-02a; AS-02c smoke test reads these).
    // NO per-frame heap allocs: ++int32 and += float are plain register ops.
    ++TickCount;
    AccumulatedSeconds += DeltaSeconds;

    // AS-02b: registered-count delta -> RequestSolve(empty patch).
    //
    // Why this is the right "dirty" signal:
    //   - UArchSimMemberData::BeginPlay (Components/ArchSimMemberData.cpp:11-28)
    //     auto-calls Registry->RegisterMember(this). That mutates CurrentModel +
    //     IndexToComponent but does NOT call RequestSolve — verified in
    //     ArchSimModelRegistry.cpp:131-208 (no RequestSolve call anywhere in body).
    //   - UArchSimMemberData::EndPlay (cpp:30-43) auto-calls
    //     Registry->DeactivateMember which DOES call RequestSolve internally
    //     (ArchSimModelRegistry.cpp:391-393). So removals self-heal; only
    //     additions need a Tick prod.
    //   - Condition is `!=` (not `<`) so both add and remove cases update
    //     LastSeenRegisteredCount; the remove case skips the second RequestSolve
    //     (DeactivateMember already fired one) while still keeping the cache fresh.
    //   - Position-change sync (member's actor moved) is deliberately OUT of
    //     AS-02b scope: demo MVP places static buildings, so no position dirty
    //     case fires. That sync is a future AS-XX.

    UArchSimModelRegistry* Registry = GetSubsystem<UArchSimModelRegistry>();
    if (!Registry)
    {
        // Subsystem teardown order can null this between Init and Shutdown in
        // PIE-stop edge cases. Bail without altering TickCount/AccumulatedSeconds
        // telemetry — those counters are already updated above.
        return;
    }

    const int32 CurrentCount = Registry->GetRegisteredCount();
    if (CurrentCount != LastSeenRegisteredCount)
    {
        // Registration delta detected. Emit an empty patch so RequestSolve's
        // 150 ms debounce coalesces a burst (PIE map-load fires BeginPlay on
        // dozens of placed members in one frame; we batch them into ONE solve).
        //
        // Empty FFrameModelPatch{} has PatchRank(P) == 0 (no toggle IDs).
        // So PendingRankAccumulation += 0, which never trips the strict-`>`
        // rebaseline ceiling at ArchSimModelRegistry.cpp:281. Safe.
        //
        // Note (S-02 review C-03): `GetRegisteredCount()` returns
        // `IndexToComponent.Num()`, which DeactivateMember does NOT shrink (the
        // map entry stays so SaveGame round-trips remain stable). So a remove
        // does not produce a `<` delta here in practice; the `!=` comparison is
        // chosen anyway for symmetry — if future work ever does shrink the
        // map, this code does not need to change. DeactivateMember already
        // emits its own RequestSolve at ArchSimModelRegistry.cpp:393, so a
        // hypothetical second call from here would only extend the 150 ms
        // debounce window with rank 0 — never double-fires the solve.
        Registry->RequestSolve(FFrameModelPatch{});
        ++SolveTriggerCount;
        LastSeenRegisteredCount = CurrentCount;
    }
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
