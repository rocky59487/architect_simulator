#include "FrameCore/StressField.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/Member.h"
#include "FrameCore/Shell.h"
#include "FrameCore/Section.h"
#include "FrameCore/Load.h"
#include <algorithm>
#include <cmath>

namespace frame {

namespace {

inline real memberLength(const FrameModel& model, const Member& m) {
    const int ni = model.nodeIndex(m.i);
    const int nj = model.nodeIndex(m.j);
    if (ni < 0 || nj < 0) return real(0);
    const Vec3 d = model.nodes[(size_t)nj].pos - model.nodes[(size_t)ni].pos;
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// Reconstruct internal forces at arc-length x in LOCAL coords from end-i forces + UDL.
// FrameCore's MemberEndForces sign convention:
//   N is compression-positive (along local +x at end-i).
//   For a cantilever tip-loaded along local +y: Vy_i = +P, Mz_i = +P*L, so the
//   bending moment must DECREASE toward the free end -> dMz/dx = -Vy.
//
//   N(x)  = N_i  - w_x * x                                (axial UDL drains N)
//   Vy(x) = Vy_i - w_y * x                                (linear, dV/dx = -w)
//   Vz(x) = Vz_i - w_z * x                                (linear)
//   Mz(x) = Mz_i - Vy_i * x + 0.5 * w_y * x^2             (quadratic, dMz/dx = -Vy)
//   My(x) = My_i + Vz_i * x - 0.5 * w_z * x^2             (about local y, opposite sign of Mz block)
//   T(x)  = T_i                                           (no torsion UDL in MemberUDL)
//
// Calibrated against F68 (cantilever tip load) so sample[0] matches checkSection(endI,.)
// AND sample[L] vanishes at the free end. F70's D/C interlock asserts the same
// equivalence against ElasticAllowable bit-exact on a mixed model.
inline void internalForcesAtX(const MemberEndForces& endI, const Vec3& w, real x,
                              real& N, real& Vy, real& Vz, real& T, real& My, real& Mz)
{
    N  = endI.N  - w.x * x;
    Vy = endI.Vy - w.y * x;
    Vz = endI.Vz - w.z * x;
    T  = endI.T;
    Mz = endI.Mz - endI.Vy * x + real(0.5) * w.y * x * x;
    My = endI.My + endI.Vz * x - real(0.5) * w.z * x * x;
}

inline void fillSample(MemberStressSample& s,
                       real x, const Section& sec,
                       real N, real Vy, real Vz, real T, real My, real Mz)
{
    s.x = x;
    s.N = N;  s.Vy = Vy;  s.Vz = Vz;  s.T = T;  s.My = My;  s.Mz = Mz;

    s.sigmaFiberTopY   = memberFiberSigma(N, sec.A, My, Mz, sec.Iy, sec.Iz, sec.cy, sec.cz, MemberFiber::TopY);
    s.sigmaFiberBotY   = memberFiberSigma(N, sec.A, My, Mz, sec.Iy, sec.Iz, sec.cy, sec.cz, MemberFiber::BotY);
    s.sigmaFiberPlusZ  = memberFiberSigma(N, sec.A, My, Mz, sec.Iy, sec.Iz, sec.cy, sec.cz, MemberFiber::PlusZ);
    s.sigmaFiberMinusZ = memberFiberSigma(N, sec.A, My, Mz, sec.Iy, sec.Iz, sec.cy, sec.cz, MemberFiber::MinusZ);

    const CornerSigmaPair csp = memberCornerSigmaMax(N, sec.A, My, Mz, sec.Wy(), sec.Wz(), sec.shape);
    s.sigmaCompMax = csp.sComp;
    s.sigmaTensMax = csp.sTens;

    s.tauShear   = memberShearPeak(Vy, Vz, sec.A, sec.shape);
    s.tauTorsion = memberTorsionTau(T, sec.J, sec.cy, sec.cz, sec.shape);
}

inline void fillShellPoint(ShellStressPoint& p, int cornerIdx,
                           real Nxx, real Nyy, real Nxy,
                           real Mxx, real Myy, real Mxy,
                           real t, ShellLayer layer)
{
    p.cornerIdx = cornerIdx;
    real sx = 0, sy = 0, tau = 0;
    shellLayerSigma(Nxx, Nyy, Nxy, Mxx, Myy, Mxy, t, layer, sx, sy, tau);
    p.sigmaXX = sx;
    p.sigmaYY = sy;
    p.tauXY   = tau;
    const PrincipalStress ps = principalStress(sx, sy, tau);
    p.sigma1   = ps.s1;
    p.sigma2   = ps.s2;
    p.vonMises = ps.vonMises;
    p.thetaRad = ps.theta;
}

} // namespace anon

StressField computeStressField(const FrameModel& model,
                               const SolveResult& sr,
                               int samplesPerSpan)
{
    StressField fld;
    if (samplesPerSpan < 2) samplesPerSpan = 2;

    // -------- Member sweep --------------------------------------------------
    // Build a member-id -> UDL index lookup (typical model has few UDLs; linear
    // scan is fine but a hash is robust against pathological counts).
    const size_t nM = std::min(model.members.size(), sr.memberForces.size());
    fld.members.reserve(nM);
    real worstMemberSigma = 0;
    // v3.3 (U-07): track the governing element by its INTERNAL INDEX (size_t e),
    // not its user-assigned id. -1 sentinel is unambiguous against valid indices,
    // unlike id-0 which previously collided. See StressField.h.
    int  worstMemberIdx   = -1;

    // Sum ALL UDL entries for a member: the solver aggregates them via equivalent nodal
    // loads in the assembler, so a load-combination builder that appends rather than
    // replaces produces consistent stress traces here. Caught by the v3.1.0 release audit
    // (A-11): pre-fix returned only the first match, silently diverging from worstUtilization.
    auto findUdl = [&](MemberId id) -> Vec3 {
        Vec3 sum{0, 0, 0};
        for (const MemberUDL& u : model.memberUDLs) {
            if (u.member == id) {
                sum.x = sum.x + u.w_local.x;
                sum.y = sum.y + u.w_local.y;
                sum.z = sum.z + u.w_local.z;
            }
        }
        return sum;
    };

    for (size_t e = 0; e < nM; ++e) {
        const Member& mem = model.members[e];
        if (!mem.active) continue;
        if (mem.matIdx < 0 || mem.matIdx >= (int)model.materials.size()) continue;
        if (mem.secIdx < 0 || mem.secIdx >= (int)model.sections.size())  continue;

        const Section& sec = model.sections[(size_t)mem.secIdx];
        const real L = memberLength(model, mem);
        if (!(L > 0)) continue;

        const Vec3 w = findUdl(mem.id);
        const MemberEndForces& endI = sr.memberForces[e].endI;

        MemberStressTrace trace;
        trace.memberIdx = (int)e;
        trace.memberId  = mem.id;
        trace.samples.resize((size_t)samplesPerSpan);

        for (int k = 0; k < samplesPerSpan; ++k) {
            const real x = (samplesPerSpan == 1) ? real(0)
                                                 : L * real(k) / real(samplesPerSpan - 1);
            real N=0, Vy=0, Vz=0, T=0, My=0, Mz=0;
            internalForcesAtX(endI, w, x, N, Vy, Vz, T, My, Mz);
            fillSample(trace.samples[(size_t)k], x, sec, N, Vy, Vz, T, My, Mz);

            const real worstHere = std::max(trace.samples[(size_t)k].sigmaCompMax,
                                            trace.samples[(size_t)k].sigmaTensMax);
            if (worstHere > worstMemberSigma) {
                worstMemberSigma = worstHere;
                worstMemberIdx   = static_cast<int>(e);
            }
        }
        fld.members.push_back(std::move(trace));
    }

    fld.globalMaxFiberSigma = worstMemberSigma;
    fld.governingMemberIdx  = worstMemberIdx;

    // -------- Shell sweep ---------------------------------------------------
    const size_t nS = std::min(model.shells.size(), sr.shellForces.size());
    fld.shellsTop.reserve(nS);
    fld.shellsBot.reserve(nS);
    real worstVM     = 0;
    // v3.3 (U-07): index, not id. See member sweep above.
    int  worstShellIdx = -1;
    ShellLayer worstLayer = ShellLayer::Top;
    int  worstCorner = -1;

    for (size_t s = 0; s < nS; ++s) {
        const ShellQuad& sh = model.shells[s];
        if (!sh.active) continue;
        if (!(sh.t > 0)) continue;
        if (sh.matIdx < 0 || sh.matIdx >= (int)model.materials.size()) continue;

        const ShellElementForces& fc = sr.shellForces[s];

        for (int face = 0; face < 2; ++face) {
            const ShellLayer layer = (face == 0) ? ShellLayer::Top : ShellLayer::Bot;
            ShellStressLayer slr;
            slr.shellIdx = (int)s;
            slr.shellId  = sh.id;
            slr.layer    = layer;

            fillShellPoint(slr.center, -1,
                           fc.Nxx, fc.Nyy, fc.Nxy,
                           fc.Mxx, fc.Myy, fc.Mxy,
                           sh.t, layer);
            if (slr.center.vonMises > worstVM) {
                worstVM = slr.center.vonMises; worstShellIdx = static_cast<int>(s);
                worstLayer = layer; worstCorner = -1;
            }

            for (int kc = 0; kc < 4; ++kc) {
                fillShellPoint(slr.corners[kc], kc,
                               fc.Nxx, fc.Nyy, fc.Nxy,
                               fc.MxxC[kc], fc.MyyC[kc], fc.MxyC[kc],
                               sh.t, layer);
                if (slr.corners[kc].vonMises > worstVM) {
                    worstVM = slr.corners[kc].vonMises; worstShellIdx = static_cast<int>(s);
                    worstLayer = layer; worstCorner = kc;
                }
            }

            if (face == 0) fld.shellsTop.push_back(std::move(slr));
            else           fld.shellsBot.push_back(std::move(slr));
        }
    }

    fld.globalMaxVonMises     = worstVM;
    fld.governingShellIdx     = worstShellIdx;
    fld.governingShellLayer   = worstLayer;
    fld.governingShellCorner  = worstCorner;
    return fld;
}

} // namespace frame
