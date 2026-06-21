#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCore/StressField.h"
#include "FrameCore/StressKernel.h"

namespace FrameCoreUE
{

static FFrameStressFieldSample MarshalSample(const frame::MemberStressSample& s)
{
    FFrameStressFieldSample out;
    out.X            = (float)s.x;
    out.SigmaCompMax = (float)s.sigmaCompMax;
    out.SigmaTensMax = (float)s.sigmaTensMax;
    out.TauShear     = (float)s.tauShear;
    out.TauTorsion   = (float)s.tauTorsion;
    out.N            = (float)s.N;
    out.Vy           = (float)s.Vy;
    out.Vz           = (float)s.Vz;
    out.T            = (float)s.T;
    out.My           = (float)s.My;
    out.Mz           = (float)s.Mz;
    out.SigmaTopY    = (float)s.sigmaFiberTopY;
    out.SigmaBotY    = (float)s.sigmaFiberBotY;
    out.SigmaPlusZ   = (float)s.sigmaFiberPlusZ;
    out.SigmaMinusZ  = (float)s.sigmaFiberMinusZ;
    return out;
}

static FFrameMemberStressTrace MarshalTrace(const frame::MemberStressTrace& tr)
{
    FFrameMemberStressTrace out;
    out.MemberIdx = tr.memberIdx;
    out.MemberId  = tr.memberId;
    out.Samples.Reserve((int32)tr.samples.size());
    for (const frame::MemberStressSample& s : tr.samples)
    {
        out.Samples.Add(MarshalSample(s));
    }
    return out;
}

static FFrameShellStressPoint MarshalPoint(const frame::ShellStressPoint& p)
{
    FFrameShellStressPoint out;
    out.CornerIdx = p.cornerIdx;
    out.SigmaXX   = (float)p.sigmaXX;
    out.SigmaYY   = (float)p.sigmaYY;
    out.TauXY     = (float)p.tauXY;
    out.Sigma1    = (float)p.sigma1;
    out.Sigma2    = (float)p.sigma2;
    out.VonMises  = (float)p.vonMises;
    out.ThetaRad  = (float)p.thetaRad;
    return out;
}

static FFrameShellStressLayer MarshalLayer(const frame::ShellStressLayer& L)
{
    FFrameShellStressLayer out;
    out.ShellIdx    = L.shellIdx;
    out.ShellId     = L.shellId;
    out.bIsTopLayer = (L.layer == frame::ShellLayer::Top);
    out.Center      = MarshalPoint(L.center);
    out.Corners.Reserve(4);
    for (int i = 0; i < 4; ++i) { out.Corners.Add(MarshalPoint(L.corners[i])); }
    return out;
}

FFrameStressField ToBlueprint(const frame::StressField& field)
{
    FFrameStressField out;

    out.Members.Reserve((int32)field.members.size());
    for (const frame::MemberStressTrace& tr : field.members) { out.Members.Add(MarshalTrace(tr)); }

    out.ShellsTop.Reserve((int32)field.shellsTop.size());
    for (const frame::ShellStressLayer& L : field.shellsTop) { out.ShellsTop.Add(MarshalLayer(L)); }

    out.ShellsBot.Reserve((int32)field.shellsBot.size());
    for (const frame::ShellStressLayer& L : field.shellsBot) { out.ShellsBot.Add(MarshalLayer(L)); }

    out.GlobalMaxFiberSigma     = (float)field.globalMaxFiberSigma;
    out.GlobalMaxVonMises       = (float)field.globalMaxVonMises;
    out.GoverningMemberId       = field.governingMemberId;
    out.GoverningShellId        = field.governingShellId;
    out.GoverningShellCorner    = field.governingShellCorner;
    out.bGoverningShellLayerIsTop = (field.governingShellLayer == frame::ShellLayer::Top);

    return out;
}

} // namespace FrameCoreUE
