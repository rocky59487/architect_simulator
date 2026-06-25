// ArchSim - UArchSimModelRegistry : GameInstance-scope structural model owner.
// Sprint S-01 deliverable A1-02..A1-05 (A1-06 stub for soft delete).
//
// Responsibilities:
//   * Own the FFrameModelDef that mirrors all placed UArchSimMemberData in the world.
//   * Auto-insert a default material (S275) and section (200x200mm) on first register
//     so the simplest gameplay path needs no prefab metadata.
//   * Debounce ApplyPatchAndResolve calls so a burst of placements / removals
//     produces a single Solve (DebounceMs = 150).
//   * Distribute each Solve's per-member Peak.Risk back to the component for the
//     heatmap (Part C2) and HUD (Part D2) to consume.
//
// Lifetime: GameInstanceSubsystem -- one per PIE / runtime session. Get(World) is
// nullptr only during very early startup (before GameInstance::Init).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"      // FFrameModelDef, FFrameSolveOptions, FFrameReanalysisOptions
#include "FrameCoreUE/FrameCoreUEResultTypes.h"     // FFrameSolveResult
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"     // FFrameModelPatch
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"   // (transitively required by FrameInteractiveSubsystem.h)
#include "ArchSimModelRegistry.generated.h"

class UArchSimMemberData;
class UFrameInteractiveSubsystem;

// Fired after each successful Solve (singular results are NOT broadcast).
// Subscribers: heatmap actor, HUD analysis panel, learning-log subsystem.
DECLARE_MULTICAST_DELEGATE_OneParam(FArchSimOnSolveComplete, const FFrameSolveResult& /*Result*/);

UCLASS()
class ARCHSIM_API UArchSimModelRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Convenience accessor. Returns nullptr if the world has no GameInstance yet
    // (early PIE startup).
    static UArchSimModelRegistry* Get(UWorld* World);

    // UGameInstanceSubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ---- A1-03: registration ----------------------------------------------------
    // Reads Comp's owning Actor transform + EndIOffsetUE/EndJOffsetUE, converts to
    // FrameCore mm, dedupes nodes (1 mm tolerance, linear scan), inserts an
    // FFrameMember row, and returns the assigned MemberIdx (or -1 on validation
    // failure -- nullptr Owner, zero-length axis, etc).
    int32 RegisterMember(UArchSimMemberData* Comp);

    // Hand the current model to UFrameInteractiveSubsystem and start a session.
    // Idempotent on the first call after construction; subsequent calls (after a
    // model rebuild) end the prior session first. Returns false + logs on failure.
    bool FlushAndStartSession();

    // ---- A1-04: debounced resolve ----------------------------------------------
    // Public entry: merge `Patch` into PendingPatch and (re)arm a DebounceMs timer.
    // Multiple calls within DebounceMs collapse to one Solve.
    void RequestSolve(const FFrameModelPatch& Patch);

    // Tag the next ExecuteSolve to first call Rebaseline() (cleared after).
    void MarkNeedsRebaseline() { bNeedsRebaseline = true; }

    // ---- A1-05: result distribution --------------------------------------------
    // Walks Result.MemberUtilization[] and writes Peak.Risk into each registered
    // Component's CachedUtilization. No-op when Result.bSingular.
    void DistributeSolveResult(const FFrameSolveResult& Result);

    FArchSimOnSolveComplete OnSolveComplete;

    // ---- A1-06: soft delete (stub; full impl deferred to its own task) ---------
    // Marks the model row inactive and queues a Deactivate patch. The mapping
    // entry is left in place so SaveGame round-trips remain stable.
    void DeactivateMember(int32 MemberIdx);

    // ---- read-only accessors (tests / heatmap / HUD) ---------------------------
    [[nodiscard]] const FFrameModelDef& GetCurrentModel() const { return CurrentModel; }
    [[nodiscard]] int32 GetRegisteredCount() const { return IndexToComponent.Num(); }
    [[nodiscard]] bool  IsSessionStarted() const { return bSessionStarted; }

private:
    // Owned aggregate. Internal (not UPROPERTY): no need for GC reflection, and
    // FFrameModelDef contains TArray<FFrameNode> with nested TArray<bool> -- keeping
    // it internal makes the clear-on-restart path trivial.
    FFrameModelDef CurrentModel;

    // MemberIdx -> Component. Weak: a destroyed actor's component is auto-pruned
    // by IsValid() checks at distribute time.
    TMap<int32, TWeakObjectPtr<UArchSimMemberData>> IndexToComponent;

    // Monotonically-increasing allocator. Soft-deleted slots are NOT reused;
    // FrameCore patch semantics expect stable user ids across the session.
    int32 NextMemberIdx = 0;

    // ---- debounce state --------------------------------------------------------
    FFrameModelPatch PendingPatch;
    FTimerHandle DebounceTimer;
    int32 PendingRankAccumulation = 0;
    static constexpr int32 DebounceMs = 150;
    // Matches FFrameReanalysisOptions::MaxRank default. Above this we force a
    // Rebaseline before the next Solve to bound the ladder cost.
    static constexpr int32 MaxRankBeforeRebaseline = 96;

    bool bSessionStarted   = false;
    bool bNeedsRebaseline  = false;

    UFrameInteractiveSubsystem* GetFrameSubsystem() const;
    void ExecuteSolve();

    // Linear-scan node dedupe; PosMm in FrameCore millimetres.
    // O(N) is acceptable up to the MVP budget (<=500 members per level).
    int32 FindOrAddNode(const FVector& PosMm);

    // Pick a RefVec that is not (anti)parallel to the member axis. Default member
    // RefVec is +Z; for a vertical column the axis is also +Z which would alias
    // the local frame, so we fall back to +X. The 0.999 threshold maps to ~2.6 deg.
    static FVector PickRefVecForAxis(const FVector& AxisUnit);

    // Insert default S275 + 200x200mm rect on first register so a freshly placed
    // member with MaterialId=0/SectionId=0 has something to reference.
    void EnsureDefaultLibraries();
};
