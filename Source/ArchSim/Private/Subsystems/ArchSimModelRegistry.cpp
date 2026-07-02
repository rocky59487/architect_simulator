#include "Subsystems/ArchSimModelRegistry.h"
#include "Components/ArchSimMemberData.h"

#include "FrameCoreUE/FrameInteractiveSubsystem.h"
#include "FrameCoreUE/FrameCoreUEMaterialLibrary.h"
#include "FrameCoreUE/FrameCoreUESectionLibrary.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogArchSimRegistry, Log, All);

// 1 mm node-merge tolerance, in FrameCore millimetres.
static constexpr double kNodeMergeTolMm = 1.0;
// Vertical-axis test for RefVec: |dot(axis, +Z)| > 0.999 means the axis is within
// ~2.6 degrees of vertical and the default RefVec=+Z would alias the local frame.
static constexpr double kVerticalAxisDot = 0.999;
// UE cm -> FrameCore mm scale.
static constexpr double kCmToMm = 10.0;

// ----- static accessor -------------------------------------------------------

UArchSimModelRegistry* UArchSimModelRegistry::Get(UWorld* World)
{
    if (!World) return nullptr;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return nullptr;
    return GI->GetSubsystem<UArchSimModelRegistry>();
}

UFrameInteractiveSubsystem* UArchSimModelRegistry::GetFrameSubsystem() const
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return nullptr;
    return GI->GetSubsystem<UFrameInteractiveSubsystem>();
}

// ----- A1-02: lifecycle ------------------------------------------------------

void UArchSimModelRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CurrentModel = FFrameModelDef{};
    IndexToComponent.Reset();
    NextMemberIdx          = 0;
    PendingPatch           = FFrameModelPatch{};
    PendingRankAccumulation = 0;
    bSessionStarted        = false;
    bNeedsRebaseline       = false;
}

void UArchSimModelRegistry::Deinitialize()
{
    // Clear the debounce timer before the world tears down to avoid a dangling
    // callback that would fire after our destructor.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWorld* World = GI->GetWorld())
        {
            World->GetTimerManager().ClearTimer(DebounceTimer);
        }
    }

    // End the engine session if one is live.
    if (bSessionStarted)
    {
        if (UFrameInteractiveSubsystem* Sub = GetFrameSubsystem())
        {
            Sub->EndSession();
        }
        bSessionStarted = false;
    }

    IndexToComponent.Reset();
    CurrentModel = FFrameModelDef{};

    Super::Deinitialize();
}

// ----- AS-08-u1: persistence reset -------------------------------------------

void UArchSimModelRegistry::Reset()
{
    // End any live FrameCore session before wiping the model. The same teardown
    // order as Deinitialize() so the engine session is cleanly closed.
    if (bSessionStarted)
    {
        if (UFrameInteractiveSubsystem* Sub = GetFrameSubsystem())
        {
            Sub->EndSession();
        }
        bSessionStarted = false;
    }

    // Clear the debounce timer so no stale ExecuteSolve fires after Reset().
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWorld* World = GI->GetWorld())
        {
            World->GetTimerManager().ClearTimer(DebounceTimer);
        }
    }

    // Clear the bRegistered / MemberIdx flags on every still-valid component before
    // wiping IndexToComponent. WHY: the BeginPlay idempotency guard in RegisterMember
    // checks bRegistered first (ArchSimModelRegistry.cpp:RegisterMember L219-228). If
    // Reset() wipes the map without clearing those flags, the SAME component object
    // that was previously registered cannot be re-registered on the rebuilt model (the
    // guard short-circuits and returns the old stale MemberIdx). This breaks replay and
    // any scenario where the same actor survives across a Reset() → re-register cycle.
    for (auto& Pair : IndexToComponent)
    {
        if (UArchSimMemberData* Comp = Pair.Value.Get())
        {
            // WHY set MemberIdx = -1 before bRegistered = false: if some other thread
            // or callback reads bRegistered after we flip it to false but before we
            // update MemberIdx, it will see an unregistered component with a valid
            // (but now invalid) idx. Writing MemberIdx first keeps the pair consistent.
            Comp->MemberIdx   = -1;
            Comp->bRegistered = false;
        }
        // Stale (null) weak-ptrs are also cleared implicitly when the map is Reset() below.
    }

    // Return all fields to Initialize()-equivalent blank state.
    CurrentModel            = FFrameModelDef{};
    IndexToComponent.Reset();
    NextMemberIdx           = 0;
    PendingPatch            = FFrameModelPatch{};
    PendingRankAccumulation = 0;
    bNeedsRebaseline        = false;

    UE_LOG(LogArchSimRegistry, Display, TEXT("Reset(): Registry cleared; ready for replay."));
}

// ----- helpers ---------------------------------------------------------------

void UArchSimModelRegistry::EnsureDefaultLibraries()
{
    if (CurrentModel.Materials.Num() == 0)
    {
        // Default material: S275 mild steel. Components with MaterialId=0 reference this.
        CurrentModel.Materials.Add(UFrameMaterialLibrary::GetS275());
    }
    if (CurrentModel.Sections.Num() == 0)
    {
        // Default section: 200 mm x 200 mm solid rectangular (b=width, d=depth).
        CurrentModel.Sections.Add(UFrameSectionLibrary::MakeRectangular(200.f, 200.f));
    }
}

int32 UArchSimModelRegistry::FindOrAddNode(const FVector& PosMm)
{
    // Linear scan. MVP budget caps level size at <=500 members which means at most
    // ~1000 nodes; an O(N) scan stays well under a millisecond. Phase 2 may swap
    // this for a spatial hash if benchmarks demand it.
    for (int32 i = 0; i < CurrentModel.Nodes.Num(); ++i)
    {
        if (FVector::Dist(CurrentModel.Nodes[i].Pos, PosMm) < kNodeMergeTolMm)
        {
            return i;
        }
    }

    FFrameNode Node;
    Node.Id  = CurrentModel.Nodes.Num();
    Node.Pos = PosMm;
    Node.Fixed.Init(false, 6);       // length-6 enforced by the FrameCore marshal layer
    Node.Prescribed.Init(0.f, 6);    // length-6 likewise
    return CurrentModel.Nodes.Add(Node);
}

FVector UArchSimModelRegistry::PickRefVecForAxis(const FVector& AxisUnit)
{
    // Default RefVec is +Z. For a near-vertical axis +Z aliases the member axis
    // and engine localAxes() falls into a degenerate cross-product; pick +X.
    if (FMath::Abs(AxisUnit.Z) > kVerticalAxisDot)
    {
        return FVector(1.0, 0.0, 0.0);
    }
    return FVector(0.0, 0.0, 1.0);
}

// ----- AS-30: boundary support API ------------------------------------------

int32 UArchSimModelRegistry::RegisterFixedSupport(const FVector& PosMm)
{
    // Validate position — NaN coords would corrupt the node array silently.
    if (PosMm.ContainsNaN())
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("RegisterFixedSupport: PosMm contains NaN (%.2f,%.2f,%.2f); rejected."),
               PosMm.X, PosMm.Y, PosMm.Z);
        return -1;
    }

    // WHY EnsureDefaultLibraries here: FindOrAddNode may be the very first call
    // on a fresh Registry (support placed before any member). The default material +
    // section must exist before a subsequent RegisterMember call uses idx 0.
    // This mirrors what RegisterMember does at its own entry point.
    EnsureDefaultLibraries();

    // Delegate dedup entirely to the existing 1 mm tolerance linear-scan.
    // WHY NOT reimplement: FindOrAddNode is the single source of truth for node
    // dedup; duplicating the scan here would risk drift and violates DRY.
    const int32 NodeIdx = FindOrAddNode(PosMm);

    if (!CurrentModel.Nodes.IsValidIndex(NodeIdx))
    {
        // Unexpected: FindOrAddNode returned an out-of-range index.
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("RegisterFixedSupport: FindOrAddNode returned out-of-range idx=%d "
                    "(Nodes.Num()=%d); rejected."),
               NodeIdx, CurrentModel.Nodes.Num());
        return -1;
    }

    // Write Fixed = [true x6]. SetNum is a no-op if length is already 6 (idempotent).
    // WHY Init(true,6) unconditionally: the node may have been previously added with
    // all-free fixity by a RegisterMember call. Overwriting with all-true is correct
    // and idempotent (a second RegisterFixedSupport call at the same point does the
    // same write — net effect is unchanged).
    TArray<bool>& FixedDofs = CurrentModel.Nodes[NodeIdx].Fixed;
    FixedDofs.Init(true, 6);   // length-6 enforced by FrameCore marshal layer

    // A topology change (fixity of an existing node changed, or a new fixed node added)
    // invalidates any open FrameCore session. WHY: the session's factored system was
    // built from the model BEFORE this fixity change; continuing to use it would produce
    // incorrect results because the boundary conditions in the stiffness matrix do not
    // match the new Fixed flags. We must force a full re-baseline on the next ExecuteSolve.
    //
    // Mechanism: end the session now (same path as RegisterMember L281-290) so that the
    // next ExecuteSolve calls FlushAndStartSession on the updated model. This is correct
    // and safe:
    //   1. EndSession is idempotent (FrameInteractiveSubsystem.cpp: `if (Session) { ... }`).
    //   2. The debounce timer still fires ExecuteSolve normally; we are NOT kicking a solve
    //      here (see header note: "Does NOT trigger a solve").
    //   3. The bSessionStarted=false + PendingPatch/Rank reset mirrors RegisterMember's
    //      session-restart path, ensuring the next solve rebuilds from scratch.
    //
    // WHY NOT just MarkNeedsRebaseline(): Rebaseline keeps the existing factor and
    // only re-solves from the current state. It does NOT re-assemble K or update DOF
    // boundary conditions. Changing Fixed flags requires a full FlushAndStartSession
    // (re-assembly from the updated FFrameModelDef).
    if (bSessionStarted)
    {
        if (UFrameInteractiveSubsystem* Sub = GetFrameSubsystem())
        {
            Sub->EndSession();
        }
        bSessionStarted = false;
        PendingPatch            = FFrameModelPatch{};
        PendingRankAccumulation = 0;

        UE_LOG(LogArchSimRegistry, Display,
               TEXT("RegisterFixedSupport: ended stale session (fixity change invalidates K matrix)."));
    }

    UE_LOG(LogArchSimRegistry, Display,
           TEXT("RegisterFixedSupport: NodeIdx=%d at (%.1f,%.1f,%.1f) mm → Fixed=[T,T,T,T,T,T]."),
           NodeIdx, PosMm.X, PosMm.Y, PosMm.Z);

    return NodeIdx;
}

// ----- AS-41-u1: v2 persistence additive API ---------------------------------

bool UArchSimModelRegistry::RestoreLibraries(const TArray<FFrameMaterial>& Materials,
                                              const TArray<FFrameSection>& Sections)
{
    if (CurrentModel.Materials.Num() > 0 || CurrentModel.Sections.Num() > 0)
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("RestoreLibraries: CurrentModel already has %d materials / %d sections. "
                    "Must be called after Reset() on an empty model. Skipping."),
               CurrentModel.Materials.Num(), CurrentModel.Sections.Num());
        return false;
    }
    CurrentModel.Materials = Materials;
    CurrentModel.Sections  = Sections;

    UE_LOG(LogArchSimRegistry, Display,
           TEXT("RestoreLibraries: installed %d materials, %d sections."),
           CurrentModel.Materials.Num(), CurrentModel.Sections.Num());
    return true;
}

bool UArchSimModelRegistry::ApplyFixityAt(const FVector& NodePosMm,
                                           const TArray<bool>& Fixed,
                                           const TArray<float>& Prescribed)
{
    if (NodePosMm.ContainsNaN())
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("ApplyFixityAt: NodePosMm contains NaN; skipped."));
        return false;
    }
    if (Fixed.Num() != 6 || Prescribed.Num() != 6)
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("ApplyFixityAt: Fixed.Num()=%d / Prescribed.Num()=%d; both must be 6. Skipped."),
               Fixed.Num(), Prescribed.Num());
        return false;
    }

    // Linear scan (same tolerance as FindOrAddNode).
    for (int32 i = 0; i < CurrentModel.Nodes.Num(); ++i)
    {
        if (FVector::Dist(CurrentModel.Nodes[i].Pos, NodePosMm) < kNodeMergeTolMm)
        {
            CurrentModel.Nodes[i].Fixed      = Fixed;
            CurrentModel.Nodes[i].Prescribed = Prescribed;
            return true;
        }
    }

    UE_LOG(LogArchSimRegistry, Warning,
           TEXT("ApplyFixityAt: no node found within 1 mm of (%.1f,%.1f,%.1f) mm; skipped."),
           NodePosMm.X, NodePosMm.Y, NodePosMm.Z);
    return false;
}

void UArchSimModelRegistry::InjectNodalLoads(const TArray<FFrameNodalLoad>& Loads)
{
    CurrentModel.NodalLoads.Append(Loads);
    UE_LOG(LogArchSimRegistry, Display,
           TEXT("InjectNodalLoads: appended %d loads (total now %d)."),
           Loads.Num(), CurrentModel.NodalLoads.Num());
}

void UArchSimModelRegistry::InjectMemberUDLs(const TArray<FFrameMemberUDL>& UDLs)
{
    CurrentModel.MemberUDLs.Append(UDLs);
    UE_LOG(LogArchSimRegistry, Display,
           TEXT("InjectMemberUDLs: appended %d UDLs (total now %d)."),
           UDLs.Num(), CurrentModel.MemberUDLs.Num());
}

void UArchSimModelRegistry::InjectShells(const TArray<FFrameShellQuad>& ShellQuads)
{
    CurrentModel.Shells.Append(ShellQuads);
    UE_LOG(LogArchSimRegistry, Display,
           TEXT("InjectShells: appended %d shells (total now %d)."),
           ShellQuads.Num(), CurrentModel.Shells.Num());
}

void UArchSimModelRegistry::InjectShellPressures(const TArray<FFrameShellPressure>& Pressures)
{
    CurrentModel.ShellPressures.Append(Pressures);
    UE_LOG(LogArchSimRegistry, Display,
           TEXT("InjectShellPressures: appended %d pressures (total now %d)."),
           Pressures.Num(), CurrentModel.ShellPressures.Num());
}

int32 UArchSimModelRegistry::FindOrAddNodePublic(const FVector& PosMm)
{
    return FindOrAddNode(PosMm);
}

bool UArchSimModelRegistry::SetMemberFlags(int32 MemberIdx,
                                            bool bInTensionOnly,
                                            const TArray<bool>& InRelease)
{
    // Validate index — caller must have a valid RegisterMember return value.
    if (!CurrentModel.Members.IsValidIndex(MemberIdx))
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("SetMemberFlags: MemberIdx=%d out of range (%d members); ignored."),
               MemberIdx, CurrentModel.Members.Num());
        return false;
    }

    // Release must be empty (leave existing) or exactly 12 elements.
    // WHY 12: FFrameMember::Release is a 12-bool DOF release array [node-i 6][node-j 6].
    // The marshal layer enforces length-12; we apply the same constraint here so
    // the stored values are always engine-compatible.
    if (!InRelease.IsEmpty() && InRelease.Num() != 12)
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("SetMemberFlags: MemberIdx=%d Release has %d elements (expected 0 or 12); ignored."),
               MemberIdx, InRelease.Num());
        return false;
    }

    FFrameMember& M = CurrentModel.Members[MemberIdx];
    M.bTensionOnly = bInTensionOnly;
    if (!InRelease.IsEmpty())
    {
        M.Release = InRelease;
    }
    // No solve kicked here — these flags only affect the next Solve; caller
    // batches all flag writes and then RequestSolve once at replay end.
    return true;
}

// ----- A1-03: registration ---------------------------------------------------

int32 UArchSimModelRegistry::RegisterMember(UArchSimMemberData* Comp)
{
    if (!Comp || !Comp->GetOwner())
    {
        return -1;
    }
    if (Comp->bRegistered)
    {
        // Idempotent re-call -- BeginPlay sometimes fires twice in PIE; trust the
        // first registration's MemberIdx.
        return Comp->MemberIdx;
    }

    EnsureDefaultLibraries();

    const FTransform& ActorT = Comp->GetOwner()->GetActorTransform();
    const FVector WorldIUE = ActorT.TransformPosition(Comp->EndIOffsetUE);
    const FVector WorldJUE = ActorT.TransformPosition(Comp->EndJOffsetUE);

    // UE cm -> FrameCore mm. The most common silent bug if forgotten -- the solve
    // returns dimensionally-wrong but numerically-plausible displacements.
    const FVector PosIMm = WorldIUE * kCmToMm;
    const FVector PosJMm = WorldJUE * kCmToMm;

    // Reject non-finite endpoints early. A NaN or Inf in the node position array
    // silently corrupts the stiffness matrix with no actionable FrameCore diagnostic.
    // Sources of non-finite values: NaN actor transform (freshly-spawned actor before
    // SetActorTransform fires, or a transform with degenerate scale), or
    // non-finite EndIOffsetUE/EndJOffsetUE fields (deserialized from a corrupt save).
    // WHY align with RegisterFixedSupport NaN check (ArchSimModelRegistry.cpp:L168-176):
    //   Consistent rejection style across both node-insertion paths.
    // WHY ContainsNaN() covers both NaN and Inf: UE::Math::TVector::ContainsNaN()
    // is implemented as !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)
    // (Vector.h:2298-2300). FMath::IsFinite returns false for both NaN and Inf.
    // There is no separate FVector::IsFinite() method in UE5.7 TVector<double>.
    if (PosIMm.ContainsNaN() || PosJMm.ContainsNaN())
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("RegisterMember: actor %s has non-finite endpoint positions "
                    "(PosIMm=(%.2f,%.2f,%.2f) PosJMm=(%.2f,%.2f,%.2f)); rejected."),
               *Comp->GetOwner()->GetName(),
               PosIMm.X, PosIMm.Y, PosIMm.Z,
               PosJMm.X, PosJMm.Y, PosJMm.Z);
        return -1;
    }

    // Reject degenerate (zero-length) members early; FrameCore would refuse them
    // later with a less actionable diagnostic.
    const FVector Axis = PosJMm - PosIMm;
    const double  Len  = Axis.Size();
    if (Len < kNodeMergeTolMm)
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("RegisterMember: actor %s has zero-length axis; rejected."),
               *Comp->GetOwner()->GetName());
        return -1;
    }
    const FVector AxisUnit = Axis / Len;

    const int32 NodeIIdx = FindOrAddNode(PosIMm);
    const int32 NodeJIdx = FindOrAddNode(PosJMm);

    const int32 NewMemberIdx = NextMemberIdx++;

    FFrameMember Member;
    // We make user Id == internal index so external code that holds either is
    // unambiguous. The FrameCore patch API consumes user Ids, not array indices.
    Member.Id     = NewMemberIdx;
    Member.I      = NodeIIdx;
    Member.J      = NodeJIdx;
    Member.MatIdx = Comp->MaterialId;
    Member.SecIdx = Comp->SectionId;
    Member.RefVec = PickRefVecForAxis(AxisUnit);
    Member.Release.Init(false, 12);   // length-12; engine ignores unless bEnableReleases
    Member.bActive      = true;
    Member.bTensionOnly = false;

    const int32 PushedIdx = CurrentModel.Members.Add(Member);
    check(PushedIdx == NewMemberIdx);   // index allocator and array stay in lock-step

    Comp->MemberIdx   = NewMemberIdx;
    Comp->bRegistered = true;
    IndexToComponent.Add(NewMemberIdx, Comp);

    // Topology changed; any existing session is stale. The next ExecuteSolve will
    // tear it down and restart on the new model.
    if (bSessionStarted)
    {
        if (UFrameInteractiveSubsystem* Sub = GetFrameSubsystem())
        {
            Sub->EndSession();
        }
        bSessionStarted = false;
    }
    bNeedsRebaseline = false;   // fresh-start path; no separate rebaseline needed

    // v0.2.0 hardening (S-02 review finding C-05): the queued PendingPatch
    // refers to ids from the just-ended session. Carrying them into the next
    // ExecuteSolve would feed stale Deactivate ids to a freshly-rebuilt model.
    // Drop the queue on session restart for the same reason RegisterMember
    // already drops bSessionStarted.
    PendingPatch            = FFrameModelPatch{};
    PendingRankAccumulation = 0;

    return NewMemberIdx;
}

bool UArchSimModelRegistry::FlushAndStartSession()
{
    UFrameInteractiveSubsystem* Sub = GetFrameSubsystem();
    if (!Sub)
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("FlushAndStartSession: UFrameInteractiveSubsystem not available."));
        return false;
    }
    if (bSessionStarted)
    {
        Sub->EndSession();
        bSessionStarted = false;
    }

    FFrameSolveOptions       Opts;
    FFrameReanalysisOptions  ReOpts;     // MaxRank=96 default matches our debounce cap
    FString                  Err;
    if (!Sub->StartSession(CurrentModel, Opts, ReOpts, Err))
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("FlushAndStartSession failed: %s"), *Err);
        return false;
    }

    bSessionStarted        = true;
    bNeedsRebaseline       = false;
    PendingRankAccumulation = 0;
    return true;
}

// ----- A1-04: debounced resolve ----------------------------------------------

namespace
{
    // Lower bound for "how much rank does this patch add" -- counts toggles
    // (Deactivate + Reactivate) on both members and shells. Underestimates the
    // FrameCore ladder cost but is a conservative trigger for Rebaseline.
    int32 PatchRank(const FFrameModelPatch& P)
    {
        return P.DeactivateMemberIds.Num() + P.ReactivateMemberIds.Num()
             + P.DeactivateShellIds.Num()  + P.ReactivateShellIds.Num();
    }

    // Merge `Src` into `Dst` for queued debounce. Append semantics for the toggle
    // lists; loads use Set-then-Add ordering at apply time so we just concatenate.
    void MergePatch(FFrameModelPatch& Dst, const FFrameModelPatch& Src)
    {
        Dst.DeactivateMemberIds.Append(Src.DeactivateMemberIds);
        Dst.ReactivateMemberIds.Append(Src.ReactivateMemberIds);
        Dst.DeactivateShellIds .Append(Src.DeactivateShellIds);
        Dst.ReactivateShellIds .Append(Src.ReactivateShellIds);
        // bResetLoads is sticky: once requested in a debounce window, keep it.
        Dst.bResetLoads = Dst.bResetLoads || Src.bResetLoads;
        Dst.SetNodalLoads.Append(Src.SetNodalLoads);
        Dst.AddNodalLoads.Append(Src.AddNodalLoads);
    }
}

void UArchSimModelRegistry::RequestSolve(const FFrameModelPatch& Patch)
{
    MergePatch(PendingPatch, Patch);
    PendingRankAccumulation += PatchRank(Patch);

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;
    UWorld* World = GI->GetWorld();
    if (!World) return;

    // Above the reanalysis ladder cap, do not wait for the debounce window --
    // burn the timer and solve immediately so the rank reset is bounded.
    if (PendingRankAccumulation > MaxRankBeforeRebaseline)
    {
        World->GetTimerManager().ClearTimer(DebounceTimer);
        bNeedsRebaseline = true;
        ExecuteSolve();
        return;
    }

    const float DelaySeconds = static_cast<float>(DebounceMs) / 1000.f;
    World->GetTimerManager().SetTimer(
        DebounceTimer, this, &UArchSimModelRegistry::ExecuteSolve,
        DelaySeconds, /*bLoop=*/false);
}

void UArchSimModelRegistry::ExecuteSolve()
{
    UFrameInteractiveSubsystem* Sub = GetFrameSubsystem();
    if (!Sub)
    {
        UE_LOG(LogArchSimRegistry, Warning,
               TEXT("ExecuteSolve: no FrameCore subsystem; drop patch."));
        PendingPatch            = FFrameModelPatch{};
        PendingRankAccumulation = 0;
        return;
    }

    if (!bSessionStarted)
    {
        if (!FlushAndStartSession())
        {
            // Session start failed (validation error / singular baseline).
            // Drop the queued patch -- a stale patch against a never-started
            // session would just compound the error.
            PendingPatch            = FFrameModelPatch{};
            PendingRankAccumulation = 0;
            return;
        }
    }

    if (bNeedsRebaseline)
    {
        Sub->Rebaseline();
        bNeedsRebaseline       = false;
        PendingRankAccumulation = 0;
    }

    FFrameSolveResult Result;
    const bool bOk = Sub->ApplyPatchAndResolve(PendingPatch, Result);

    PendingPatch            = FFrameModelPatch{};
    PendingRankAccumulation = 0;

    if (!bOk || Result.bSingular)
    {
        // Mechanism / singular path: skip Distribute so we never write NaNs into
        // the heatmap. HUD should surface Result.Diagnostic separately.
        UE_LOG(LogArchSimRegistry, Verbose,
               TEXT("ExecuteSolve singular: %s"), *Result.Diagnostic);
        return;
    }

    DistributeSolveResult(Result);
    OnSolveComplete.Broadcast(Result);
}

// ----- A1-05: result distribution -------------------------------------------

void UArchSimModelRegistry::DistributeSolveResult(const FFrameSolveResult& Result)
{
    if (Result.bSingular)
    {
        // Belt-and-braces: callers should already guard, but on singular results
        // MemberUtilization can be empty or hold sentinel values; never write through.
        return;
    }

    for (const FFrameMemberUtilization& U : Result.MemberUtilization)
    {
        if (U.MemberIdx < 0) continue;

        const TWeakObjectPtr<UArchSimMemberData>* Found =
            IndexToComponent.Find(U.MemberIdx);
        if (!Found) continue;

        UArchSimMemberData* Comp = Found->Get();
        if (!Comp) continue;     // weak-ptr stale -> component was destroyed; skip

        // Peak.Risk is the FrameCore D/C ratio for the worst end (see
        // FrameCoreUEResultTypes.h:126). Despite the calling convention name on the
        // plan ("DC"), the marshal struct uses "Risk".
        Comp->CachedUtilization = U.Peak.Risk;
    }
}

// ----- A1-06: soft delete (stub; full sweep + reactivate in a later task) ---

void UArchSimModelRegistry::DeactivateMember(int32 MemberIdx)
{
    if (!CurrentModel.Members.IsValidIndex(MemberIdx))
    {
        return;
    }

    CurrentModel.Members[MemberIdx].bActive = false;

    // FFrameModelPatch::DeactivateMemberIds takes the USER-ASSIGNED id (FFrameMember.Id),
    // not the internal Members[] array index. RegisterMember keeps Id == MemberIdx so
    // they coincide; written explicitly here to flag the API contract.
    const int32 UserId = CurrentModel.Members[MemberIdx].Id;

    // v0.2.0 hardening (S-02 review finding C-01): clear the owning component's
    // bRegistered / MemberIdx pair so a PIE restart can re-register fresh without
    // the BeginPlay idempotency guard at RegisterMember:139 short-circuiting on
    // a stale MemberIdx that points into a now-Deinitialized CurrentModel.
    if (TWeakObjectPtr<UArchSimMemberData>* WeakComp = IndexToComponent.Find(MemberIdx))
    {
        if (UArchSimMemberData* Comp = WeakComp->Get())
        {
            Comp->bRegistered = false;
            Comp->MemberIdx   = -1;
        }
    }

    FFrameModelPatch P;
    P.DeactivateMemberIds.Add(UserId);
    RequestSolve(P);
}
