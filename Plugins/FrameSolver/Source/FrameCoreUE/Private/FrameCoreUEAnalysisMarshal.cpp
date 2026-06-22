// v3.4 Phase 3 + Phase 4 -- marshal layer for every analysis result struct.

#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Shell.h"
#include "FrameCore/Load.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/ModalResult.h"
#include "FrameCore/BucklingResult.h"
#include "FrameCore/ResponseSpectrum.h"
#include "FrameCore/ModalDynamics.h"
#include "FrameCore/Combination.h"
#include "FrameCore/PDeltaAnalysis.h"
#include "FrameCore/TensionOnly.h"
#include "FrameCore/SizeOpt.h"
#include "FrameCore/Topology.h"
#include "FrameCore/CorotationalAnalysis.h"
#include "FrameCore/DynamicCollapse.h"
#include "FrameCore/ISectionStrength.h"
#include "FrameCore/Connectivity.h"

#include <cmath>

namespace
{
    EFrameFailMode MapFailMode(frame::FailMode M)
    {
        switch (M)
        {
            case frame::FailMode::Crush:         return EFrameFailMode::Crush;
            case frame::FailMode::Tension:       return EFrameFailMode::Tension;
            case frame::FailMode::Shear:         return EFrameFailMode::Shear;
            case frame::FailMode::Bending:       return EFrameFailMode::Bending;
            case frame::FailMode::Torsion:       return EFrameFailMode::Torsion;
            case frame::FailMode::ShellVonMises: return EFrameFailMode::ShellVonMises;
            default:                             return EFrameFailMode::None;
        }
    }

    EFrameDynCollapseOutcome MapDynOutcome(frame::CollapseOutcome O)
    {
        switch (O)
        {
            case frame::CollapseOutcome::Stable:    return EFrameDynCollapseOutcome::Stable;
            case frame::CollapseOutcome::Collapsed: return EFrameDynCollapseOutcome::Collapsed;
            case frame::CollapseOutcome::MaxSteps:  return EFrameDynCollapseOutcome::MaxSteps;
            case frame::CollapseOutcome::Invalid:
            default:                                return EFrameDynCollapseOutcome::Invalid;
        }
    }

    EFrameBESOStop MapBESOStop(frame::BESOStop S)
    {
        switch (S)
        {
            case frame::BESOStop::TargetReached:  return EFrameBESOStop::TargetReached;
            case frame::BESOStop::ComplianceJump: return EFrameBESOStop::ComplianceJump;
            case frame::BESOStop::Stalled:        return EFrameBESOStop::Stalled;
            case frame::BESOStop::Mechanism:      return EFrameBESOStop::Mechanism;
            case frame::BESOStop::MaxIter:        return EFrameBESOStop::MaxIter;
            case frame::BESOStop::Invalid:
            default:                              return EFrameBESOStop::Invalid;
        }
    }

    FFrameMemberEndForces MarshalEndForces(const frame::MemberEndForces& f)
    {
        FFrameMemberEndForces out;
        out.N  = (float)f.N;
        out.Vy = (float)f.Vy;
        out.Vz = (float)f.Vz;
        out.T  = (float)f.T;
        out.My = (float)f.My;
        out.Mz = (float)f.Mz;
        return out;
    }

    // 6N flat -> TArray<FFrameNodalDisplacement>. NodeIndex/NodeId carried from model.
    TArray<FFrameNodalDisplacement> Marshal6NToDisplacements(
        const frame::FrameModel& M, const std::vector<frame::real>& u)
    {
        TArray<FFrameNodalDisplacement> out;
        const int32 N = (int32)M.nodes.size();
        if ((int32)u.size() < N * 6) return out;
        out.Reserve(N);
        for (int32 k = 0; k < N; ++k)
        {
            FFrameNodalDisplacement d;
            d.NodeIndex = k;
            d.NodeId    = (int32)M.nodes[k].id;
            d.Ux = (float)u[6 * k + 0];
            d.Uy = (float)u[6 * k + 1];
            d.Uz = (float)u[6 * k + 2];
            d.Rx = (float)u[6 * k + 3];
            d.Ry = (float)u[6 * k + 4];
            d.Rz = (float)u[6 * k + 5];
            out.Add(d);
        }
        return out;
    }
}

namespace FrameCoreUE
{
    // Forward from FrameCoreUEResultMarshal.cpp (same module).
    FRAMECOREUE_API FFrameSolveResult ToBlueprint(const frame::FrameModel& M, const frame::SolveResult& R);
}

namespace FrameCoreUE
{

// --- Modal -------------------------------------------------------------------
FFrameModalResult ToBlueprint(const frame::FrameModel& M, const frame::ModalResult& R)
{
    FFrameModalResult out;
    out.bSingular  = R.singular;
    out.Diagnostic = FString(UTF8_TO_TCHAR(R.diagnostic.c_str()));
    out.Modes.Reserve((int32)R.modes.size());
    for (const frame::ModeShape& mode : R.modes)
    {
        FFrameModeShape s;
        s.Omega  = (float)mode.omega;
        s.FreqHz = (float)mode.freqHz;
        s.Period = (mode.freqHz > 1e-12) ? (float)(1.0 / mode.freqHz) : TNumericLimits<float>::Max();
        s.Shape  = Marshal6NToDisplacements(M, mode.shape);
        out.Modes.Add(s);
    }
    return out;
}

// --- Buckling ----------------------------------------------------------------
FFrameBucklingResult ToBlueprint(const frame::FrameModel& M, const frame::BucklingResult& R)
{
    FFrameBucklingResult out;
    out.bSingular              = R.singular;
    out.Diagnostic             = FString(UTF8_TO_TCHAR(R.diagnostic.c_str()));
    out.CriticalFactor         = (float)R.criticalFactor;
    out.ReportedCriticalFactor = (float)R.reportedCriticalFactor;
    out.KnockdownFactor        = (float)R.knockdownFactor;
    out.ModeShape              = Marshal6NToDisplacements(M, R.mode);
    return out;
}

// --- Response spectrum -------------------------------------------------------
FFrameResponseSpectrumResult ToBlueprint(const frame::FrameModel& M, const frame::ResponseSpectrumResult& R)
{
    FFrameResponseSpectrumResult out;
    out.bSingular         = R.singular;
    out.Diagnostic        = FString(UTF8_TO_TCHAR(R.diagnostic.c_str()));
    out.PeakDisplacements = Marshal6NToDisplacements(M, R.u);
    out.BaseShear         = (float)R.baseShear;
    out.EffMass.Reserve((int32)R.effMass.size());
    for (frame::real m : R.effMass) out.EffMass.Add((float)m);
    out.TotalMass         = (float)R.totalMass;
    return out;
}

// --- Modal dynamics (transient) ---------------------------------------------
FFrameModalTimeHistory ToBlueprint(const frame::FrameModel& M, const frame::ModalTimeHistory& H)
{
    FFrameModalTimeHistory out;
    out.bSingular  = H.singular;
    out.Diagnostic = FString(UTF8_TO_TCHAR(H.diagnostic.c_str()));
    out.Dt         = (float)H.dt;
    out.Steps.Reserve((int32)H.u.size());
    for (int32 step = 0; step < (int32)H.u.size(); ++step)
    {
        FFrameModalTimeStep s;
        s.StepIndex     = step;
        s.Time          = (float)(H.dt * step);
        s.Displacements = Marshal6NToDisplacements(M, H.u[step]);
        out.Steps.Add(s);
    }
    return out;
}

// --- Envelope ---------------------------------------------------------------
FFrameLoadEnvelope ToBlueprint(const frame::FrameModel& /*M*/, const frame::ResultEnvelope& E)
{
    FFrameLoadEnvelope out;
    out.bSingular  = E.singular;
    out.Diagnostic = FString(UTF8_TO_TCHAR(E.diagnostic.c_str()));
    auto FillFloat = [](TArray<float>& dst, const std::vector<frame::real>& src)
    {
        dst.Reserve((int32)src.size());
        for (frame::real v : src) dst.Add((float)v);
    };
    FillFloat(out.UMax,     E.uMax);
    FillFloat(out.UMin,     E.uMin);
    FillFloat(out.ReactMax, E.reactMax);
    FillFloat(out.ReactMin, E.reactMin);
    auto FillForces = [](TArray<FFrameMemberEndForces>& dst,
                          const std::vector<frame::MemberEndForces>& src)
    {
        dst.Reserve((int32)src.size());
        for (const frame::MemberEndForces& f : src) dst.Add(MarshalEndForces(f));
    };
    FillForces(out.EndIMax, E.endIMax);
    FillForces(out.EndIMin, E.endIMin);
    FillForces(out.EndJMax, E.endJMax);
    FillForces(out.EndJMin, E.endJMin);
    return out;
}

// --- P-Delta -----------------------------------------------------------------
FFramePDeltaResult ToBlueprint(const frame::FrameModel& M, const frame::PDeltaResult& R)
{
    FFramePDeltaResult out;
    out.bConverged    = R.converged;
    out.bDiverged     = R.diverged;
    out.Iterations    = R.iterations;
    out.LastIncrement = (float)R.lastIncrement;
    out.FinalState    = ToBlueprint(M, R.finalState);
    return out;
}

// --- Tension-only ------------------------------------------------------------
FFrameTensionOnlyResult ToBlueprint(const frame::FrameModel& M, const frame::TensionOnlyResult& R)
{
    FFrameTensionOnlyResult out;
    out.bConverged = R.converged;
    out.bCycled    = R.cycled;
    out.Iterations = R.iterations;
    out.FinalState = ToBlueprint(M, R.finalState);
    out.Slack.Reserve((int32)R.slack.size());
    for (frame::MemberId id : R.slack) out.Slack.Add((int32)id);
    return out;
}

// --- Size optimisation -------------------------------------------------------
FFrameSizeOptResult ToBlueprint(const frame::FrameModel& /*M*/, const frame::SizeOptResult& R)
{
    FFrameSizeOptResult out;
    out.FinalAreas.Reserve((int32)R.finalAreas.size());
    for (frame::real a : R.finalAreas) out.FinalAreas.Add((float)a);
    out.FinalSections.Reserve((int32)R.finalSections.size());
    for (const frame::Section& s : R.finalSections)
    {
        FFrameSection fs;
        fs.A   = (float)s.A;
        fs.Iy  = (float)s.Iy;
        fs.Iz  = (float)s.Iz;
        fs.J   = (float)s.J;
        fs.Cy  = (float)s.cy;
        fs.Cz  = (float)s.cz;
        fs.Asy = (float)s.Asy;
        fs.Asz = (float)s.Asz;
        fs.Zy  = (float)s.Zy;
        fs.Zz  = (float)s.Zz;
        fs.Shape = (s.shape == frame::Section::Shape::Circular)
                   ? EFrameSectionShape::Circular
                   : EFrameSectionShape::Rectangular;
        out.FinalSections.Add(fs);
    }
    out.FinalDC.Reserve((int32)R.finalDC.size());
    for (frame::real v : R.finalDC) out.FinalDC.Add((float)v);
    out.DCHistory.Reserve((int32)R.dcHistory.size());
    for (frame::real v : R.dcHistory) out.DCHistory.Add((float)v);
    out.WeightHistory.Reserve((int32)R.weightHistory.size());
    for (frame::real v : R.weightHistory) out.WeightHistory.Add((float)v);
    out.bConverged     = R.converged;
    out.bCycled        = R.cycled;
    out.bSingular      = R.singular;
    out.bInvalidDemand = R.invalidDemand;
    out.Iterations     = R.iterations;
    return out;
}

// --- BESO --------------------------------------------------------------------
FFrameBESOResult ToBlueprint(const frame::BESOResult& R)
{
    FFrameBESOResult out;
    out.FinalActive.Reserve((int32)R.finalActive.size());
    for (char b : R.finalActive) out.FinalActive.Add((int32)b);
    out.BestActive.Reserve((int32)R.bestActive.size());
    for (char b : R.bestActive)  out.BestActive.Add((int32)b);
    out.VolFracHistory.Reserve((int32)R.volFracHistory.size());
    for (frame::real v : R.volFracHistory)    out.VolFracHistory.Add((float)v);
    out.ComplianceHistory.Reserve((int32)R.complianceHistory.size());
    for (frame::real v : R.complianceHistory) out.ComplianceHistory.Add((float)v);
    out.ProtectedMembers.Reserve((int32)R.protectedMembers.size());
    for (frame::MemberId id : R.protectedMembers) out.ProtectedMembers.Add((int32)id);
    out.BestIter   = R.bestIter;
    out.Iterations = R.iterations;
    out.Reason     = MapBESOStop(R.reason);
    out.bConverged = R.converged;
    return out;
}

// --- Corotational ------------------------------------------------------------
FFrameCorotationalResult ToBlueprint(const frame::FrameModel& M, const frame::CorotationalResult& R)
{
    FFrameCorotationalResult out;
    out.bConverged         = R.converged;
    out.bDiverged          = R.diverged;
    out.LoadStepsCompleted = R.loadStepsCompleted;
    out.TotalIterations    = R.totalIterations;
    out.LastResidual       = (float)R.lastResidual;
    out.FinalState         = ToBlueprint(M, R.finalState);
    out.PathLambda.Reserve((int32)R.pathLambda.size());
    for (frame::real v : R.pathLambda) out.PathLambda.Add((float)v);
    out.PathDisp.Reserve((int32)R.pathDisp.size());
    for (frame::real v : R.pathDisp)   out.PathDisp.Add((float)v);
    return out;
}

// --- DynCollapse ------------------------------------------------------------
static FFrameFragmentCluster MarshalFragment(const frame::FragmentCluster& F)
{
    FFrameFragmentCluster out;
    out.Nodes.Reserve((int32)F.nodes.size());
    for (frame::NodeId id : F.nodes) out.Nodes.Add((int32)id);
    out.Members.Reserve((int32)F.members.size());
    for (frame::MemberId id : F.members) out.Members.Add((int32)id);
    out.Shells.Reserve((int32)F.shells.size());
    for (int id : F.shells) out.Shells.Add(id);
    out.Mass = (float)F.mass;
    out.COM  = FVector((float)F.com.x, (float)F.com.y, (float)F.com.z);
    out.Inertia.Reset(6);
    for (int32 k = 0; k < 6; ++k) out.Inertia.Add((float)F.inertia[k]);
    out.Vel    = FVector((float)F.vel.x, (float)F.vel.y, (float)F.vel.z);
    out.AngVel = FVector((float)F.angVel.x, (float)F.angVel.y, (float)F.angVel.z);
    return out;
}

FFrameDynCollapseResult ToBlueprint(const frame::FrameModel& /*M*/, const frame::DynCollapseHistory& H)
{
    FFrameDynCollapseResult out;
    out.Outcome    = MapDynOutcome(H.outcome);
    out.Diagnostic = FString(UTF8_TO_TCHAR(H.diagnostic.c_str()));

    out.Events.Reserve((int32)H.events.size());
    for (const frame::DynCollapseEvent& ev : H.events)
    {
        FFrameDynCollapseEvent e;
        e.Time = (float)ev.t;
        e.Mode = MapFailMode(ev.mode);
        e.RemovedMembers.Reserve((int32)ev.removedMembers.size());
        for (frame::MemberId id : ev.removedMembers) e.RemovedMembers.Add((int32)id);
        e.RemovedShells.Reserve((int32)ev.removedShells.size());
        for (int id : ev.removedShells) e.RemovedShells.Add(id);
        e.FormedHinges.Reserve((int32)ev.formedHinges.size());
        for (const frame::CollapseHingeEvent& he : ev.formedHinges)
        {
            FFrameCollapseHingeEvent fhe;
            fhe.MemberId = (int32)he.member;
            fhe.Dof      = he.dof;
            fhe.Mp       = (float)he.Mp;
            e.FormedHinges.Add(fhe);
        }
        e.Detached.Reserve((int32)ev.detached.size());
        for (const frame::FragmentCluster& f : ev.detached) e.Detached.Add(MarshalFragment(f));
        e.TruncationResidual = (float)ev.truncationResidual;
        e.EnergyBefore       = (float)ev.energyBefore;
        e.EnergyAfter        = (float)ev.energyAfter;
        out.Events.Add(e);
    }

    out.Frames.Reserve((int32)H.frames.size());
    for (const frame::DynCollapseFrame& fr : H.frames)
    {
        FFrameDynCollapseFrame f;
        f.Time = (float)fr.t;
        f.UFlat.Reserve((int32)fr.u.size());
        for (frame::real v : fr.u) f.UFlat.Add((float)v);
        f.VFlat.Reserve((int32)fr.v.size());
        for (frame::real v : fr.v) f.VFlat.Add((float)v);
        out.Frames.Add(f);
    }
    return out;
}

} // namespace FrameCoreUE
