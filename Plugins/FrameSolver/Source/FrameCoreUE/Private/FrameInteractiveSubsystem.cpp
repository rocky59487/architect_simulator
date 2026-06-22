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
    Cached.Reset(new FCachedModel);

    frame::SolveOptions Opts;
    FrameCoreUE::FromBlueprint(OptsBP, Opts);
    if (!FrameCoreUE::FromBlueprint(Def, Cached->Model, OutError))
    {
        Cached.Reset();
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
        Cached.Reset();
        OutError = TEXT("ReSolveSession ctor threw");
        return false;
    }
    if (!Session->valid())
    {
        OutError = FString(UTF8_TO_TCHAR(Session->diagnostic().c_str()));
        delete Session; Session = nullptr;
        Cached.Reset();
        return false;
    }
    return true;
}

void UFrameInteractiveSubsystem::EndSession()
{
    if (Session) { delete Session; Session = nullptr; }
    Cached.Reset();
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
