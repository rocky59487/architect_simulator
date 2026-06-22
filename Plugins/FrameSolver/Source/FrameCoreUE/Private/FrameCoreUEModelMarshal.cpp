// v3.4 Phase 1 -- marshal layer FFrameModelDef -> frame::FrameModel and
// FFrameSolveOptions -> frame::SolveOptions. Pure function set; no engine source touched.
// Range-checks every TArray length on the way (USTRUCT TArray<bool>/TArray<float> default
// to empty; an empty array is treated as "all zero / all false" of the right length).

#include "FrameCoreUE/FrameCoreUEModelTypes.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Shell.h"
#include "FrameCore/Load.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveOptions.h"

namespace FrameCoreUE
{

bool FromBlueprint(const FFrameSolveOptions& In, frame::SolveOptions& Out)
{
    Out.pivotTol                   = static_cast<frame::real>(In.PivotTol);
    Out.enableReleases             = In.bEnableReleases;
    Out.useTimoshenko              = In.bUseTimoshenko;
    Out.useIncompatibleMembrane    = In.bUseIncompatibleMembrane;
    Out.useDKQPlate                = In.bUseDKQPlate;
    Out.shellGeometricStiffness    = In.bShellGeometricStiffness;
    Out.useWarpingCorrection       = In.bUseWarpingCorrection;
    Out.warpTolerance              = static_cast<frame::real>(In.WarpTolerance);
    Out.shellCurvatureMaxAngleDeg  = static_cast<frame::real>(In.ShellCurvatureMaxAngleDeg);
    Out.useSupernodalPrimary       = In.bUseSupernodalPrimary;
    return true;
}

static frame::Material ToEngineMat(const FFrameMaterial& In)
{
    frame::Material m(static_cast<frame::real>(In.E),
                      static_cast<frame::real>(In.G),
                      static_cast<frame::real>(In.Rho));
    m.nu = static_cast<frame::real>(In.Nu);
    m.fy = static_cast<frame::real>(In.Fy);
    // If Comp/Tens/Shear are all zero we leave cap default (vm=0 -> screens as +inf D/C
    // on demand; matches engine 0-init semantics). Otherwise rebuild via Capacity::make
    // so Bend/Tors/VM derivations are consistent (engine convention; the BP-side Bend/
    // Tors/VM fields are advisory and not re-applied here -- a future v3.x can add an
    // "override derived" toggle if needed).
    if (In.Cap.Comp != 0.f || In.Cap.Tens != 0.f || In.Cap.Shear != 0.f)
    {
        m.cap = frame::Capacity::make(static_cast<frame::real>(In.Cap.Comp),
                                      static_cast<frame::real>(In.Cap.Tens),
                                      static_cast<frame::real>(In.Cap.Shear));
    }
    return m;
}

static frame::Section ToEngineSec(const FFrameSection& In)
{
    frame::Section s;
    s.A   = static_cast<frame::real>(In.A);
    s.Iy  = static_cast<frame::real>(In.Iy);
    s.Iz  = static_cast<frame::real>(In.Iz);
    s.J   = static_cast<frame::real>(In.J);
    s.cy  = static_cast<frame::real>(In.Cy);
    s.cz  = static_cast<frame::real>(In.Cz);
    s.Asy = static_cast<frame::real>(In.Asy);
    s.Asz = static_cast<frame::real>(In.Asz);
    s.Zy  = static_cast<frame::real>(In.Zy);
    s.Zz  = static_cast<frame::real>(In.Zz);
    s.shape = (In.Shape == EFrameSectionShape::Circular)
              ? frame::Section::Shape::Circular
              : frame::Section::Shape::Rectangular;
    return s;
}

static bool ToEngineNode(const FFrameNode& In, frame::Node& Out, FString& Err, int32 NodeIndex)
{
    Out = frame::Node(static_cast<frame::NodeId>(In.Id),
                      static_cast<frame::real>(In.Pos.X),
                      static_cast<frame::real>(In.Pos.Y),
                      static_cast<frame::real>(In.Pos.Z));
    if (In.Fixed.Num() != 0)
    {
        if (In.Fixed.Num() != 6)
        {
            Err = FString::Printf(TEXT("Nodes[%d].Fixed length=%d (expected 0 or 6)"),
                                  NodeIndex, In.Fixed.Num());
            return false;
        }
        for (int32 d = 0; d < 6; ++d) Out.fixed[d] = In.Fixed[d];
    }
    if (In.Prescribed.Num() != 0)
    {
        if (In.Prescribed.Num() != 6)
        {
            Err = FString::Printf(TEXT("Nodes[%d].Prescribed length=%d (expected 0 or 6)"),
                                  NodeIndex, In.Prescribed.Num());
            return false;
        }
        for (int32 d = 0; d < 6; ++d)
            Out.prescribed[d] = static_cast<frame::real>(In.Prescribed[d]);
    }
    return true;
}

static bool ToEngineMember(const FFrameMember& In, int32 NMaterials, int32 NSections,
                           frame::Member& Out, FString& Err, int32 MemberIndex)
{
    if (In.MatIdx < 0 || In.MatIdx >= NMaterials)
    {
        Err = FString::Printf(TEXT("Members[%d].MatIdx=%d out of range (Materials count=%d)"),
                              MemberIndex, In.MatIdx, NMaterials);
        return false;
    }
    if (In.SecIdx < 0 || In.SecIdx >= NSections)
    {
        Err = FString::Printf(TEXT("Members[%d].SecIdx=%d out of range (Sections count=%d)"),
                              MemberIndex, In.SecIdx, NSections);
        return false;
    }
    Out = frame::Member(static_cast<frame::MemberId>(In.Id),
                        static_cast<frame::NodeId>(In.I),
                        static_cast<frame::NodeId>(In.J),
                        In.MatIdx, In.SecIdx);
    Out.refVec      = frame::Vec3(static_cast<frame::real>(In.RefVec.X),
                                  static_cast<frame::real>(In.RefVec.Y),
                                  static_cast<frame::real>(In.RefVec.Z));
    Out.active      = In.bActive;
    Out.tensionOnly = In.bTensionOnly;
    if (In.Release.Num() != 0)
    {
        if (In.Release.Num() != 12)
        {
            Err = FString::Printf(TEXT("Members[%d].Release length=%d (expected 0 or 12)"),
                                  MemberIndex, In.Release.Num());
            return false;
        }
        for (int32 d = 0; d < 12; ++d) Out.release[d] = In.Release[d];
    }
    return true;
}

static bool ToEngineShell(const FFrameShellQuad& In, int32 NMaterials,
                          frame::ShellQuad& Out, FString& Err, int32 ShellIndex)
{
    if (In.MatIdx < 0 || In.MatIdx >= NMaterials)
    {
        Err = FString::Printf(TEXT("Shells[%d].MatIdx=%d out of range (Materials count=%d)"),
                              ShellIndex, In.MatIdx, NMaterials);
        return false;
    }
    if (In.N.Num() != 4)
    {
        Err = FString::Printf(TEXT("Shells[%d].N length=%d (expected 4)"), ShellIndex, In.N.Num());
        return false;
    }
    Out = frame::ShellQuad(In.Id,
                           static_cast<frame::NodeId>(In.N[0]),
                           static_cast<frame::NodeId>(In.N[1]),
                           static_cast<frame::NodeId>(In.N[2]),
                           static_cast<frame::NodeId>(In.N[3]),
                           In.MatIdx,
                           static_cast<frame::real>(In.T));
    Out.active = In.bActive;
    return true;
}

bool FromBlueprint(const FFrameModelDef& Def, frame::FrameModel& Out, FString& Err)
{
    Out = frame::FrameModel{};

    // Materials
    Out.materials.reserve(static_cast<size_t>(Def.Materials.Num()));
    for (int32 k = 0; k < Def.Materials.Num(); ++k)
    {
        Out.materials.push_back(ToEngineMat(Def.Materials[k]));
    }
    // Sections
    Out.sections.reserve(static_cast<size_t>(Def.Sections.Num()));
    for (int32 k = 0; k < Def.Sections.Num(); ++k)
    {
        Out.sections.push_back(ToEngineSec(Def.Sections[k]));
    }
    // Nodes
    Out.nodes.reserve(static_cast<size_t>(Def.Nodes.Num()));
    for (int32 k = 0; k < Def.Nodes.Num(); ++k)
    {
        frame::Node fn;
        if (!ToEngineNode(Def.Nodes[k], fn, Err, k)) return false;
        Out.nodes.push_back(fn);
    }
    // Members
    Out.members.reserve(static_cast<size_t>(Def.Members.Num()));
    for (int32 k = 0; k < Def.Members.Num(); ++k)
    {
        frame::Member fm;
        if (!ToEngineMember(Def.Members[k],
                            static_cast<int32>(Out.materials.size()),
                            static_cast<int32>(Out.sections.size()),
                            fm, Err, k)) return false;
        Out.members.push_back(fm);
    }
    // Shells
    Out.shells.reserve(static_cast<size_t>(Def.Shells.Num()));
    for (int32 k = 0; k < Def.Shells.Num(); ++k)
    {
        frame::ShellQuad fs;
        if (!ToEngineShell(Def.Shells[k],
                           static_cast<int32>(Out.materials.size()),
                           fs, Err, k)) return false;
        Out.shells.push_back(fs);
    }
    // Nodal loads
    Out.nodalLoads.reserve(static_cast<size_t>(Def.NodalLoads.Num()));
    for (int32 k = 0; k < Def.NodalLoads.Num(); ++k)
    {
        const FFrameNodalLoad& In = Def.NodalLoads[k];
        if (In.Comp.Num() != 6)
        {
            Err = FString::Printf(TEXT("NodalLoads[%d].Comp length=%d (expected 6)"),
                                  k, In.Comp.Num());
            return false;
        }
        frame::NodalLoad nl;
        nl.node = static_cast<frame::NodeId>(In.Node);
        for (int32 d = 0; d < 6; ++d) nl.comp[d] = static_cast<frame::real>(In.Comp[d]);
        Out.nodalLoads.push_back(nl);
    }
    // Member UDLs
    Out.memberUDLs.reserve(static_cast<size_t>(Def.MemberUDLs.Num()));
    for (int32 k = 0; k < Def.MemberUDLs.Num(); ++k)
    {
        const FFrameMemberUDL& In = Def.MemberUDLs[k];
        frame::MemberUDL u;
        u.member  = static_cast<frame::MemberId>(In.Member);
        u.w_local = frame::Vec3(static_cast<frame::real>(In.WLocal.X),
                                static_cast<frame::real>(In.WLocal.Y),
                                static_cast<frame::real>(In.WLocal.Z));
        Out.memberUDLs.push_back(u);
    }
    // Shell pressures
    Out.shellPressures.reserve(static_cast<size_t>(Def.ShellPressures.Num()));
    for (int32 k = 0; k < Def.ShellPressures.Num(); ++k)
    {
        frame::ShellPressure sp;
        sp.shell = Def.ShellPressures[k].Shell;
        sp.p     = static_cast<frame::real>(Def.ShellPressures[k].P);
        Out.shellPressures.push_back(sp);
    }
    return true;
}

} // namespace FrameCoreUE
