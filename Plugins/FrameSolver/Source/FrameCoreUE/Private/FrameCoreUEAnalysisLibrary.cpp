#include "FrameCoreUE/FrameCoreUEAnalysisLibrary.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"

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
#include "FrameCore/ModalAnalysis.h"
#include "FrameCore/ModalResult.h"
#include "FrameCore/BucklingAnalysis.h"
#include "FrameCore/BucklingResult.h"
#include "FrameCore/InfluenceLine.h"
#include "FrameCore/ResponseSpectrum.h"
#include "FrameCore/ModalDynamics.h"
#include "FrameCore/Combination.h"
#include "FrameCore/PDeltaAnalysis.h"
#include "FrameCore/TensionOnly.h"
#include "FrameCore/SizeOpt.h"
#include "FrameCore/Topology.h"
#include "FrameCore/CorotationalAnalysis.h"
#include "FrameCore/DynamicCollapse.h"
#include "FrameCore/Reanalysis.h"

DEFINE_LOG_CATEGORY_STATIC(LogFrameCoreUEAnalysis, Log, All);

namespace FrameCoreUE
{
    // Phase 1 (FrameCoreUEModelMarshal.cpp)
    FRAMECOREUE_API bool FromBlueprint(const FFrameModelDef& Def, frame::FrameModel& Out, FString& Err);
    FRAMECOREUE_API bool FromBlueprint(const FFrameSolveOptions& Opts, frame::SolveOptions& Out);

    // Phase 2 (FrameCoreUEResultMarshal.cpp)
    FRAMECOREUE_API FFrameSolveResult ToBlueprint(const frame::FrameModel& M, const frame::SolveResult& R);

    // Phase 3+4 (FrameCoreUEAnalysisMarshal.cpp)
    FRAMECOREUE_API FFrameModalResult            ToBlueprint(const frame::FrameModel& M, const frame::ModalResult& R);
    FRAMECOREUE_API FFrameBucklingResult         ToBlueprint(const frame::FrameModel& M, const frame::BucklingResult& R);
    FRAMECOREUE_API FFrameResponseSpectrumResult ToBlueprint(const frame::FrameModel& M, const frame::ResponseSpectrumResult& R);
    FRAMECOREUE_API FFrameModalTimeHistory       ToBlueprint(const frame::FrameModel& M, const frame::ModalTimeHistory& H);
    FRAMECOREUE_API FFrameLoadEnvelope           ToBlueprint(const frame::FrameModel& M, const frame::ResultEnvelope& E);
    FRAMECOREUE_API FFramePDeltaResult           ToBlueprint(const frame::FrameModel& M, const frame::PDeltaResult& R);
    FRAMECOREUE_API FFrameTensionOnlyResult      ToBlueprint(const frame::FrameModel& M, const frame::TensionOnlyResult& R);
    FRAMECOREUE_API FFrameSizeOptResult          ToBlueprint(const frame::FrameModel& M, const frame::SizeOptResult& R);
    FRAMECOREUE_API FFrameBESOResult             ToBlueprint(const frame::BESOResult& R);
    FRAMECOREUE_API FFrameCorotationalResult     ToBlueprint(const frame::FrameModel& M, const frame::CorotationalResult& R);
    FRAMECOREUE_API FFrameDynCollapseResult      ToBlueprint(const frame::FrameModel& M, const frame::DynCollapseHistory& H);
}

namespace
{
    // Build engine model + options + log on FromBlueprint failure. Returns false to abort
    // the caller with a stubbed singular result.
    bool BuildModelAndOpts(const FFrameModelDef& Def, const FFrameSolveOptions& OptsBP,
                           frame::FrameModel& Model, frame::SolveOptions& Opts, FString& Err)
    {
        FrameCoreUE::FromBlueprint(OptsBP, Opts);
        return FrameCoreUE::FromBlueprint(Def, Model, Err);
    }

    FFrameSolveResult MakeSingularSolveResult(const FString& Diagnostic)
    {
        FFrameSolveResult out;
        out.bSingular  = true;
        out.Diagnostic = Diagnostic;
        return out;
    }
}

// ---------------------------------------------------------------------------
// Phase 3 -- Linear
// ---------------------------------------------------------------------------

FFrameSolveResult UFrameAnalysisLibrary::SolveLinear(const FFrameModelDef& Def,
                                                    const FFrameSolveOptions& OptsBP)
{
    frame::FrameModel M;
    frame::SolveOptions Opts;
    FString Err;
    if (!BuildModelAndOpts(Def, OptsBP, M, Opts, Err))
    {
        return MakeSingularSolveResult(FString::Printf(TEXT("model marshal failed: %s"), *Err));
    }
    const frame::SolveResult R = frame::solve(M, Opts);
    return FrameCoreUE::ToBlueprint(M, R);
}

FFrameModalResult UFrameAnalysisLibrary::AnalysisModal(const FFrameModelDef& Def,
                                                       const FFrameSolveOptions& OptsBP,
                                                       const FFrameModalOptions& MOpts)
{
    frame::FrameModel M;
    frame::SolveOptions Opts;
    FString Err;
    if (!BuildModelAndOpts(Def, OptsBP, M, Opts, Err))
    {
        FFrameModalResult out;
        out.bSingular  = true;
        out.Diagnostic = FString::Printf(TEXT("model marshal failed: %s"), *Err);
        return out;
    }
    const frame::PreparedSystem prepared = frame::assembleAndFactor(M, Opts);
    frame::ModalOptions eopts;
    eopts.numModes        = MOpts.NumModes;
    eopts.useSparseSolver = MOpts.bUseSparseSolver;
    const frame::ModalResult R = frame::solveModal(prepared, eopts);
    return FrameCoreUE::ToBlueprint(M, R);
}

FFrameBucklingResult UFrameAnalysisLibrary::AnalysisBuckling(const FFrameModelDef& Def,
                                                             const FFrameSolveOptions& OptsBP,
                                                             const FFrameBucklingOptions& BOpts)
{
    frame::FrameModel M;
    frame::SolveOptions Opts;
    FString Err;
    if (!BuildModelAndOpts(Def, OptsBP, M, Opts, Err))
    {
        FFrameBucklingResult out;
        out.bSingular  = true;
        out.Diagnostic = FString::Printf(TEXT("model marshal failed: %s"), *Err);
        return out;
    }
    const frame::PreparedSystem prepared = frame::assembleAndFactor(M, Opts);
    frame::BucklingOptions eopts;
    eopts.denseThreshold         = BOpts.DenseThreshold;
    eopts.nev                    = BOpts.Nev;
    eopts.maxIter                = BOpts.MaxIter;
    eopts.tol                    = BOpts.Tol;
    eopts.shellBucklingKnockdown = BOpts.ShellBucklingKnockdown;
    const frame::BucklingResult R = frame::solveBuckling(prepared, M, eopts);
    return FrameCoreUE::ToBlueprint(M, R);
}

FFrameLoadEnvelope UFrameAnalysisLibrary::LoadCombineEnvelope(const FFrameModelDef& BaseDef,
                                                              const FFrameSolveOptions& OptsBP,
                                                              const TArray<FFrameSizeOptLoadCase>& Cases)
{
    frame::FrameModel BaseM;
    frame::SolveOptions Opts;
    FString Err;
    if (!BuildModelAndOpts(BaseDef, OptsBP, BaseM, Opts, Err))
    {
        FFrameLoadEnvelope out;
        out.bSingular  = true;
        out.Diagnostic = FString::Printf(TEXT("model marshal failed: %s"), *Err);
        return out;
    }
    if (Cases.Num() == 0)
    {
        FFrameLoadEnvelope out;
        out.bSingular  = true;
        out.Diagnostic = TEXT("LoadCombineEnvelope: Cases is empty");
        return out;
    }

    // Solve each case with the BASE model's structural data + case loads override. Mirrors
    // SizeOpt's per-case behaviour (frame::SolveOptions threaded; supports / sections /
    // members fixed across cases).
    std::vector<frame::SolveResult> results;
    results.reserve(Cases.Num());
    for (int32 c = 0; c < Cases.Num(); ++c)
    {
        frame::FrameModel M = BaseM;
        M.nodalLoads.clear();
        M.memberUDLs.clear();
        for (const FFrameNodalLoad& L : Cases[c].NodalLoads)
        {
            if (L.Comp.Num() != 6) continue;
            frame::NodalLoad nl;
            nl.node = (frame::NodeId)L.Node;
            for (int32 d = 0; d < 6; ++d) nl.comp[d] = (frame::real)L.Comp[d];
            M.nodalLoads.push_back(nl);
        }
        for (const FFrameMemberUDL& U : Cases[c].MemberUDLs)
        {
            frame::MemberUDL u;
            u.member  = (frame::MemberId)U.Member;
            u.w_local = frame::Vec3((frame::real)U.WLocal.X,
                                    (frame::real)U.WLocal.Y,
                                    (frame::real)U.WLocal.Z);
            M.memberUDLs.push_back(u);
        }
        results.push_back(frame::solve(M, Opts));
    }

    const frame::ResultEnvelope env = frame::envelope(results);
    return FrameCoreUE::ToBlueprint(BaseM, env);
}

FFrameInfluenceLine UFrameAnalysisLibrary::InfluenceLine(const FFrameModelDef& Def,
                                                         const FFrameSolveOptions& OptsBP,
                                                         const TArray<int32>& LoadNodes,
                                                         int32 ReactNode,
                                                         int32 ReactDof)
{
    FFrameInfluenceLine out;
    out.ReactNode = ReactNode;
    out.ReactDof  = ReactDof;
    out.LoadNodes = LoadNodes;

    frame::FrameModel M;
    frame::SolveOptions Opts;
    FString Err;
    if (!BuildModelAndOpts(Def, OptsBP, M, Opts, Err)) return out;

    const frame::PreparedSystem prepared = frame::assembleAndFactor(M, Opts);

    std::vector<frame::NodeId> loadIds;
    loadIds.reserve(LoadNodes.Num());
    for (int32 n : LoadNodes) loadIds.push_back((frame::NodeId)n);

    const std::vector<frame::real> line = frame::reactionInfluenceLine(
        prepared, M, loadIds, (frame::NodeId)ReactNode, ReactDof);

    out.ReactionAtPosition.Reserve((int32)line.size());
    for (frame::real v : line) out.ReactionAtPosition.Add((float)v);
    return out;
}

FFrameResponseSpectrumResult UFrameAnalysisLibrary::ResponseSpectrum(const FFrameModelDef& Def,
                                                                     const FFrameSolveOptions& OptsBP,
                                                                     const FFrameModalOptions& MOpts,
                                                                     const FFrameSpectrum& Spectrum,
                                                                     int32 ExcDof,
                                                                     EFrameSpectrumCombo Combo,
                                                                     float Zeta)
{
    frame::FrameModel M;
    frame::SolveOptions Opts;
    FString Err;
    if (!BuildModelAndOpts(Def, OptsBP, M, Opts, Err))
    {
        FFrameResponseSpectrumResult out;
        out.bSingular  = true;
        out.Diagnostic = FString::Printf(TEXT("model marshal failed: %s"), *Err);
        return out;
    }

    const frame::PreparedSystem prepared = frame::assembleAndFactor(M, Opts);
    frame::ModalOptions mopts;
    mopts.numModes        = MOpts.NumModes;
    mopts.useSparseSolver = MOpts.bUseSparseSolver;
    const frame::ModalResult modes = frame::solveModal(prepared, mopts);

    frame::Spectrum spec;
    spec.T.reserve(Spectrum.T.Num());
    spec.Sa.reserve(Spectrum.Sa.Num());
    for (float v : Spectrum.T)  spec.T.push_back((frame::real)v);
    for (float v : Spectrum.Sa) spec.Sa.push_back((frame::real)v);
    const frame::SpectrumCombo c = (Combo == EFrameSpectrumCombo::CQC)
                                    ? frame::SpectrumCombo::CQC
                                    : frame::SpectrumCombo::SRSS;
    const frame::ResponseSpectrumResult R =
        frame::solveResponseSpectrum(prepared, modes, spec, ExcDof, c, (frame::real)Zeta);
    return FrameCoreUE::ToBlueprint(M, R);
}

FFrameModalTimeHistory UFrameAnalysisLibrary::RealTimeDynamic(const FFrameModelDef& Def,
                                                              const FFrameSolveOptions& OptsBP,
                                                              const FFrameModalOptions& MOpts,
                                                              const FFrameModalDynamicsOptions& DynOpts)
{
    frame::FrameModel M;
    frame::SolveOptions Opts;
    FString Err;
    if (!BuildModelAndOpts(Def, OptsBP, M, Opts, Err))
    {
        FFrameModalTimeHistory out;
        out.bSingular  = true;
        out.Diagnostic = FString::Printf(TEXT("model marshal failed: %s"), *Err);
        return out;
    }
    const frame::PreparedSystem prepared = frame::assembleAndFactor(M, Opts);
    frame::ModalOptions mopts;
    mopts.numModes        = MOpts.NumModes;
    mopts.useSparseSolver = MOpts.bUseSparseSolver;
    const frame::ModalResult modes = frame::solveModal(prepared, mopts);

    frame::ModalDynamicsOptions dopts;
    dopts.dt     = (frame::real)DynOpts.Dt;
    dopts.nSteps = DynOpts.NSteps;
    dopts.zeta   = (frame::real)DynOpts.Zeta;
    const frame::ModalTimeHistory H = frame::solveModalStepResponse(prepared, M, modes, dopts);
    return FrameCoreUE::ToBlueprint(M, H);
}

FFrameSolveResult UFrameAnalysisLibrary::ReanalysisSolve(const FFrameModelDef& Def,
                                                        const FFrameSolveOptions& OptsBP,
                                                        const FFrameReanalysisOptions& ReOpts,
                                                        const TArray<int32>& DeactivateMemberIds,
                                                        const TArray<int32>& DeactivateShellIds)
{
    frame::FrameModel M;
    frame::SolveOptions Opts;
    FString Err;
    if (!BuildModelAndOpts(Def, OptsBP, M, Opts, Err))
    {
        return MakeSingularSolveResult(FString::Printf(TEXT("model marshal failed: %s"), *Err));
    }
    frame::ReanalysisOptions eopts;
    eopts.maxRank      = ReOpts.MaxRank;
    eopts.pcgTol       = (frame::real)ReOpts.PcgTol;
    eopts.pcgMaxIter   = ReOpts.PcgMaxIter;
    eopts.allowTier2   = ReOpts.bAllowTier2;
    eopts.mechPivotTol = (frame::real)ReOpts.MechPivotTol;
    eopts.solve        = Opts;

    frame::ReSolveSession session(M, eopts);
    if (!session.valid())
    {
        return MakeSingularSolveResult(FString(UTF8_TO_TCHAR(session.diagnostic().c_str())));
    }
    for (int32 id : DeactivateMemberIds) session.setMemberActive((frame::MemberId)id, false);
    for (int32 id : DeactivateShellIds)  session.setShellActive(id, false);
    const frame::SolveResult R = session.solve(/*stats=*/nullptr);
    return FrameCoreUE::ToBlueprint(M, R);
}

// ---------------------------------------------------------------------------
// Phase 4 -- Nonlinear
// ---------------------------------------------------------------------------

FFramePDeltaResult UFrameAnalysisLibrary::SolvePDelta(const FFrameModelDef& Def,
                                                     const FFramePDeltaOptions& PDOpts)
{
    frame::FrameModel M;
    frame::SolveOptions OptsBase;
    FString Err;
    if (!BuildModelAndOpts(Def, PDOpts.Solve, M, OptsBase, Err))
    {
        FFramePDeltaResult out;
        out.FinalState.bSingular  = true;
        out.FinalState.Diagnostic = FString::Printf(TEXT("model marshal failed: %s"), *Err);
        return out;
    }
    frame::PDeltaOptions eopts;
    eopts.maxIter      = PDOpts.MaxIter;
    eopts.tolU         = (frame::real)PDOpts.TolU;
    eopts.accelerate   = PDOpts.bAccelerate;
    eopts.refactorPath = PDOpts.bRefactorPath;
    eopts.solve        = OptsBase;
    const frame::PDeltaResult R = frame::runPDelta(M, eopts);
    return FrameCoreUE::ToBlueprint(M, R);
}

FFrameTensionOnlyResult UFrameAnalysisLibrary::SolveTensionOnly(const FFrameModelDef& Def,
                                                                const FFrameTensionOnlyOptions& TOpts)
{
    frame::FrameModel M;
    frame::SolveOptions OptsBase;
    FString Err;
    if (!BuildModelAndOpts(Def, TOpts.Solve, M, OptsBase, Err))
    {
        FFrameTensionOnlyResult out;
        out.FinalState.bSingular  = true;
        out.FinalState.Diagnostic = FString::Printf(TEXT("model marshal failed: %s"), *Err);
        return out;
    }
    frame::TensionOnlyOptions eopts;
    eopts.maxIter           = TOpts.MaxIter;
    eopts.allowReactivation = TOpts.bAllowReactivation;
    eopts.axialTol          = (frame::real)TOpts.AxialTol;
    eopts.solve             = OptsBase;
    const frame::TensionOnlyResult R = frame::runTensionOnly(M, eopts);
    return FrameCoreUE::ToBlueprint(M, R);
}

FFrameSizeOptResult UFrameAnalysisLibrary::SolveSizeOpt(const FFrameModelDef& Def,
                                                       const FFrameSizeOptOptions& SOpts,
                                                       const TArray<int32>& SizableMembers)
{
    frame::FrameModel M;
    frame::SolveOptions OptsBase;
    FString Err;
    if (!BuildModelAndOpts(Def, SOpts.Solve, M, OptsBase, Err))
    {
        FFrameSizeOptResult out;
        out.bSingular = true;
        return out;
    }
    frame::SizeOptOptions eopts;
    eopts.maxIter = SOpts.MaxIter;
    eopts.dcTol   = (frame::real)SOpts.DCTol;
    eopts.Amin    = (frame::real)SOpts.Amin;
    eopts.solve   = OptsBase;
    eopts.sectionTable.reserve(SOpts.SectionTable.Num());
    for (float v : SOpts.SectionTable) eopts.sectionTable.push_back((frame::real)v);
    // Multi-case load sets.
    eopts.cases.reserve(SOpts.Cases.Num());
    for (const FFrameSizeOptLoadCase& C : SOpts.Cases)
    {
        frame::SizeOptLoadCase lc;
        for (const FFrameNodalLoad& L : C.NodalLoads)
        {
            if (L.Comp.Num() != 6) continue;
            frame::NodalLoad nl;
            nl.node = (frame::NodeId)L.Node;
            for (int32 d = 0; d < 6; ++d) nl.comp[d] = (frame::real)L.Comp[d];
            lc.nodalLoads.push_back(nl);
        }
        for (const FFrameMemberUDL& U : C.MemberUDLs)
        {
            frame::MemberUDL u;
            u.member  = (frame::MemberId)U.Member;
            u.w_local = frame::Vec3((frame::real)U.WLocal.X,
                                    (frame::real)U.WLocal.Y,
                                    (frame::real)U.WLocal.Z);
            lc.memberUDLs.push_back(u);
        }
        eopts.cases.push_back(lc);
    }
    std::vector<int> sizable;
    sizable.reserve(SizableMembers.Num());
    for (int32 v : SizableMembers) sizable.push_back(v);
    const frame::SizeOptResult R = frame::runSizeOptimization(M, eopts, sizable);
    return FrameCoreUE::ToBlueprint(M, R);
}

FFrameBESOResult UFrameAnalysisLibrary::SolveBESO(const FFrameModelDef& Def,
                                                  const FFrameBESOOptions& BOpts,
                                                  const TArray<int32>& DesignMembers)
{
    frame::FrameModel M;
    frame::SolveOptions OptsBase;
    FString Err;
    if (!BuildModelAndOpts(Def, BOpts.Solve, M, OptsBase, Err))
    {
        FFrameBESOResult out;
        out.Reason = EFrameBESOStop::Invalid;
        return out;
    }
    frame::BESOOptions eopts;
    eopts.targetVolFrac           = (frame::real)BOpts.TargetVolFrac;
    eopts.evolRate                = (frame::real)BOpts.EvolRate;
    eopts.maxIter                 = BOpts.MaxIter;
    eopts.sensHistory             = BOpts.bSensHistory;
    eopts.complianceJumpTol       = (frame::real)BOpts.ComplianceJumpTol;
    eopts.complianceBestRollback  = BOpts.bComplianceBestRollback;
    eopts.wAxial                  = (frame::real)BOpts.WAxial;
    eopts.wBending                = (frame::real)BOpts.WBending;
    eopts.wShear                  = (frame::real)BOpts.WShear;
    eopts.wTorsion                = (frame::real)BOpts.WTorsion;
    eopts.redundancyCheckEvery    = BOpts.RedundancyCheckEvery;
    eopts.redundancySamples       = BOpts.RedundancySamples;
    // Redundancy CollapseOptions: map BOpts.Redundancy fields.
    eopts.redundancy.dlf             = (frame::real)BOpts.Redundancy.DLF;
    eopts.redundancy.removeThreshold = (frame::real)BOpts.Redundancy.RemoveThreshold;
    eopts.redundancy.maxSteps        = BOpts.Redundancy.MaxSteps;
    eopts.redundancy.plasticHinges   = BOpts.Redundancy.bPlasticHinges;
    eopts.redundancy.nmInteraction   = BOpts.Redundancy.bNMInteraction;
    for (int32 id : BOpts.Redundancy.InitialRemovals)      eopts.redundancy.initialRemovals.push_back((frame::MemberId)id);
    for (int32 id : BOpts.Redundancy.InitialShellRemovals) eopts.redundancy.initialShellRemovals.push_back(id);
    eopts.redundancy.solve = OptsBase;
    eopts.solve                   = OptsBase;
    std::vector<int> design;
    design.reserve(DesignMembers.Num());
    for (int32 v : DesignMembers) design.push_back(v);
    const frame::BESOResult R = frame::runBESO(M, eopts, design);
    return FrameCoreUE::ToBlueprint(R);
}

FFrameCorotationalResult UFrameAnalysisLibrary::SolveCorotational(const FFrameModelDef& Def,
                                                                  const FFrameCorotationalOptions& COpts)
{
    frame::FrameModel M;
    frame::SolveOptions OptsBase;
    FString Err;
    if (!BuildModelAndOpts(Def, COpts.Solve, M, OptsBase, Err))
    {
        FFrameCorotationalResult out;
        out.FinalState.bSingular  = true;
        out.FinalState.Diagnostic = FString::Printf(TEXT("model marshal failed: %s"), *Err);
        return out;
    }
    frame::CorotationalOptions eopts;
    eopts.loadSteps         = COpts.LoadSteps;
    eopts.maxIter           = COpts.MaxIter;
    eopts.tolR              = (frame::real)COpts.TolR;
    eopts.tolU              = (frame::real)COpts.TolU;
    eopts.useArcLength      = COpts.bUseArcLength;
    eopts.arcLength         = (frame::real)COpts.ArcLength;
    eopts.arcSteps          = COpts.ArcSteps;
    eopts.monitorDof        = COpts.MonitorDof;
    eopts.consistentTangent = COpts.bConsistentTangent;
    eopts.shellCorotational = COpts.bShellCorotational;
    eopts.solve             = OptsBase;
    const frame::CorotationalResult R = frame::runCorotational(M, eopts);
    return FrameCoreUE::ToBlueprint(M, R);
}

FFrameCorotationalResult UFrameAnalysisLibrary::SolveArcLength(const FFrameModelDef& Def,
                                                               const FFrameCorotationalOptions& COpts)
{
    FFrameCorotationalOptions Forced = COpts;
    Forced.bUseArcLength = true;
    return SolveCorotational(Def, Forced);
}

FFrameDynCollapseResult UFrameAnalysisLibrary::SolveDynCollapse(const FFrameModelDef& Def,
                                                                const FFrameDynCollapseOptions& DOpts)
{
    frame::FrameModel M;
    frame::SolveOptions OptsBase;
    FString Err;
    if (!BuildModelAndOpts(Def, DOpts.Solve, M, OptsBase, Err))
    {
        FFrameDynCollapseResult out;
        out.Outcome    = EFrameDynCollapseOutcome::Invalid;
        out.Diagnostic = FString::Printf(TEXT("model marshal failed: %s"), *Err);
        return out;
    }
    frame::DynCollapseOptions eopts;
    eopts.dt                = (frame::real)DOpts.Dt;
    eopts.maxTime           = (frame::real)DOpts.MaxTime;
    eopts.basisSize         = DOpts.BasisSize;
    eopts.useRitzVectors    = DOpts.bUseRitzVectors;
    eopts.rayleighAlpha     = (frame::real)DOpts.RayleighAlpha;
    eopts.rayleighBeta      = (frame::real)DOpts.RayleighBeta;
    eopts.removeThreshold   = (frame::real)DOpts.RemoveThreshold;
    eopts.screenEvery       = DOpts.ScreenEvery;
    eopts.quietKineticRatio = (frame::real)DOpts.QuietKineticRatio;
    eopts.maxEvents         = DOpts.MaxEvents;
    eopts.frameStride       = DOpts.FrameStride;
    eopts.solve             = OptsBase;
    for (int32 id : DOpts.InitialRemovals)      eopts.initialRemovals.push_back((frame::MemberId)id);
    for (int32 id : DOpts.InitialShellRemovals) eopts.initialShellRemovals.push_back(id);
    const frame::DynCollapseHistory H = frame::runDynamicCollapse(M, eopts);
    return FrameCoreUE::ToBlueprint(M, H);
}
