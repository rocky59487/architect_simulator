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
