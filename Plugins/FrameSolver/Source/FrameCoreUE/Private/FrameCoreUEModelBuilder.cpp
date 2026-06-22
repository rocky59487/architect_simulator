#include "FrameCoreUE/FrameCoreUEModelBuilder.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/SolveOptions.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <string>

DEFINE_LOG_CATEGORY_STATIC(LogFrameCoreUEModelBuilder, Log, All);

namespace FrameCoreUE
{
    // Defined in FrameCoreUEModelMarshal.cpp (same module).
    bool FromBlueprint(const FFrameModelDef& Def, frame::FrameModel& OutModel, FString& OutError);
}

bool UFrameModelBuilder::ValidateModel(const FFrameModelDef& Def, FString& OutError)
{
    frame::FrameModel m;
    if (!FrameCoreUE::FromBlueprint(Def, m, OutError))
    {
        return false;
    }
    std::string why;
    if (!m.validate(why))
    {
        OutError = FString(UTF8_TO_TCHAR(why.c_str()));
        return false;
    }
    OutError.Empty();
    return true;
}

namespace
{
    // Local helpers ----------------------------------------------------------
    bool ReadFloatArray(const TSharedPtr<FJsonObject>& Obj, const FString& Key,
                        int32 ExpectedN, TArray<float>& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
        if (!Obj->TryGetArrayField(Key, Arr) || Arr->Num() != ExpectedN) return false;
        Out.Reset(ExpectedN);
        for (int32 k = 0; k < ExpectedN; ++k)
        {
            double v = 0.0;
            if (!(*Arr)[k]->TryGetNumber(v)) return false;
            Out.Add(static_cast<float>(v));
        }
        return true;
    }

    bool ReadBoolArray(const TSharedPtr<FJsonObject>& Obj, const FString& Key,
                       int32 ExpectedN, TArray<bool>& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
        if (!Obj->TryGetArrayField(Key, Arr) || Arr->Num() != ExpectedN) return false;
        Out.Reset(ExpectedN);
        for (int32 k = 0; k < ExpectedN; ++k)
        {
            bool b = false;
            if (!(*Arr)[k]->TryGetBool(b)) return false;
            Out.Add(b);
        }
        return true;
    }

    bool BuildDef(const TSharedPtr<FJsonObject>& Root, FFrameModelDef& Out, FString& Err)
    {
        if (!Root.IsValid()) { Err = TEXT("root JSON object null"); return false; }

        // Materials
        const TArray<TSharedPtr<FJsonValue>>* Mats = nullptr;
        if (Root->TryGetArrayField(TEXT("materials"), Mats))
        {
            Out.Materials.Reserve(Mats->Num());
            for (int32 k = 0; k < Mats->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* M = nullptr;
                if (!(*Mats)[k]->TryGetObject(M))
                { Err = FString::Printf(TEXT("materials[%d] not an object"), k); return false; }
                FFrameMaterial fm;
                fm.E  = static_cast<float>((*M)->GetNumberField(TEXT("E")));
                fm.G  = static_cast<float>((*M)->GetNumberField(TEXT("G")));
                if ((*M)->HasField(TEXT("rho"))) fm.Rho = static_cast<float>((*M)->GetNumberField(TEXT("rho")));
                if ((*M)->HasField(TEXT("nu")))  fm.Nu  = static_cast<float>((*M)->GetNumberField(TEXT("nu")));
                if ((*M)->HasField(TEXT("fy")))  fm.Fy  = static_cast<float>((*M)->GetNumberField(TEXT("fy")));
                const TSharedPtr<FJsonObject>* Cap = nullptr;
                if ((*M)->TryGetObjectField(TEXT("cap"), Cap))
                {
                    const float c = static_cast<float>((*Cap)->GetNumberField(TEXT("comp")));
                    const float t = (*Cap)->HasField(TEXT("tens"))  ? static_cast<float>((*Cap)->GetNumberField(TEXT("tens")))  : c;
                    const float s = (*Cap)->HasField(TEXT("shear")) ? static_cast<float>((*Cap)->GetNumberField(TEXT("shear"))) : 0.6f * c;
                    fm.Cap.Comp  = c;
                    fm.Cap.Tens  = t;
                    fm.Cap.Shear = s;
                    fm.Cap.Bend  = FMath::Min(c, t);
                    fm.Cap.Tors  = s;
                    fm.Cap.VM    = FMath::Min(c, t);
                }
                Out.Materials.Add(fm);
            }
        }

        // Sections
        const TArray<TSharedPtr<FJsonValue>>* Secs = nullptr;
        if (Root->TryGetArrayField(TEXT("sections"), Secs))
        {
            Out.Sections.Reserve(Secs->Num());
            for (int32 k = 0; k < Secs->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* S = nullptr;
                if (!(*Secs)[k]->TryGetObject(S))
                { Err = FString::Printf(TEXT("sections[%d] not an object"), k); return false; }
                FFrameSection fs;
                fs.A   = static_cast<float>((*S)->GetNumberField(TEXT("A")));
                fs.Iy  = static_cast<float>((*S)->GetNumberField(TEXT("Iy")));
                fs.Iz  = static_cast<float>((*S)->GetNumberField(TEXT("Iz")));
                fs.J   = static_cast<float>((*S)->GetNumberField(TEXT("J")));
                fs.Cy  = static_cast<float>((*S)->GetNumberField(TEXT("cy")));
                fs.Cz  = static_cast<float>((*S)->GetNumberField(TEXT("cz")));
                if ((*S)->HasField(TEXT("Asy"))) fs.Asy = static_cast<float>((*S)->GetNumberField(TEXT("Asy")));
                if ((*S)->HasField(TEXT("Asz"))) fs.Asz = static_cast<float>((*S)->GetNumberField(TEXT("Asz")));
                if ((*S)->HasField(TEXT("Zy")))  fs.Zy  = static_cast<float>((*S)->GetNumberField(TEXT("Zy")));
                if ((*S)->HasField(TEXT("Zz")))  fs.Zz  = static_cast<float>((*S)->GetNumberField(TEXT("Zz")));
                FString Shape;
                fs.Shape = ((*S)->TryGetStringField(TEXT("shape"), Shape) && Shape == TEXT("circular"))
                           ? EFrameSectionShape::Circular
                           : EFrameSectionShape::Rectangular;
                Out.Sections.Add(fs);
            }
        }

        // Nodes
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        if (Root->TryGetArrayField(TEXT("nodes"), Nodes))
        {
            Out.Nodes.Reserve(Nodes->Num());
            for (int32 k = 0; k < Nodes->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* N = nullptr;
                if (!(*Nodes)[k]->TryGetObject(N))
                { Err = FString::Printf(TEXT("nodes[%d] not an object"), k); return false; }
                FFrameNode fn;
                fn.Id    = static_cast<int32>((*N)->GetNumberField(TEXT("id")));
                fn.Pos.X = (*N)->GetNumberField(TEXT("x"));
                fn.Pos.Y = (*N)->GetNumberField(TEXT("y"));
                fn.Pos.Z = (*N)->GetNumberField(TEXT("z"));
                ReadBoolArray(*N, TEXT("fixed"), 6, fn.Fixed);
                ReadFloatArray(*N, TEXT("prescribed"), 6, fn.Prescribed);
                Out.Nodes.Add(fn);
            }
        }

        // Members
        const TArray<TSharedPtr<FJsonValue>>* Mems = nullptr;
        if (Root->TryGetArrayField(TEXT("members"), Mems))
        {
            Out.Members.Reserve(Mems->Num());
            for (int32 k = 0; k < Mems->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* MO = nullptr;
                if (!(*Mems)[k]->TryGetObject(MO))
                { Err = FString::Printf(TEXT("members[%d] not an object"), k); return false; }
                // Mirror v3.3 audit A-1 closeout: mat/sec are required.
                if (!(*MO)->HasField(TEXT("mat")))
                { Err = FString::Printf(TEXT("members[%d].mat is required"), k); return false; }
                if (!(*MO)->HasField(TEXT("sec")))
                { Err = FString::Printf(TEXT("members[%d].sec is required"), k); return false; }
                FFrameMember fm;
                fm.Id     = static_cast<int32>((*MO)->GetNumberField(TEXT("id")));
                fm.I      = static_cast<int32>((*MO)->GetNumberField(TEXT("i")));
                fm.J      = static_cast<int32>((*MO)->GetNumberField(TEXT("j")));
                fm.MatIdx = static_cast<int32>((*MO)->GetNumberField(TEXT("mat")));
                fm.SecIdx = static_cast<int32>((*MO)->GetNumberField(TEXT("sec")));
                TArray<float> RefV;
                if (ReadFloatArray(*MO, TEXT("ref"), 3, RefV))
                {
                    fm.RefVec = FVector(RefV[0], RefV[1], RefV[2]);
                }
                if ((*MO)->HasField(TEXT("active")))
                    fm.bActive = (*MO)->GetBoolField(TEXT("active"));
                if ((*MO)->HasField(TEXT("tensionOnly")))
                    fm.bTensionOnly = (*MO)->GetBoolField(TEXT("tensionOnly"));
                ReadBoolArray(*MO, TEXT("release"), 12, fm.Release);
                Out.Members.Add(fm);
            }
        }

        // Shells
        const TArray<TSharedPtr<FJsonValue>>* Shells = nullptr;
        if (Root->TryGetArrayField(TEXT("shells"), Shells))
        {
            Out.Shells.Reserve(Shells->Num());
            for (int32 k = 0; k < Shells->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* SO = nullptr;
                if (!(*Shells)[k]->TryGetObject(SO))
                { Err = FString::Printf(TEXT("shells[%d] not an object"), k); return false; }
                FFrameShellQuad fq;
                fq.Id     = static_cast<int32>((*SO)->GetNumberField(TEXT("id")));
                fq.MatIdx = static_cast<int32>((*SO)->GetNumberField(TEXT("mat")));
                fq.T      = static_cast<float>((*SO)->GetNumberField(TEXT("t")));
                const TArray<TSharedPtr<FJsonValue>>* NArr = nullptr;
                if (!(*SO)->TryGetArrayField(TEXT("n"), NArr) || NArr->Num() != 4)
                { Err = FString::Printf(TEXT("shells[%d].n must be a 4-int array"), k); return false; }
                fq.N.Reset(4);
                for (int32 d = 0; d < 4; ++d)
                {
                    double v = 0.0;
                    if (!(*NArr)[d]->TryGetNumber(v))
                    { Err = FString::Printf(TEXT("shells[%d].n[%d] not a number"), k, d); return false; }
                    fq.N.Add(static_cast<int32>(v));
                }
                if ((*SO)->HasField(TEXT("active"))) fq.bActive = (*SO)->GetBoolField(TEXT("active"));
                Out.Shells.Add(fq);
            }
        }

        // Nodal loads
        const TArray<TSharedPtr<FJsonValue>>* NLs = nullptr;
        if (Root->TryGetArrayField(TEXT("nodalLoads"), NLs))
        {
            Out.NodalLoads.Reserve(NLs->Num());
            for (int32 k = 0; k < NLs->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* NO = nullptr;
                if (!(*NLs)[k]->TryGetObject(NO))
                { Err = FString::Printf(TEXT("nodalLoads[%d] not an object"), k); return false; }
                FFrameNodalLoad nl;
                nl.Node = static_cast<int32>((*NO)->GetNumberField(TEXT("node")));
                if (!ReadFloatArray(*NO, TEXT("comp"), 6, nl.Comp))
                { Err = FString::Printf(TEXT("nodalLoads[%d].comp must be 6-number array"), k); return false; }
                Out.NodalLoads.Add(nl);
            }
        }

        // Member UDLs
        const TArray<TSharedPtr<FJsonValue>>* UDLs = nullptr;
        if (Root->TryGetArrayField(TEXT("memberUDLs"), UDLs))
        {
            Out.MemberUDLs.Reserve(UDLs->Num());
            for (int32 k = 0; k < UDLs->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* UO = nullptr;
                if (!(*UDLs)[k]->TryGetObject(UO))
                { Err = FString::Printf(TEXT("memberUDLs[%d] not an object"), k); return false; }
                FFrameMemberUDL u;
                u.Member = static_cast<int32>((*UO)->GetNumberField(TEXT("member")));
                TArray<float> W;
                if (!ReadFloatArray(*UO, TEXT("w_local"), 3, W))
                { Err = FString::Printf(TEXT("memberUDLs[%d].w_local must be 3-number array"), k); return false; }
                u.WLocal = FVector(W[0], W[1], W[2]);
                Out.MemberUDLs.Add(u);
            }
        }

        // Shell pressures
        const TArray<TSharedPtr<FJsonValue>>* SPs = nullptr;
        if (Root->TryGetArrayField(TEXT("shellPressures"), SPs))
        {
            Out.ShellPressures.Reserve(SPs->Num());
            for (int32 k = 0; k < SPs->Num(); ++k)
            {
                const TSharedPtr<FJsonObject>* PO = nullptr;
                if (!(*SPs)[k]->TryGetObject(PO))
                { Err = FString::Printf(TEXT("shellPressures[%d] not an object"), k); return false; }
                FFrameShellPressure sp;
                sp.Shell = static_cast<int32>((*PO)->GetNumberField(TEXT("shell")));
                sp.P     = static_cast<float>((*PO)->GetNumberField(TEXT("p")));
                Out.ShellPressures.Add(sp);
            }
        }

        return true;
    }
}

FFrameModelDef UFrameModelBuilder::LoadModelFromJson(const FString& JsonPath, FString& OutError)
{
    OutError.Empty();
    const FString ResolvedPath = FPaths::ConvertRelativePathToFull(JsonPath);
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *ResolvedPath))
    {
        OutError = FString::Printf(TEXT("failed to read '%s'"), *ResolvedPath);
        UE_LOG(LogFrameCoreUEModelBuilder, Warning, TEXT("LoadModelFromJson: %s"), *OutError);
        return FFrameModelDef{};
    }

    TSharedPtr<FJsonObject> Root;
    const auto Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = FString::Printf(TEXT("JSON parse failed for '%s'"), *ResolvedPath);
        UE_LOG(LogFrameCoreUEModelBuilder, Warning, TEXT("LoadModelFromJson: %s"), *OutError);
        return FFrameModelDef{};
    }

    FFrameModelDef Def;
    if (!BuildDef(Root, Def, OutError))
    {
        UE_LOG(LogFrameCoreUEModelBuilder, Warning,
               TEXT("LoadModelFromJson: schema error in '%s': %s"),
               *ResolvedPath, *OutError);
        return FFrameModelDef{};
    }
    return Def;
}
