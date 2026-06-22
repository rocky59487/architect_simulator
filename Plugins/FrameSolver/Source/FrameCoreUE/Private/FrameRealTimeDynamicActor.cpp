#include "FrameCoreUE/FrameRealTimeDynamicActor.h"
#include "ProceduralMeshComponent.h"

namespace
{
    void MemberLocalAxes(const FVector& InAxis, FVector& OutAxis, FVector& OutRefY, FVector& OutRefZ)
    {
        OutAxis = InAxis.GetSafeNormal();
        if (OutAxis.IsNearlyZero())
        {
            OutAxis  = FVector::ForwardVector;
            OutRefY  = FVector::RightVector;
            OutRefZ  = FVector::UpVector;
            return;
        }
        const FVector GlobalUp = FVector::UpVector;
        const float dotUp = FMath::Abs(FVector::DotProduct(OutAxis, GlobalUp));
        const FVector RefSeed = (dotUp > 0.95f) ? FVector::ForwardVector : GlobalUp;
        OutRefZ = FVector::CrossProduct(OutAxis, RefSeed).GetSafeNormal();
        OutRefY = FVector::CrossProduct(OutRefZ, OutAxis).GetSafeNormal();
    }
}

AFrameRealTimeDynamicActor::AFrameRealTimeDynamicActor()
{
    PrimaryActorTick.bCanEverTick = true;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RealTimeDynamicMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameRealTimeDynamicActor::BeginPlay()
{
    Super::BeginPlay();
    CurrentTime = 0.f;
    RebuildAtCurrentTime();
}

void AFrameRealTimeDynamicActor::SetPlaybackTime(float NewTime)
{
    CurrentTime = FMath::Max(NewTime, 0.f);
    RebuildAtCurrentTime();
}

void AFrameRealTimeDynamicActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bPlaying || History.Steps.Num() == 0) { return; }
    const float Total = History.Steps.Last().Time;
    CurrentTime += DeltaTime * PlaybackSpeed;
    if (CurrentTime > Total)
    {
        if (bLoop) { CurrentTime = FMath::Fmod(CurrentTime, FMath::Max(Total, KINDA_SMALL_NUMBER)); }
        else       { CurrentTime = Total; bPlaying = false; }
    }
    RebuildAtCurrentTime();
}

bool AFrameRealTimeDynamicActor::RebuildAtCurrentTime()
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();
    if (History.Steps.Num() == 0 || MemberGeometry.Num() == 0) { return false; }

    int32 k = 0;
    while (k + 1 < History.Steps.Num() && History.Steps[k + 1].Time < CurrentTime) { ++k; }
    const FFrameModalTimeStep& S0 = History.Steps[k];
    const FFrameModalTimeStep& S1 = (k + 1 < History.Steps.Num()) ? History.Steps[k + 1] : S0;
    const float DT = S1.Time - S0.Time;
    const float Alpha = (DT <= KINDA_SMALL_NUMBER) ? 0.f
                       : FMath::Clamp((CurrentTime - S0.Time) / DT, 0.f, 1.f);

    // Lerp per-node displacement vectors.
    TArray<FFrameNodalDisplacement> Lerped;
    const int32 N = FMath::Min(S0.Displacements.Num(), S1.Displacements.Num());
    Lerped.Reserve(N);
    for (int32 i = 0; i < N; ++i)
    {
        FFrameNodalDisplacement D;
        D.NodeIndex = S0.Displacements[i].NodeIndex;
        D.NodeId    = S0.Displacements[i].NodeId;
        D.Ux = FMath::Lerp(S0.Displacements[i].Ux, S1.Displacements[i].Ux, Alpha);
        D.Uy = FMath::Lerp(S0.Displacements[i].Uy, S1.Displacements[i].Uy, Alpha);
        D.Uz = FMath::Lerp(S0.Displacements[i].Uz, S1.Displacements[i].Uz, Alpha);
        D.Rx = FMath::Lerp(S0.Displacements[i].Rx, S1.Displacements[i].Rx, Alpha);
        D.Ry = FMath::Lerp(S0.Displacements[i].Ry, S1.Displacements[i].Ry, Alpha);
        D.Rz = FMath::Lerp(S0.Displacements[i].Rz, S1.Displacements[i].Rz, Alpha);
        Lerped.Add(D);
    }
    int32 SectionsBuilt = 0;
    for (const FFrameMemberGeometry& G : MemberGeometry)
    {
        BuildOneMemberSection(SectionsBuilt, G, Lerped);
        ++SectionsBuilt;
    }
    return SectionsBuilt > 0;
}

void AFrameRealTimeDynamicActor::BuildOneMemberSection(int32 SectionIdx,
                                                      const FFrameMemberGeometry& Geom,
                                                      const TArray<FFrameNodalDisplacement>& Lerped)
{
    auto Get = [&](int32 NodeIdx) -> FVector
    {
        if (NodeIdx < 0 || NodeIdx >= Lerped.Num()) { return FVector::ZeroVector; }
        const FFrameNodalDisplacement& D = Lerped[NodeIdx];
        return FVector(D.Ux, D.Uy, D.Uz);
    };
    const FVector UI = Get(Geom.EndINodeIdx) * DeflectionScale;
    const FVector UJ = Get(Geom.EndJNodeIdx) * DeflectionScale;
    const FVector StartD = Geom.Start + UI;
    const FVector EndD   = Geom.End   + UJ;

    const int32 NRings = 11;
    const int32 NSeg   = NRings - 1;

    TArray<FVector>          Vertices;     Vertices.Reserve(NRings * 4);
    TArray<int32>            Indices;      Indices.Reserve(NSeg * 4 * 2 * 3 + 12);
    TArray<FVector>          Normals;      Normals.Reserve(NRings * 4);
    TArray<FVector2D>        UVs;          UVs.Reserve(NRings * 4);
    TArray<FLinearColor>     VColors;      VColors.Reserve(NRings * 4);
    TArray<FProcMeshTangent> Tangents;     Tangents.Reserve(NRings * 4);

    FVector Axis, RefY, RefZ;
    MemberLocalAxes(EndD - StartD, Axis, RefY, RefZ);
    const float halfW = Geom.Width  * 0.5f;
    const float halfD = Geom.Depth  * 0.5f;
    auto Corner = [&](int32 c) -> FVector
    {
        switch (c)
        {
            case 0: return  RefY * halfW + RefZ * halfD;
            case 1: return  RefY * halfW - RefZ * halfD;
            case 2: return -RefY * halfW - RefZ * halfD;
            case 3: return -RefY * halfW + RefZ * halfD;
        }
        return FVector::ZeroVector;
    };
    const FLinearColor BaseColor(0.85f, 0.85f, 0.30f, 1.f);    // amber -> dynamic motif
    for (int32 k = 0; k < NRings; ++k)
    {
        const float t = (NRings == 1) ? 0.f : (float)k / (float)(NRings - 1);
        const FVector Center = FMath::Lerp(StartD, EndD, t);
        for (int32 c = 0; c < 4; ++c)
        {
            const FVector V = Center + Corner(c);
            Vertices.Add(V);
            VColors.Add(BaseColor);
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
