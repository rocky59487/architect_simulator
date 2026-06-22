// v3.4 Phase 2 -- marshal layer frame::SolveResult -> FFrameSolveResult, including
// the post-process worstUtilization / worstShellUtilization screens. Pure functions;
// no engine source touched.

#include "FrameCoreUE/FrameCoreUEResultTypes.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Shell.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/ISectionStrength.h"
#include "FrameCore/ElasticAllowable.h"

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
            case frame::FailMode::None:
            default:                             return EFrameFailMode::None;
        }
    }

    FFrameMemberEndForces MarshalEndForces(const frame::MemberEndForces& f)
    {
        FFrameMemberEndForces out;
        out.N  = static_cast<float>(f.N);
        out.Vy = static_cast<float>(f.Vy);
        out.Vz = static_cast<float>(f.Vz);
        out.T  = static_cast<float>(f.T);
        out.My = static_cast<float>(f.My);
        out.Mz = static_cast<float>(f.Mz);
        return out;
    }

    FFrameDemandResult MarshalDemand(const frame::DemandResult& d)
    {
        FFrameDemandResult out;
        out.Risk  = static_cast<float>(d.risk);
        out.Mode  = MapFailMode(d.mode);
        out.SComp = static_cast<float>(d.sComp);
        out.STens = static_cast<float>(d.sTens);
        out.Tau   = static_cast<float>(d.tau);
        out.STor  = static_cast<float>(d.sTor);
        return out;
    }
}

namespace FrameCoreUE
{

FFrameDemandSummary ToBlueprint(const frame::DemandSummary& D)
{
    FFrameDemandSummary out;
    out.MaxDC        = static_cast<float>(D.maxDC);
    out.SafetyFactor = std::isfinite(D.safetyFactor)
                       ? static_cast<float>(D.safetyFactor)
                       : TNumericLimits<float>::Max();
    out.GoverningMemberId = static_cast<int32>(D.governingMember);
    out.Mode             = MapFailMode(D.mode);
    out.bValid           = D.valid;
    // GoverningMemberIdx (slot index) is resolved by the higher-level ToBlueprint(M, R)
    // overload because the slot-index lookup needs the model. Leave -1 here.
    return out;
}

FFrameDemandSummary ToBlueprint(const frame::DemandSummary& M, const frame::ShellDemandSummary& S)
{
    FFrameDemandSummary out = ToBlueprint(M);
    out.MaxShellDC         = static_cast<float>(S.maxDC);
    out.GoverningShellId   = static_cast<int32>(S.governingShell);
    out.bShellValid        = S.valid;
    return out;
}

FFrameSolveResult ToBlueprint(const frame::FrameModel& Model, const frame::SolveResult& R)
{
    FFrameSolveResult out;
    out.bSingular   = R.singular;
    out.Diagnostic  = FString(UTF8_TO_TCHAR(R.diagnostic.c_str()));
    out.PivotMargin = static_cast<float>(R.pivotMargin);

    const int32 NNodes = static_cast<int32>(Model.nodes.size());

    // Displacements -- per-node, 6N flat -> N x 6. Skip if u is empty (singular path).
    if (static_cast<int32>(R.u.size()) >= NNodes * 6 && NNodes > 0)
    {
        out.Displacements.Reserve(NNodes);
        for (int32 k = 0; k < NNodes; ++k)
        {
            FFrameNodalDisplacement d;
            d.NodeIndex = k;
            d.NodeId    = static_cast<int32>(Model.nodes[k].id);
            d.Ux = static_cast<float>(R.u[6 * k + 0]);
            d.Uy = static_cast<float>(R.u[6 * k + 1]);
            d.Uz = static_cast<float>(R.u[6 * k + 2]);
            d.Rx = static_cast<float>(R.u[6 * k + 3]);
            d.Ry = static_cast<float>(R.u[6 * k + 4]);
            d.Rz = static_cast<float>(R.u[6 * k + 5]);
            out.Displacements.Add(d);
        }
    }

    // Reactions -- same shape, but NaN at free DOFs (engine convention). The marshal layer
    // collapses NaN -> 0 and sets bHasConstrainedDof from Node::fixed[].
    if (static_cast<int32>(R.reactions.size()) >= NNodes * 6 && NNodes > 0)
    {
        out.Reactions.Reserve(NNodes);
        for (int32 k = 0; k < NNodes; ++k)
        {
            FFrameNodalReaction r;
            r.NodeIndex = k;
            r.NodeId    = static_cast<int32>(Model.nodes[k].id);
            const frame::Node& N = Model.nodes[k];
            for (int32 d = 0; d < 6; ++d)
            {
                if (N.fixed[d]) r.bHasConstrainedDof = true;
            }
            auto clean = [](double v) -> float
            {
                return std::isfinite(v) ? static_cast<float>(v) : 0.f;
            };
            r.Fx = clean(R.reactions[6 * k + 0]);
            r.Fy = clean(R.reactions[6 * k + 1]);
            r.Fz = clean(R.reactions[6 * k + 2]);
            r.Mx = clean(R.reactions[6 * k + 3]);
            r.My = clean(R.reactions[6 * k + 4]);
            r.Mz = clean(R.reactions[6 * k + 5]);
            out.Reactions.Add(r);
        }
    }

    // Member forces. memberForces is a vector parallel to Model.members for ACTIVE members
    // (engine convention: inactive members are skipped). Look up the original slot via the
    // pair's member id, which keeps the BP slot index meaningful even when some are skipped.
    out.MemberForces.Reserve(static_cast<int32>(R.memberForces.size()));
    for (const frame::MemberForcePair& mf : R.memberForces)
    {
        FFrameMemberInternalForces fm;
        fm.MemberId = static_cast<int32>(mf.member);
        fm.MemberIdx = Model.memberIndex(mf.member);
        fm.EndI = MarshalEndForces(mf.endI);
        fm.EndJ = MarshalEndForces(mf.endJ);
        out.MemberForces.Add(fm);
    }

    // Per-member utilization screen (ElasticAllowable D/C per end). Skipped for singular runs.
    if (!R.singular)
    {
        const frame::ElasticAllowable Screen{};
        out.MemberUtilization.Reserve(static_cast<int32>(R.memberForces.size()));
        for (const frame::MemberForcePair& mf : R.memberForces)
        {
            const int32 MemIdx = Model.memberIndex(mf.member);
            if (MemIdx < 0) continue;
            const frame::Member& Mem = Model.members[MemIdx];
            if (Mem.matIdx < 0 || Mem.matIdx >= static_cast<int>(Model.materials.size())) continue;
            if (Mem.secIdx < 0 || Mem.secIdx >= static_cast<int>(Model.sections.size()))  continue;
            const frame::Section&  Sec = Model.sections[Mem.secIdx];
            const frame::Capacity& Cap = Model.materials[Mem.matIdx].cap;

            FFrameMemberUtilization u;
            u.MemberIdx = MemIdx;
            u.MemberId  = static_cast<int32>(mf.member);
            const frame::DemandResult dI = Screen.checkSection(mf.endI, Sec, Cap);
            const frame::DemandResult dJ = Screen.checkSection(mf.endJ, Sec, Cap);
            u.EndI = MarshalDemand(dI);
            u.EndJ = MarshalDemand(dJ);
            u.Peak = (dI.risk >= dJ.risk) ? u.EndI : u.EndJ;
            out.MemberUtilization.Add(u);
        }
    }

    // Shell internal forces.
    out.ShellForces.Reserve(static_cast<int32>(R.shellForces.size()));
    for (const frame::ShellElementForces& sf : R.shellForces)
    {
        FFrameShellInternalForces fs;
        fs.ShellId   = sf.shell;
        fs.ShellIdx  = Model.shellIndex(sf.shell);
        fs.Mxx = static_cast<float>(sf.Mxx);
        fs.Myy = static_cast<float>(sf.Myy);
        fs.Mxy = static_cast<float>(sf.Mxy);
        fs.Qx  = static_cast<float>(sf.Qx);
        fs.Qy  = static_cast<float>(sf.Qy);
        fs.Nxx = static_cast<float>(sf.Nxx);
        fs.Nyy = static_cast<float>(sf.Nyy);
        fs.Nxy = static_cast<float>(sf.Nxy);
        fs.MxxCorners.Reset(4);
        fs.MyyCorners.Reset(4);
        fs.MxyCorners.Reset(4);
        for (int32 c = 0; c < 4; ++c)
        {
            fs.MxxCorners.Add(static_cast<float>(sf.MxxC[c]));
            fs.MyyCorners.Add(static_cast<float>(sf.MyyC[c]));
            fs.MxyCorners.Add(static_cast<float>(sf.MxyC[c]));
        }
        out.ShellForces.Add(fs);
    }

    // Per-shell surface vM screen (mirrors checkShellSurface in ElasticAllowable.h).
    if (!R.singular)
    {
        out.ShellUtilization.Reserve(static_cast<int32>(R.shellForces.size()));
        for (const frame::ShellElementForces& sf : R.shellForces)
        {
            const int32 ShellIdx = Model.shellIndex(sf.shell);
            if (ShellIdx < 0) continue;
            const frame::ShellQuad& Q = Model.shells[ShellIdx];
            if (!Q.active) continue;
            if (Q.matIdx < 0 || Q.matIdx >= static_cast<int>(Model.materials.size())) continue;
            const frame::Capacity& Cap = Model.materials[Q.matIdx].cap;
            const frame::ShellDemandResult sd = frame::checkShellSurface(sf, Q.t, Cap);
            FFrameShellUtilization u;
            u.ShellIdx = ShellIdx;
            u.ShellId  = sf.shell;
            u.Risk     = static_cast<float>(sd.risk);
            u.Corner   = sd.corner;
            u.bTop     = sd.top;
            out.ShellUtilization.Add(u);
        }
    }

    // Demand summary aggregate. Skip for singular runs (worstUtilization is a free
    // post-process but undefined on a singular SolveResult, and the engine API doesn't
    // promise safety there).
    if (!R.singular)
    {
        const frame::DemandSummary M = frame::worstUtilization(Model, R);
        const frame::ShellDemandSummary S = frame::worstShellUtilization(Model, R);
        out.Utilization = ToBlueprint(M, S);

        // Resolve slot indices (DemandSummary only carries user-assigned ids).
        if (M.valid) out.Utilization.GoverningMemberIdx = Model.memberIndex(M.governingMember);
        if (S.valid) out.Utilization.GoverningShellIdx  = Model.shellIndex(S.governingShell);
    }

    return out;
}

} // namespace FrameCoreUE
