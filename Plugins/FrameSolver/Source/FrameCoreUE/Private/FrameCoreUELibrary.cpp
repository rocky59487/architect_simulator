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

// v3.3 (U-01): JSON parsing for ComputeFromJsonModel
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogFrameCoreUEJson, Log, All);

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

int32 UFrameCoreStressFieldLibrary::GetGoverningMemberIdx(const FFrameStressField& Field)
{
    return Field.GoverningMemberIdx;
}

int32 UFrameCoreStressFieldLibrary::GetGoverningShellIdx(const FFrameStressField& Field)
{
    return Field.GoverningShellIdx;
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

// ---------------------------------------------------------------------------
// v3.3 (U-01) -- BP JSON model loader
// ---------------------------------------------------------------------------
namespace
{
    // Subset of dispatcher model.set schema:
    //   { "materials": [{ "E", "G", "rho", "nu", "fy", "cap": {"comp","tens","shear"} }],
    //     "sections":  [{ "A", "Iy", "Iz", "J", "cy", "cz", "Asy", "Asz", "Zy", "Zz", "shape": "rectangular"|"circular" }],
    //     "nodes":     [{ "id", "x", "y", "z", "fixed": [bool*6] }],
    //     "members":   [{ "id", "i", "j", "mat", "sec", "ref": [num*3], "active": bool }],
    //     "nodalLoads":[{ "node", "comp": [num*6] }],
    //     "memberUDLs":[{ "member", "w_local": [num*3] }] }
    // Returns true on success, fills `out` with the parsed model. On failure, populates
    // `err` with a one-line diagnostic; `out` may be partially populated and should not
    // be used by the caller.
    bool BuildModelFromJsonObject(const TSharedPtr<FJsonObject>& Root, frame::FrameModel& out, FString& err)
    {
        using namespace frame;
        if (!Root.IsValid()) { err = TEXT("root object is null"); return false; }

        auto GetNumberArray = [](const TSharedPtr<FJsonObject>& Obj, const FString& Key, int32 ExpectedN,
                                 TArray<double>& OutArr) -> bool
        {
            const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
            if (!Obj->TryGetArrayField(Key, Arr)) { return false; }
            if (Arr->Num() != ExpectedN) { return false; }
            OutArr.Reset(ExpectedN);
            for (int32 k = 0; k < ExpectedN; ++k)
            {
                double v = 0.0;
                if (!(*Arr)[k]->TryGetNumber(v)) { return false; }
                OutArr.Add(v);
            }
            return true;
        };

        auto GetBoolArray = [](const TSharedPtr<FJsonObject>& Obj, const FString& Key, int32 ExpectedN,
                               TArray<bool>& OutArr) -> bool
        {
            const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
            if (!Obj->TryGetArrayField(Key, Arr)) { return false; }
            if (Arr->Num() != ExpectedN) { return false; }
            OutArr.Reset(ExpectedN);
            for (int32 k = 0; k < ExpectedN; ++k)
            {
                bool b = false;
                if (!(*Arr)[k]->TryGetBool(b)) { return false; }
                OutArr.Add(b);
            }
            return true;
        };

        // materials
        const TArray<TSharedPtr<FJsonValue>>* MatsArr = nullptr;
        if (Root->TryGetArrayField(TEXT("materials"), MatsArr))
        {
            out.materials.reserve(MatsArr->Num());
            for (int32 k = 0; k < MatsArr->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* MatObj = nullptr;
                if (!(*MatsArr)[k]->TryGetObject(MatObj))
                {
                    err = FString::Printf(TEXT("materials[%d] is not an object"), k);
                    return false;
                }
                const double E   = (*MatObj)->GetNumberField(TEXT("E"));
                const double G   = (*MatObj)->GetNumberField(TEXT("G"));
                const double rho = (*MatObj)->HasField(TEXT("rho")) ? (*MatObj)->GetNumberField(TEXT("rho")) : 0.0;
                Material fm(static_cast<real>(E), static_cast<real>(G), static_cast<real>(rho));
                if ((*MatObj)->HasField(TEXT("nu"))) fm.nu = static_cast<real>((*MatObj)->GetNumberField(TEXT("nu")));
                if ((*MatObj)->HasField(TEXT("fy"))) fm.fy = static_cast<real>((*MatObj)->GetNumberField(TEXT("fy")));
                const TSharedPtr<FJsonObject>* CapObj = nullptr;
                if ((*MatObj)->TryGetObjectField(TEXT("cap"), CapObj))
                {
                    const double cC = (*CapObj)->GetNumberField(TEXT("comp"));
                    const double cT = (*CapObj)->HasField(TEXT("tens"))  ? (*CapObj)->GetNumberField(TEXT("tens"))  : cC;
                    const double cS = (*CapObj)->HasField(TEXT("shear")) ? (*CapObj)->GetNumberField(TEXT("shear")) : 0.6 * cC;
                    fm.cap = Capacity::make(static_cast<real>(cC), static_cast<real>(cT), static_cast<real>(cS));
                }
                else
                {
                    fm.cap = Capacity::make(real(300), real(300), real(180));
                }
                out.materials.push_back(fm);
            }
        }

        // sections
        const TArray<TSharedPtr<FJsonValue>>* SecsArr = nullptr;
        if (Root->TryGetArrayField(TEXT("sections"), SecsArr))
        {
            out.sections.reserve(SecsArr->Num());
            for (int32 k = 0; k < SecsArr->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* SecObj = nullptr;
                if (!(*SecsArr)[k]->TryGetObject(SecObj))
                {
                    err = FString::Printf(TEXT("sections[%d] is not an object"), k);
                    return false;
                }
                Section fs;
                fs.A   = static_cast<real>((*SecObj)->GetNumberField(TEXT("A")));
                fs.Iy  = static_cast<real>((*SecObj)->GetNumberField(TEXT("Iy")));
                fs.Iz  = static_cast<real>((*SecObj)->GetNumberField(TEXT("Iz")));
                fs.J   = static_cast<real>((*SecObj)->GetNumberField(TEXT("J")));
                fs.cy  = static_cast<real>((*SecObj)->GetNumberField(TEXT("cy")));
                fs.cz  = static_cast<real>((*SecObj)->GetNumberField(TEXT("cz")));
                fs.Asy = static_cast<real>((*SecObj)->HasField(TEXT("Asy")) ? (*SecObj)->GetNumberField(TEXT("Asy")) : 0.0);
                fs.Asz = static_cast<real>((*SecObj)->HasField(TEXT("Asz")) ? (*SecObj)->GetNumberField(TEXT("Asz")) : 0.0);
                fs.Zy  = static_cast<real>((*SecObj)->HasField(TEXT("Zy"))  ? (*SecObj)->GetNumberField(TEXT("Zy"))  : 0.0);
                fs.Zz  = static_cast<real>((*SecObj)->HasField(TEXT("Zz"))  ? (*SecObj)->GetNumberField(TEXT("Zz"))  : 0.0);
                FString Shape;
                if ((*SecObj)->TryGetStringField(TEXT("shape"), Shape) && Shape == TEXT("circular"))
                    fs.shape = Section::Shape::Circular;
                else
                    fs.shape = Section::Shape::Rectangular;
                out.sections.push_back(fs);
            }
        }

        // nodes
        const TArray<TSharedPtr<FJsonValue>>* NodesArr = nullptr;
        if (Root->TryGetArrayField(TEXT("nodes"), NodesArr))
        {
            out.nodes.reserve(NodesArr->Num());
            for (int32 k = 0; k < NodesArr->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* NodeObj = nullptr;
                if (!(*NodesArr)[k]->TryGetObject(NodeObj))
                {
                    err = FString::Printf(TEXT("nodes[%d] is not an object"), k);
                    return false;
                }
                const NodeId id = static_cast<NodeId>((int)(*NodeObj)->GetNumberField(TEXT("id")));
                Node fn(id,
                        static_cast<real>((*NodeObj)->GetNumberField(TEXT("x"))),
                        static_cast<real>((*NodeObj)->GetNumberField(TEXT("y"))),
                        static_cast<real>((*NodeObj)->GetNumberField(TEXT("z"))));
                TArray<bool> Fixed;
                if (GetBoolArray(*NodeObj, TEXT("fixed"), 6, Fixed))
                {
                    for (int32 d = 0; d < 6; ++d) fn.fixed[d] = Fixed[d];
                }
                out.nodes.push_back(fn);
            }
        }

        // members
        const TArray<TSharedPtr<FJsonValue>>* MemsArr = nullptr;
        if (Root->TryGetArrayField(TEXT("members"), MemsArr))
        {
            out.members.reserve(MemsArr->Num());
            for (int32 k = 0; k < MemsArr->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* MemObj = nullptr;
                if (!(*MemsArr)[k]->TryGetObject(MemObj))
                {
                    err = FString::Printf(TEXT("members[%d] is not an object"), k);
                    return false;
                }
                // v3.3 audit A-1 closeout: a JSON member that omits "mat" / "sec" used to
                // default to matIdx=-1 / secIdx=-1, pass FrameModel::validate (which only
                // checks node-id existence), then get silently skipped by computeStressField --
                // the caller saw an empty stress field with no diagnostic. Require these
                // explicitly so missing-field is a loud parse error instead.
                if (!(*MemObj)->HasField(TEXT("mat")))
                {
                    err = FString::Printf(TEXT("members[%d].mat is required (no default)"), k);
                    return false;
                }
                if (!(*MemObj)->HasField(TEXT("sec")))
                {
                    err = FString::Printf(TEXT("members[%d].sec is required (no default)"), k);
                    return false;
                }
                const int32 MatIdx = static_cast<int32>((*MemObj)->GetNumberField(TEXT("mat")));
                const int32 SecIdx = static_cast<int32>((*MemObj)->GetNumberField(TEXT("sec")));
                if (MatIdx < 0 || MatIdx >= static_cast<int32>(out.materials.size()))
                {
                    err = FString::Printf(TEXT("members[%d].mat=%d out of range (materials count=%d)"),
                                          k, MatIdx, (int32)out.materials.size());
                    return false;
                }
                if (SecIdx < 0 || SecIdx >= static_cast<int32>(out.sections.size()))
                {
                    err = FString::Printf(TEXT("members[%d].sec=%d out of range (sections count=%d)"),
                                          k, SecIdx, (int32)out.sections.size());
                    return false;
                }
                Member fm(static_cast<MemberId>((int)(*MemObj)->GetNumberField(TEXT("id"))),
                          static_cast<NodeId>((int)(*MemObj)->GetNumberField(TEXT("i"))),
                          static_cast<NodeId>((int)(*MemObj)->GetNumberField(TEXT("j"))),
                          MatIdx,
                          SecIdx);
                TArray<double> RefV;
                if (GetNumberArray(*MemObj, TEXT("ref"), 3, RefV))
                {
                    fm.refVec = Vec3(static_cast<real>(RefV[0]),
                                     static_cast<real>(RefV[1]),
                                     static_cast<real>(RefV[2]));
                }
                if ((*MemObj)->HasField(TEXT("active")))
                    fm.active = (*MemObj)->GetBoolField(TEXT("active"));
                out.members.push_back(fm);
            }
        }

        // nodalLoads
        const TArray<TSharedPtr<FJsonValue>>* NLsArr = nullptr;
        if (Root->TryGetArrayField(TEXT("nodalLoads"), NLsArr))
        {
            out.nodalLoads.reserve(NLsArr->Num());
            for (int32 k = 0; k < NLsArr->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* NLObj = nullptr;
                if (!(*NLsArr)[k]->TryGetObject(NLObj))
                {
                    err = FString::Printf(TEXT("nodalLoads[%d] is not an object"), k);
                    return false;
                }
                NodalLoad nl;
                nl.node = static_cast<NodeId>((int)(*NLObj)->GetNumberField(TEXT("node")));
                TArray<double> Comp;
                if (!GetNumberArray(*NLObj, TEXT("comp"), 6, Comp))
                {
                    err = FString::Printf(TEXT("nodalLoads[%d].comp must be a 6-number array"), k);
                    return false;
                }
                for (int32 d = 0; d < 6; ++d) nl.comp[d] = static_cast<real>(Comp[d]);
                out.nodalLoads.push_back(nl);
            }
        }

        // memberUDLs
        const TArray<TSharedPtr<FJsonValue>>* UDLsArr = nullptr;
        if (Root->TryGetArrayField(TEXT("memberUDLs"), UDLsArr))
        {
            out.memberUDLs.reserve(UDLsArr->Num());
            for (int32 k = 0; k < UDLsArr->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* UObj = nullptr;
                if (!(*UDLsArr)[k]->TryGetObject(UObj))
                {
                    err = FString::Printf(TEXT("memberUDLs[%d] is not an object"), k);
                    return false;
                }
                MemberUDL u;
                u.member = static_cast<MemberId>((int)(*UObj)->GetNumberField(TEXT("member")));
                TArray<double> W;
                if (!GetNumberArray(*UObj, TEXT("w_local"), 3, W))
                {
                    err = FString::Printf(TEXT("memberUDLs[%d].w_local must be a 3-number array"), k);
                    return false;
                }
                u.w_local = Vec3(static_cast<real>(W[0]), static_cast<real>(W[1]), static_cast<real>(W[2]));
                out.memberUDLs.push_back(u);
            }
        }

        return true;
    }
}

FFrameStressField UFrameCoreStressFieldLibrary::ComputeFromJsonModel(
    const FString& JsonPath, int32 SamplesPerSpan)
{
    if (SamplesPerSpan < 2) SamplesPerSpan = 11;

    const FString ResolvedPath = FPaths::ConvertRelativePathToFull(JsonPath);
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *ResolvedPath))
    {
        UE_LOG(LogFrameCoreUEJson, Warning,
               TEXT("ComputeFromJsonModel: failed to read '%s'"), *ResolvedPath);
        return FFrameStressField{};
    }

    TSharedPtr<FJsonObject> Root;
    const auto Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogFrameCoreUEJson, Warning,
               TEXT("ComputeFromJsonModel: JSON parse failed for '%s'"), *ResolvedPath);
        return FFrameStressField{};
    }

    frame::FrameModel m;
    FString buildErr;
    if (!BuildModelFromJsonObject(Root, m, buildErr))
    {
        UE_LOG(LogFrameCoreUEJson, Warning,
               TEXT("ComputeFromJsonModel: schema error in '%s': %s"),
               *ResolvedPath, *buildErr);
        return FFrameStressField{};
    }

    std::string why;
    if (!m.validate(why))
    {
        UE_LOG(LogFrameCoreUEJson, Warning,
               TEXT("ComputeFromJsonModel: model.validate rejected '%s': %s"),
               *ResolvedPath, *FString(UTF8_TO_TCHAR(why.c_str())));
        return FFrameStressField{};
    }

    const frame::SolveResult r = frame::solve(m);
    if (r.singular)
    {
        UE_LOG(LogFrameCoreUEJson, Warning,
               TEXT("ComputeFromJsonModel: solver returned singular for '%s': %s"),
               *ResolvedPath, *FString(UTF8_TO_TCHAR(r.diagnostic.c_str())));
        return FFrameStressField{};
    }

    const frame::StressField field = frame::computeStressField(m, r, SamplesPerSpan);
    return FrameCoreUE::ToBlueprint(field);
}
