// ArchSim - UArchSimModelRegistry : GameInstance-scope structural model owner.
// Sprint S-01 deliverable A1-02..A1-05 (A1-06 stub for soft delete);
// S-09 AS-41-u1 added the additive v2-persistence API (RestoreLibraries /
// ApplyFixityAt / Inject* / SetMemberFlags / FindOrAddNodePublic) — see below.
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

    // ---- AS-30: boundary support API -------------------------------------------
    // Register a fully-fixed support node at the given FrameCore-mm position.
    // WHY: K1/K2/K4 placed without any Fixed nodes produce a mechanism (12-DOF-free
    // model for a 3-member portal frame) → LDLT rejects → no heatmap. This API
    // lets SpawnDefaultPortalFrame (and future widget UI) pin base nodes before solve.
    //
    // Internally calls private FindOrAddNode for 1 mm tolerance node dedupe, then
    // writes Fixed (length 6 enforced by FrameCore marshal layer = [Ux,Uy,Uz,Rx,Ry,Rz]).
    //
    // Idempotent: repeated calls at the same position re-confirm Fixed=all-true
    // without duplicating or changing any node. Node dedup is fully delegated to the
    // existing FindOrAddNode linear-scan logic — no separate dedup path here.
    //
    // Does NOT trigger a solve (fixed-node registration is part of model topology
    // setup; the caller should batch topology changes and request solve separately,
    // or let RegisterMember's next call coalesce into the debounced solve).
    //
    // Returns NodeIdx (>= 0) on success, -1 on validation failure (PosMm contains
    // NaN or the Nodes array is unexpectedly full; both conditions are logged).
    [[nodiscard]] int32 RegisterFixedSupport(const FVector& PosMm);

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

    // ---- AS-08-u1: persistence reset (called by UArchSimPersistenceSubsystem) ---
    // Tear down any live FrameCore session and return the Registry to the same
    // blank state as immediately after Initialize(). Called before replaying a
    // loaded sidecar so stale member/node data does not accumulate.
    //
    // WHY here: UArchSimPersistenceSubsystem needs to clear the model without
    // being a friend; a dedicated method with a single purpose is cleaner than
    // adding full-access or duplicating teardown logic.
    //
    // Contract: after Reset() returns, GetCurrentModel().Members/Nodes are empty,
    // GetRegisteredCount()==0, IsSessionStarted()==false.
    void Reset();

    // ---- AS-41-u1: v2 persistence additive API (called by UArchSimPersistenceSubsystem) ---

    // Restore a material library array directly into CurrentModel.Materials.
    // MUST be called before RegisterMember so MatIdx 0..N-1 are valid.
    // Precondition: CurrentModel.Materials is empty (i.e., called after Reset()).
    // WHY additive not setter: callers should only call once per replay; using
    // this instead of touching CurrentModel directly keeps the Registry as the
    // single owner of CurrentModel mutations.
    // [[nodiscard]]: returns false if Materials is already non-empty (caller error).
    [[nodiscard]] bool RestoreLibraries(const TArray<FFrameMaterial>& Materials,
                                        const TArray<FFrameSection>& Sections);

    // Apply per-node fixity to a node identified by world position (mm).
    // Finds the node via the same 1mm tolerance linear scan as FindOrAddNode.
    // Does NOT create a new node — only patches fixity on an existing node.
    // Called after members/supports are replayed (nodes must already exist).
    // Returns true if the node was found and patched; false if not found (logged).
    [[nodiscard]] bool ApplyFixityAt(const FVector& NodePosMm,
                                     const TArray<bool>& Fixed,
                                     const TArray<float>& Prescribed);

    // Inject model state (loads, shells) directly into CurrentModel from an
    // authoritative FFrameModelDef snapshot. Used by v2 replay to set loads/UDLs/
    // shells after member/node reconstruction without triggering a solve.
    // WHY not expose CurrentModel mutably: selective injection keeps the contract
    // surface minimal; replay code populates exactly what was snapshotted.
    //
    // InjectNodalLoads: appends NodalLoads entries (node Id matched by index 0..N-1
    // where N = CurrentModel.Nodes.Num() at inject time). Caller is responsible for
    // ensuring node-id alignment (replay order builds nodes in the same order).
    void InjectNodalLoads(const TArray<FFrameNodalLoad>& Loads);

    // InjectMemberUDLs: appends MemberUDL entries. Member field is the FrameCore
    // user id (== MemberIdx in our scheme; caller maps record index to member id).
    void InjectMemberUDLs(const TArray<FFrameMemberUDL>& UDLs);

    // InjectShells: appends FFrameShellQuad entries. Caller builds node arrays
    // using FindOrAddNodePublic for position-based node lookup.
    void InjectShells(const TArray<FFrameShellQuad>& ShellQuads);

    // InjectShellPressures: appends pressure entries. Shell field is user id.
    void InjectShellPressures(const TArray<FFrameShellPressure>& Pressures);

    // SetMemberFlags: write bTensionOnly and Release onto a specific member row.
    // WHY separate from RegisterMember: RegisterMember only accepts a component
    // (actor-bound geometry); it has no parameter for bTensionOnly / Release.
    // These are post-placement flags that are applied after RegisterMember assigns
    // the member row. The sidecar replay path records non-default values (audit
    // finding #5) and calls this immediately after RegisterMember to restore them.
    // WHY MemberIdx not member-id: the two are equal in our scheme (RegisterMember
    // keeps Id == MemberIdx), but MemberIdx is the internal array index and is
    // what ReplayLoadedSidecar obtains from RegisterMember's return value.
    // Preconditions: MemberIdx must be a valid index into CurrentModel.Members
    // (i.e., RegisterMember returned it successfully). Release must be either
    // empty (leaves the row's existing 12-false array untouched) or length 12.
    // Returns false + logs if preconditions are not met.
    [[nodiscard]] bool SetMemberFlags(int32 MemberIdx,
                                      bool bTensionOnly,
                                      const TArray<bool>& Release);

    // FindOrAddNodePublic: expose FindOrAddNode for replay callers that need to
    // convert world positions to node ids for load/shell injection.
    // Same 1mm tolerance as the private overload; EnsureDefaultLibraries not called.
    // WHY public for v2 replay: shell-node building must happen outside RegisterMember
    // (shells have no actor component; ReplayShells drives this).
    [[nodiscard]] int32 FindOrAddNodePublic(const FVector& PosMm);

    // ---- read-only accessors (tests / heatmap / HUD) ---------------------------
    [[nodiscard]] const FFrameModelDef& GetCurrentModel() const { return CurrentModel; }
    [[nodiscard]] int32 GetRegisteredCount() const { return IndexToComponent.Num(); }
    [[nodiscard]] bool  IsSessionStarted() const { return bSessionStarted; }

    // ---- AS-10: rebaseline telemetry (pure observers) -------------------------
    // PendingRankAccumulation accumulates per-call PatchRank inside RequestSolve
    // (see RequestSolve body in .cpp — look for `PendingRankAccumulation +=`).
    // It resets to 0 in ExecuteSolve after any solve attempt — rebaseline or not
    // (see ExecuteSolve top + 3 early-exit paths in .cpp — look for
    // `PendingRankAccumulation = 0`).
    // NOTE: in headless (NewObject, no GameInstance), RequestSolve early-returns
    // at the GI-null guard before reaching the trip check — so accum grows but is
    // NEVER reset by ExecuteSolve in headless mode (no timer/solve fires).
    // This getter is exposed for AS-10 test that pins the strict `> MaxRankBeforeRebaseline`
    // trip semantic (97th cumulative rank trips rebaseline, NOT 96th) while acknowledging
    // the headless limitation in test comments.
    [[nodiscard]] int32 GetPendingRankAccumulation() const noexcept { return PendingRankAccumulation; }

    // bNeedsRebaseline is set true when the >MaxRankBeforeRebaseline threshold
    // trips inside RequestSolve (see `bNeedsRebaseline = true` in .cpp).
    // Clears inside ExecuteSolve's rebaseline branch (see `bNeedsRebaseline = false`
    // after Sub->Rebaseline() call). In headless mode (no GI), this flag is never
    // set because the trip path is unreachable (GI-null early-return prevents it).
    [[nodiscard]] bool IsRebaselineDue() const noexcept { return bNeedsRebaseline; }

    // Public mirror of the private constexpr for test assertion clarity (no
    // behaviour change; lets the test write assertions in terms of the real
    // constant rather than a magic literal 96).
    //
    // TODO(AS-12): consumer plan — when the HUD "rank budget" indicator ships
    // (out of S-03 scope), it should read this via Registry->GetMaxRankBeforeRebaseline()
    // so the on-screen budget UI stays in sync with the engine pin. Until then,
    // this accessor is intentionally test-only (no production caller).
    [[nodiscard]] static constexpr int32 GetMaxRankBeforeRebaseline() noexcept { return MaxRankBeforeRebaseline; }

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
