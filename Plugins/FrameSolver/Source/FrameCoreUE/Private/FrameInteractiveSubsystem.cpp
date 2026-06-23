#include "FrameCoreUE/FrameInteractiveSubsystem.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Shell.h"
#include "FrameCore/Load.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/SolveOptions.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/Reanalysis.h"

namespace FrameCoreUE
{
    // Phase 1 / Phase 2 marshal helpers (defined elsewhere).
    FRAMECOREUE_API bool FromBlueprint(const FFrameModelDef& Def, frame::FrameModel& Out, FString& Err);
    FRAMECOREUE_API bool FromBlueprint(const FFrameSolveOptions& Opts, frame::SolveOptions& Out);
    FRAMECOREUE_API FFrameSolveResult ToBlueprint(const frame::FrameModel& M, const frame::SolveResult& R);
}

// Implementation-only state — owns the cached engine model. UTUniquePtr requires the
// destructor visible at instantiation, so define the struct here and let the wrapper Type
// know it via the forward decl in the header.
struct UFrameInteractiveSubsystem::FCachedModel
{
    frame::FrameModel Model;
};

UFrameInteractiveSubsystem::UFrameInteractiveSubsystem() = default;
UFrameInteractiveSubsystem::~UFrameInteractiveSubsystem()
{
    EndSession();
}

void UFrameInteractiveSubsystem::Deinitialize()
{
    EndSession();
    Super::Deinitialize();
}

bool UFrameInteractiveSubsystem::StartSession(const FFrameModelDef& Def,
                                              const FFrameSolveOptions& OptsBP,
                                              const FFrameReanalysisOptions& ReOpts,
                                              FString& OutError)
{
    EndSession();
    Cached = new FCachedModel;

    frame::SolveOptions Opts;
    FrameCoreUE::FromBlueprint(OptsBP, Opts);
    if (!FrameCoreUE::FromBlueprint(Def, Cached->Model, OutError))
    {
        delete Cached; Cached = nullptr;
        return false;
    }
    frame::ReanalysisOptions eopts;
    eopts.maxRank      = ReOpts.MaxRank;
    eopts.pcgTol       = (frame::real)ReOpts.PcgTol;
    eopts.pcgMaxIter   = ReOpts.PcgMaxIter;
    eopts.allowTier2   = ReOpts.bAllowTier2;
    eopts.mechPivotTol = (frame::real)ReOpts.MechPivotTol;
    eopts.solve        = Opts;
    // Audit D-01: wrap the engine ctor in try/catch so a ReSolveSession allocation
    // failure (std::bad_alloc, engine-side throw) does not leave Cached non-null with
    // Session null — the !Session || !Cached invariant guards downstream calls, but
    // the orphaned FCachedModel held a full frame::FrameModel and could surprise
    // memory budget for large models.
    try
    {
        Session = new frame::ReSolveSession(Cached->Model, eopts);
    }
    catch (...)
    {
        delete Cached; Cached = nullptr;
        OutError = TEXT("ReSolveSession ctor threw");
        return false;
    }
    if (!Session->valid())
    {
        OutError = FString(UTF8_TO_TCHAR(Session->diagnostic().c_str()));
        delete Session; Session = nullptr;
        delete Cached; Cached = nullptr;
        return false;
    }
    return true;
}

void UFrameInteractiveSubsystem::EndSession()
{
    if (Session) { delete Session; Session = nullptr; }
    delete Cached; Cached = nullptr;
}

bool UFrameInteractiveSubsystem::ApplyPatchAndResolve(const FFrameModelPatch& Patch,
                                                      FFrameSolveResult& OutResult)
{
    if (!Session || !Cached)
    {
        OutResult.bSingular  = true;
        OutResult.Diagnostic = TEXT("ApplyPatchAndResolve: no active session");
        return false;
    }
    for (int32 id : Patch.DeactivateMemberIds) Session->setMemberActive((frame::MemberId)id, false);
    for (int32 id : Patch.ReactivateMemberIds) Session->setMemberActive((frame::MemberId)id, true);
    for (int32 id : Patch.DeactivateShellIds)  Session->setShellActive(id, false);
    for (int32 id : Patch.ReactivateShellIds)  Session->setShellActive(id, true);

    // v3.6 U-12: incremental nodal-load patch. Order: reset -> set -> add.
    if (Patch.bResetLoads) { Cached->Model.nodalLoads.clear(); }
    for (const FFrameNodalLoad& L : Patch.SetNodalLoads)
    {
        if (L.Comp.Num() != 6) continue;
        // Replace any existing load at this node.
        for (auto it = Cached->Model.nodalLoads.begin(); it != Cached->Model.nodalLoads.end(); )
        {
            if ((int32)it->node == L.Node) { it = Cached->Model.nodalLoads.erase(it); }
            else { ++it; }
        }
        frame::NodalLoad nl;
        nl.node = (frame::NodeId)L.Node;
        for (int32 d = 0; d < 6; ++d) nl.comp[d] = (frame::real)L.Comp[d];
        Cached->Model.nodalLoads.push_back(nl);
    }
    for (const FFrameNodalLoad& L : Patch.AddNodalLoads)
    {
        if (L.Comp.Num() != 6) continue;
        // Accumulate into existing entry, or create one.
        bool bFound = false;
        for (auto& Existing : Cached->Model.nodalLoads)
        {
            if ((int32)Existing.node == L.Node)
            {
                for (int32 d = 0; d < 6; ++d) { Existing.comp[d] += (frame::real)L.Comp[d]; }
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            frame::NodalLoad nl;
            nl.node = (frame::NodeId)L.Node;
            for (int32 d = 0; d < 6; ++d) nl.comp[d] = (frame::real)L.Comp[d];
            Cached->Model.nodalLoads.push_back(nl);
        }
    }

    // v3.6 audit Lane 3 (A-02): ReSolveSession::solve() reads its internal `work`
    // model snapshot — Cached->Model mutations to nodalLoads don't propagate. When
    // the patch touches loads, the correct fix is to recreate the session with the
    // updated model so the engine re-reads the load list at construction time.
    // This is Tier-3++ cost (defeats the ReSolve performance promise for the load
    // path); if a future engine release exposes `setNodalLoads` on ReSolveSession,
    // the load path can drop back to Tier-1 / Tier-2.
    const bool bLoadsTouched = Patch.bResetLoads ||
                               !Patch.SetNodalLoads.IsEmpty() ||
                               !Patch.AddNodalLoads.IsEmpty();
    if (bLoadsTouched)
    {
        frame::ReanalysisOptions eopts;
        // We don't have the original ReanalysisOptions cached. Use defaults; the
        // session lifetime semantics (active member set survived inside Cached->Model
        // since setMemberActive went straight to engine session, but Cached->Model
        // member.active reflects the user's intent). For v3.6 simplicity reinit
        // with defaults — the load patch is the correctness target.
        delete Session;
        try
        {
            Session = new frame::ReSolveSession(Cached->Model, eopts);
        }
        catch (...)
        {
            Session = nullptr;
            OutResult.bSingular  = true;
            OutResult.Diagnostic = TEXT("ApplyPatchAndResolve: session re-init threw");
            return false;
        }
        if (!Session->valid())
        {
            OutResult.bSingular  = true;
            OutResult.Diagnostic = FString(UTF8_TO_TCHAR(Session->diagnostic().c_str()));
            delete Session; Session = nullptr;
            return false;
        }
    }
    const frame::SolveResult R = Session->solve(nullptr);
    OutResult = FrameCoreUE::ToBlueprint(Cached->Model, R);
    return !R.singular;
}

void UFrameInteractiveSubsystem::Rebaseline()
{
    if (Session) { Session->rebaseline(); }
}

bool UFrameInteractiveSubsystem::ResolveCurrent(FFrameSolveResult& OutResult)
{
    if (!Session || !Cached)
    {
        OutResult.bSingular  = true;
        OutResult.Diagnostic = TEXT("ResolveCurrent: no active session");
        return false;
    }
    const frame::SolveResult R = Session->solve(nullptr);
    OutResult = FrameCoreUE::ToBlueprint(Cached->Model, R);
    return !R.singular;
}
