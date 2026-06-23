#include "FrameCoreUE/FrameUtilizationFieldActor.h"
#include "FramePMCHelpers.h"
#include "ProceduralMeshComponent.h"

namespace
{
    FLinearColor DCRamp(float t)
    {
        t = FMath::Clamp(t, 0.f, 1.f);
        if (t < 1.f / 3.f)        { const float u = t * 3.f;                  return FLinearColor(0.f, u, 1.f - u, 1.f); }
        if (t < 2.f / 3.f)        { const float u = (t - 1.f / 3.f) * 3.f;    return FLinearColor(u, 1.f, 0.f, 1.f); }
        const float u = (t - 2.f / 3.f) * 3.f;
        return FLinearColor(1.f, 1.f - u, 0.f, 1.f);
    }
}

float AFrameUtilizationFieldActor::SampleDC(const FFrameStressFieldSample& S,
                                            const FFrameCapacity& Cap) const
{
    // v3.6 audit (C-2 MEDIUM): mirror engine ElasticAllowable::checkSection which takes
    // max over 4 ratios — compression, tension, shear, and torsion. Pre-fix v3.6 only
    // computed three and silently under-reported D/C in torsion-dominated cases.
    const float Comp = (Cap.Comp  > KINDA_SMALL_NUMBER) ? FMath::Abs(S.SigmaCompMax) / Cap.Comp  : 0.f;
    const float Tens = (Cap.Tens  > KINDA_SMALL_NUMBER) ? FMath::Abs(S.SigmaTensMax) / Cap.Tens  : 0.f;
    const float Shear= (Cap.Shear > KINDA_SMALL_NUMBER) ? FMath::Abs(S.TauShear)     / Cap.Shear : 0.f;
    const float Tors = (Cap.Tors  > KINDA_SMALL_NUMBER) ? FMath::Abs(S.TauTorsion)   / Cap.Tors  : 0.f;
    return FMath::Max(FMath::Max3(Comp, Tens, Shear), Tors);
}

AFrameUtilizationFieldActor::AFrameUtilizationFieldActor()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("UtilFieldMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameUtilizationFieldActor::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoBuildOnBeginPlay) { BuildMesh(); }
}

bool AFrameUtilizationFieldActor::BuildMesh()
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();
    if (Field.Members.Num() == 0 || MemberGeometry.Num() == 0) { return false; }

    int32 SectionsBuilt = 0;
    for (const FFrameMemberGeometry& Geom : MemberGeometry)
    {
        if (Geom.MemberIdx < 0 || Geom.MemberIdx >= Field.Members.Num()) { continue; }
        const FFrameMemberStressTrace& Trace = Field.Members[Geom.MemberIdx];
        if (Trace.Samples.Num() < 2) { continue; }
        BuildOneMemberSection(SectionsBuilt, Trace, Geom);
        ++SectionsBuilt;
    }
    return SectionsBuilt > 0;
}

void AFrameUtilizationFieldActor::BuildOneMemberSection(int32 SectionIdx,
                                                       const FFrameMemberStressTrace& Trace,
                                                       const FFrameMemberGeometry& Geom)
{
    const int32 NRings = Trace.Samples.Num();
    const int32 NSeg   = NRings - 1;
    const float SatGuard = FMath::Max(SaturationDC, 1e-6f);
    const FFrameCapacity Cap = Capacities.IsValidIndex(Geom.MemberIdx)
        ? Capacities[Geom.MemberIdx]
        : FFrameCapacity{};

    TArray<FVector>          Vertices;     Vertices.Reserve(NRings * 4);
    TArray<int32>            Indices;      Indices.Reserve(NSeg * 4 * 2 * 3 + 12);
    TArray<FVector>          Normals;      Normals.Reserve(NRings * 4);
    TArray<FVector2D>        UVs;          UVs.Reserve(NRings * 4);
    TArray<FLinearColor>     VColors;      VColors.Reserve(NRings * 4);
    TArray<FProcMeshTangent> Tangents;     Tangents.Reserve(NRings * 4);

    FVector Axis, RefY, RefZ;
    FrameCorePMC::MemberLocalAxes(Geom.End - Geom.Start, Axis, RefY, RefZ);
    const float halfW = Geom.Width * 0.5f;
    const float halfD = Geom.Depth * 0.5f;
    auto Corner = [&](int32 c) { return FrameCorePMC::CornerOffset(c, RefY, RefZ, halfW, halfD); };

    for (int32 k = 0; k < NRings; ++k)
    {
        const float t = (NRings == 1) ? 0.f : (float)k / (float)(NRings - 1);
        const FVector Center = FMath::Lerp(Geom.Start, Geom.End, t);
        const float DC = SampleDC(Trace.Samples[k], Cap);
        // Exceedance filter: paint only when DC > 1.0; below cap show as transparent blue.
        const bool bPaint = !bShowExceedanceOnly || (DC > 1.0f);
        const FLinearColor Col = bPaint
            ? DCRamp(DC / SatGuard)
            : FLinearColor(0.f, 0.f, 1.f, 0.2f);
        for (int32 c = 0; c < 4; ++c)
        {
            const FVector V = Center + Corner(c);
            Vertices.Add(V);
            VColors.Add(Col);
            const FVector Radial = (V - Center).GetSafeNormal();
            Normals.Add(Radial);
            UVs.Add(FVector2D(t, (float)c * 0.25f));
            Tangents.Add(FProcMeshTangent(Axis, false));
        }
    }
    for (int32 k = 0; k < NSeg; ++k)
    {
        const int32 base0 = k * 4;
        const int32 base1 = (k + 1) * 4;
        for (int32 f = 0; f < 4; ++f)
        {
            const int32 a = base0 + f;
            const int32 b = base0 + ((f + 1) % 4);
            const int32 c = base1 + ((f + 1) % 4);
            const int32 d = base1 + f;
            Indices.Add(a); Indices.Add(b); Indices.Add(c);
            Indices.Add(a); Indices.Add(c); Indices.Add(d);
        }
    }
    Indices.Add(0); Indices.Add(3); Indices.Add(2);
    Indices.Add(0); Indices.Add(2); Indices.Add(1);
    const int32 baseEnd = (NRings - 1) * 4;
    Indices.Add(baseEnd + 0); Indices.Add(baseEnd + 1); Indices.Add(baseEnd + 2);
    Indices.Add(baseEnd + 0); Indices.Add(baseEnd + 2); Indices.Add(baseEnd + 3);

    MeshComponent->CreateMeshSection_LinearColor(
        SectionIdx, Vertices, Indices, Normals, UVs, VColors, Tangents,
        /*bCreateCollision=*/false);
}
