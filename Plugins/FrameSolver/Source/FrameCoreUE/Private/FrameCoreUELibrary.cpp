#include "FrameCoreUE/FrameCoreUELibrary.h"
#include "FrameCoreUE/FrameCoreUETypes.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Load.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/StressField.h"

namespace FrameCoreUE
{
    // Forward decl from FrameCoreUETypes.cpp (same module, same namespace).
    FFrameStressField ToBlueprint(const frame::StressField& Field);
}

// Local cantilever builder: mirrors fixtures::cantileverTipLoad in Private/FrameTestFixtures.h
// but inlined here so FrameCoreUE never reaches into FrameCore's Private headers
// (engine-side encapsulation is preserved). Geometry matches F68 standalone fixture:
// 100x100 rectangular section, S235-like Capacity, horizontal cantilever along +X.
static frame::FrameModel BuildCantileverFixture(double P, double L)
{
    using namespace frame;

    Material mat(210000.0, 80769.0, 7850.0);   // E (MPa), G (MPa), rho (kg/m3)
    mat.cap = Capacity::make(300.0, 300.0, 180.0);
    Section sec = Section::Rectangular(100.0, 100.0);   // b, d (mm)

    FrameModel m;
    m.materials = { mat };
    m.sections  = { sec };

    Node n0(0, 0, 0, 0); n0.fixAll();
    Node n1(1, L, 0, 0);
    m.nodes   = { n0, n1 };
    m.members = { Member(0, 0, 1, 0, 0) };           // matIdx=0, secIdx=0

    NodalLoad nl;
    nl.node      = 1;
    nl.comp[Uz]  = P;                                // +Z tip load (matches F68)
    m.nodalLoads = { nl };

    return m;
}

FFrameStressField UFrameCoreStressFieldLibrary::ComputeCantileverFixture(
    float P, float L, int32 SamplesPerSpan)
{
    // Engine -> POD -> USTRUCT. Lossy double->float cast in ToBlueprint.
    const frame::FrameModel m = BuildCantileverFixture((double)P, (double)L);
    const frame::SolveResult r = frame::solve(m);
    if (r.singular) { return FFrameStressField{}; }

    // Clamp samplesPerSpan to engine's minimum (>= 2). BP designer who passes < 2 gets
    // samplesPerSpan=11 (the demo default) rather than an empty trace or a crash.
    const int32 n = (SamplesPerSpan < 2) ? 11 : SamplesPerSpan;
    const frame::StressField field = frame::computeStressField(m, r, n);
    return FrameCoreUE::ToBlueprint(field);
}

int32 UFrameCoreStressFieldLibrary::GetGoverningMemberId(const FFrameStressField& Field)
{
    return Field.GoverningMemberId;
}

int32 UFrameCoreStressFieldLibrary::GetGoverningShellId(const FFrameStressField& Field)
{
    return Field.GoverningShellId;
}

float UFrameCoreStressFieldLibrary::GetGlobalMaxFiberSigma(const FFrameStressField& Field)
{
    return Field.GlobalMaxFiberSigma;
}

float UFrameCoreStressFieldLibrary::GetGlobalMaxVonMises(const FFrameStressField& Field)
{
    return Field.GlobalMaxVonMises;
}

TArray<FFrameStressFieldSample> UFrameCoreStressFieldLibrary::GetMemberSamples(
    const FFrameStressField& Field, int32 MemberIdx)
{
    if (MemberIdx < 0 || MemberIdx >= Field.Members.Num()) { return {}; }
    return Field.Members[MemberIdx].Samples;
}
