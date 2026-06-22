#include "FrameCoreUE/FrameModalShapeActor.h"
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

AFrameModalShapeActor::AFrameModalShapeActor()
{
    PrimaryActorTick.bCanEverTick = true;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ModalShapeMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameModalShapeActor::BeginPlay()
{
    Super::BeginPlay();
    BuildAtPhase(0.f);
}

void AFrameModalShapeActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    CurrentPhase += DeltaTime * TimeScale;
    BuildAtPhase(CurrentPhase);
}

FVector AFrameModalShapeActor::LookupShapeDisplacement(int32 NodeIdx) const
{
    if (Modes.Modes.Num() == 0 || ModeIndex < 0 || ModeIndex >= Modes.Modes.Num())
    {
        return FVector::ZeroVector;
    }
    const FFrameModeShape& MS = Modes.Modes[ModeIndex];
    if (NodeIdx < 0 || NodeIdx >= MS.Shape.Num()) { return FVector::ZeroVector; }
    const FFrameNodalDisplacement& D = MS.Shape[NodeIdx];
    return FVector(D.Ux, D.Uy, D.Uz);
}

bool AFrameModalShapeActor::BuildAtPhase(float PhaseSec)
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();
    if (MemberGeometry.Num() == 0) { return false; }
    if (Modes.Modes.Num() == 0 || ModeIndex < 0 || ModeIndex >= Modes.Modes.Num())
    {
        // No mode -> just emit undeformed.
        for (int32 i = 0; i < MemberGeometry.Num(); ++i)
        {
            BuildOneMemberSection(i, MemberGeometry[i], 0.f);
        }
        return MemberGeometry.Num() > 0;
    }

    const FFrameModeShape& MS = Modes.Modes[ModeIndex];
    const float OmegaT = 2.f * PI * MS.FreqHz * PhaseSec;
    const float DispScale = Amplitude * FMath::Cos(OmegaT);

    for (int32 i = 0; i < MemberGeometry.Num(); ++i)
    {
        BuildOneMemberSection(i, MemberGeometry[i], DispScale);
    }
    return true;
}

void AFrameModalShapeActor::BuildOneMemberSection(int32 SectionIdx,
                                                  const FFrameMemberGeometry& Geom,
                                                  float DispScale)
{
    const FVector UI = LookupShapeDisplacement(Geom.EndINodeIdx) * DispScale;
    const FVector UJ = LookupShapeDisplacement(Geom.EndJNodeIdx) * DispScale;
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
    const FLinearColor BaseColor(0.55f, 0.30f, 0.85f, 1.f);   // violet -> modal motif
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
