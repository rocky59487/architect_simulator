#include "FrameCoreUE/FrameUtilizationHeatmapActor.h"
#include "FramePMCHelpers.h"
#include "ProceduralMeshComponent.h"

namespace
{
    // blue (0,0,1) -> green (0,1,0) -> yellow (1,1,0) -> red (1,0,0)
    FLinearColor DCRamp(float t)
    {
        t = FMath::Clamp(t, 0.f, 1.f);
        if (t < 1.f / 3.f)
        {
            const float u = t * 3.f;
            return FLinearColor(0.f, u, 1.f - u, 1.f);
        }
        if (t < 2.f / 3.f)
        {
            const float u = (t - 1.f / 3.f) * 3.f;
            return FLinearColor(u, 1.f, 0.f, 1.f);
        }
        const float u = (t - 2.f / 3.f) * 3.f;
        return FLinearColor(1.f, 1.f - u, 0.f, 1.f);
    }
}

AFrameUtilizationHeatmapActor::AFrameUtilizationHeatmapActor()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HeatmapMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameUtilizationHeatmapActor::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoBuildOnBeginPlay)
    {
        BuildHeatmap();
    }
}

bool AFrameUtilizationHeatmapActor::BuildHeatmap()
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();

    // Audit F-04/F-05: O(M+N) precompute of per-MemberIdx / per-ShellIdx peak risk
    // replaces the previous O(M*N) inner linear scan inside BuildOneMemberSection.
    // Hoisted SatGuard (audit F-12) avoids recomputing FMath::Max once per section.
    const float SatGuard = FMath::Max(SaturationDC, 1e-6f);
    TMap<int32, float> MemberRisk;
    MemberRisk.Reserve(Solution.MemberUtilization.Num());
    for (const FFrameMemberUtilization& U : Solution.MemberUtilization)
    {
        MemberRisk.Add(U.MemberIdx, FMath::Max(U.Peak.Risk, 0.f));
    }
    TMap<int32, float> ShellRisk;
    ShellRisk.Reserve(Solution.ShellUtilization.Num());
    for (const FFrameShellUtilization& U : Solution.ShellUtilization)
    {
        float& W = ShellRisk.FindOrAdd(U.ShellIdx);
        W = FMath::Max(W, U.Risk);
    }

    int32 SectionsBuilt = 0;
    for (const FFrameMemberGeometry& G : MemberGeometry)
    {
        const float* Found = MemberRisk.Find(G.MemberIdx);
        const float R = Found ? *Found : 0.f;
        BuildOneMemberSection(SectionsBuilt, G, SatGuard, R);
        ++SectionsBuilt;
    }
    for (const FFrameShellGeometry& G : ShellGeometry)
    {
        const float* Found = ShellRisk.Find(G.ShellIdx);
        const float R = Found ? *Found : 0.f;
        BuildOneShellSection(SectionsBuilt, G, SatGuard, R);
        ++SectionsBuilt;
    }
    return SectionsBuilt > 0;
}

void AFrameUtilizationHeatmapActor::BuildOneMemberSection(int32 SectionIdx,
                                                         const FFrameMemberGeometry& Geom,
                                                         float SatGuard,
                                                         float RiskValue)
{
    const FLinearColor MemberColor = DCRamp(RiskValue / SatGuard);

    constexpr int32 NRings = FrameCorePMC::kRings;
    constexpr int32 NSeg   = NRings - 1;

    TArray<FVector>          Vertices;     Vertices.Reserve(NRings * 4);
    TArray<int32>            Indices;      Indices.Reserve(NSeg * 4 * 2 * 3 + 12);
    TArray<FVector>          Normals;      Normals.Reserve(NRings * 4);
    TArray<FVector2D>        UVs;          UVs.Reserve(NRings * 4);
    TArray<FLinearColor>     VColors;      VColors.Reserve(NRings * 4);
    TArray<FProcMeshTangent> Tangents;     Tangents.Reserve(NRings * 4);

    FVector Axis, RefY, RefZ;
    FrameCorePMC::MemberLocalAxes(Geom.End - Geom.Start, Axis, RefY, RefZ);
    const float halfW = Geom.Width  * 0.5f;
    const float halfD = Geom.Depth  * 0.5f;

    auto Corner = [&](int32 c) -> FVector
    {
        return FrameCorePMC::CornerOffset(c, RefY, RefZ, halfW, halfD);
    };

    for (int32 k = 0; k < NRings; ++k)
    {
        const float t = (NRings == 1) ? 0.f : (float)k / (float)(NRings - 1);
        const FVector Center = FMath::Lerp(Geom.Start, Geom.End, t);
        for (int32 c = 0; c < 4; ++c)
        {
            const FVector V = Center + Corner(c);
            Vertices.Add(V);
            VColors.Add(MemberColor);
            const FVector Radial = (V - Center).GetSafeNormal();
            Normals.Add(Radial);
            UVs.Add(FVector2D(t, (float)c * 0.25f));
            Tangents.Add(FProcMeshTangent(Axis, false));
        }
    }

    for (int32 k = 0; k < NSeg; ++k)
    {
        const int32 base0 = k       * 4;
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

void AFrameUtilizationHeatmapActor::BuildOneShellSection(int32 SectionIdx,
                                                        const FFrameShellGeometry& Geom,
                                                        float SatGuard,
                                                        float RiskValue)
{
    if (Geom.Corners.Num() != 4) { return; }   // length-4 required

    const FLinearColor ShellColor = DCRamp(RiskValue / SatGuard);

    TArray<FVector>          Vertices;     Vertices.Reserve(4);
    TArray<int32>            Indices;      Indices.Reserve(6);
    TArray<FVector>          Normals;      Normals.Reserve(4);
    TArray<FVector2D>        UVs;          UVs.Reserve(4);
    TArray<FLinearColor>     VColors;      VColors.Reserve(4);
    TArray<FProcMeshTangent> Tangents;     Tangents.Reserve(4);

    // Outward normal = (P1-P0) x (P3-P0)
    const FVector Normal = FVector::CrossProduct(Geom.Corners[1] - Geom.Corners[0],
                                                  Geom.Corners[3] - Geom.Corners[0]).GetSafeNormal();
    const FVector Tan    = (Geom.Corners[1] - Geom.Corners[0]).GetSafeNormal();
    for (int32 c = 0; c < 4; ++c)
    {
        Vertices.Add(Geom.Corners[c]);
        VColors.Add(ShellColor);
        Normals.Add(Normal);
        UVs.Add(FVector2D((c == 1 || c == 2) ? 1.f : 0.f,
                          (c == 2 || c == 3) ? 1.f : 0.f));
        Tangents.Add(FProcMeshTangent(Tan, false));
    }
    Indices.Add(0); Indices.Add(1); Indices.Add(2);
    Indices.Add(0); Indices.Add(2); Indices.Add(3);

    MeshComponent->CreateMeshSection_LinearColor(
        SectionIdx, Vertices, Indices, Normals, UVs, VColors, Tangents,
        /*bCreateCollision=*/false);
}
