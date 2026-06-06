#include "FrameCore/Construction.h"
#include "FrameCore/EquivalentModeling.h"   // equiv::equivalentBraceArea
#include "FrameCore/Member.h"               // makeRelease, ReleasePreset
#include <cmath>

namespace frame { namespace construct {

// reserve-before-capture: store one material + one section in m, hand back pointers that
// stay valid for the model's lifetime (Members capture them).
static void prep1(FrameModel& m, const Material& mat, const Section& sec,
                  const Material*& pm, const Section*& ps) {
    m = FrameModel{};
    m.materials.reserve(1); m.sections.reserve(1);
    m.materials.push_back(mat); m.sections.push_back(sec);
    pm = &m.materials.back(); ps = &m.sections.back();
}

void buildColumn(FrameModel& m, real height, real axialP, real lateralH,
                 const Material& mat, const Section& sec) {
    const Material* pm; const Section* ps; prep1(m, mat, sec, pm, ps);
    Node n0(0, 0, 0, 0);       n0.fixAll();
    Node n1(1, 0, 0, height);
    m.nodes   = { n0, n1 };
    m.members = { Member(0, 0, 1, pm, ps) };
    NodalLoad nl; nl.node = 1; nl.comp[Uz] = -axialP; nl.comp[Ux] = lateralH;
    m.nodalLoads = { nl };
}

void buildFixedBeam(FrameModel& m, real span, real w, int nSeg,
                    const Material& mat, const Section& sec) {
    const Material* pm; const Section* ps; prep1(m, mat, sec, pm, ps);
    if (nSeg < 2) nSeg = 2;
    m.nodes.clear(); m.members.clear();
    for (int k = 0; k <= nSeg; ++k) {
        Node n(k, span * (real(k) / real(nSeg)), 0, 0);
        if (k == 0 || k == nSeg) n.fixAll();     // both ends clamped
        m.nodes.push_back(n);
    }
    for (int k = 0; k < nSeg; ++k) m.members.push_back(Member(k, k, k + 1, pm, ps));
    for (int k = 0; k < nSeg; ++k) {
        MemberUDL u; u.member = k; u.w_local = { 0, -w, 0 };
        m.memberUDLs.push_back(u);
    }
}

bool buildSlab(FrameModel& m, const grillage::PlateSpec& spec, const Material& mat, std::string& why) {
    return grillage::buildGrillage(m, spec, mat, why);
}

void buildEquivalentBraceWall(FrameModel& m, real bay, real height, real Ktarget,
                              real lateralH, const Material& mat) {
    // size the brace area so its projected axial stiffness E*A*bay^2/Ld^3 == Ktarget
    const real A = equiv::equivalentBraceArea(Ktarget, mat.E, bay, height);

    // truss-pin brace -> only axial matters; give I/J sane positive values (released anyway).
    Section sec;
    sec.A   = A;
    sec.Iy  = sec.Iz = A * A / 12.0;
    sec.J   = A * A / 6.0;
    sec.cy  = sec.cz = std::sqrt(A) / 2.0;
    sec.shape = Section::Shape::Rectangular;

    m = FrameModel{};
    m.materials.reserve(1); m.sections.reserve(1);
    m.materials.push_back(mat); m.sections.push_back(sec);
    const Material* pm = &m.materials.back();
    const Section*  ps = &m.sections.back();

    Node n0(0, 0,   0,      0); n0.fixAll();
    Node n1(1, bay, height, 0);                          // top node sways only in +X
    n1.fixed[Uy] = n1.fixed[Uz] = n1.fixed[Rx] = n1.fixed[Ry] = n1.fixed[Rz] = true;
    m.nodes = { n0, n1 };

    // Release the four bending rotations (Ry,Rz at both ends) so the brace carries no
    // moment -> its only sway stiffness is the projected axial term. We keep TORSION
    // (unlike TrussPin, which also releases Rx at both ends and would make the released
    // sub-block singular for a lone brace); with every rotation fixed at the nodes,
    // torsion is simply never engaged.
    const std::array<bool, 12> rI = makeRelease(ReleasePreset::HingeI);
    const std::array<bool, 12> rJ = makeRelease(ReleasePreset::HingeJ);
    std::array<bool, 12> rel{};
    for (int k = 0; k < 12; ++k) rel[k] = rI[k] || rJ[k];

    Member br(0, 0, 1, pm, ps);
    br.release = rel;
    m.members = { br };

    NodalLoad nl; nl.node = 1; nl.comp[Ux] = lateralH;
    m.nodalLoads = { nl };
}

}} // namespace frame::construct
