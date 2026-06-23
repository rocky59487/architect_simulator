#include "FrameCoreUE/FrameDeformedShapeActor.h"
#include "FramePMCHelpers.h"
#include "ProceduralMeshComponent.h"

AFrameDeformedShapeActor::AFrameDeformedShapeActor()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DeformedShapeMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameDeformedShapeActor::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoBuildOnBeginPlay)
    {
        BuildMesh();
    }
}

FVector AFrameDeformedShapeActor::GetNodalDisplacement(int32 NodeIdx) const
{
    if (NodeIdx < 0 || NodeIdx >= Solution.Displacements.Num()) { return FVector::ZeroVector; }
    const FFrameNodalDisplacement& D = Solution.Displacements[NodeIdx];
    return FVector(D.Ux, D.Uy, D.Uz);
}

bool AFrameDeformedShapeActor::BuildMesh()
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();

    if (MemberGeometry.Num() == 0) { return false; }

    int32 SectionsBuilt = 0;
    for (const FFrameMemberGeometry& Geom : MemberGeometry)
    {
        BuildOneMemberSection(SectionsBuilt, Geom);
        ++SectionsBuilt;
    }
    return SectionsBuilt > 0;
}

void AFrameDeformedShapeActor::BuildOneMemberSection(int32 SectionIdx,
                                                    const FFrameMemberGeometry& Geom)
{
    // Lift per-end displacement, multiply by visual scale.
    const FVector UI = GetNodalDisplacement(Geom.EndINodeIdx) * DeflectionScale;
    const FVector UJ = GetNodalDisplacement(Geom.EndJNodeIdx) * DeflectionScale;

    const FVector StartD = Geom.Start + UI;
    const FVector EndD   = Geom.End   + UJ;

    // v3.5.1: NRings + MemberLocalAxes + CornerOffset live in FramePMCHelpers.h.
    constexpr int32 NRings = FrameCorePMC::kRings;
    constexpr int32 NSeg   = NRings - 1;

    TArray<FVector>          Vertices;     Vertices.Reserve(NRings * 4);
    TArray<int32>            Indices;      Indices.Reserve(NSeg * 4 * 2 * 3 + 12);
    TArray<FVector>          Normals;      Normals.Reserve(NRings * 4);
    TArray<FVector2D>        UVs;          UVs.Reserve(NRings * 4);
    TArray<FLinearColor>     VColors;      VColors.Reserve(NRings * 4);
    TArray<FProcMeshTangent> Tangents;     Tangents.Reserve(NRings * 4);

    FVector Axis, RefY, RefZ;
    FrameCorePMC::MemberLocalAxes(EndD - StartD, Axis, RefY, RefZ);
    const float halfW = Geom.Width  * 0.5f;
    const float halfD = Geom.Depth  * 0.5f;

    auto Corner = [&](int32 c) -> FVector
    {
        return FrameCorePMC::CornerOffset(c, RefY, RefZ, halfW, halfD);
    };

    // Linear interpolation per ring (cubic Hermite needs per-end rotation vectors and is
    // deferred to v3.6 -- see docs/HANDOFF_v3.5.0.md U-11; current straight-lerp
    // regardless of EndINodeIdx/EndJNodeIdx is the v3.5 contract).
    // Audit F-09: hoist BaseColor out of the ring loop (it's invariant).
    const FLinearColor BaseColor(0.30f, 0.55f, 0.85f, 1.f);   // steel-blue motif
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
    // End caps, winding same as v3.3 stress-field actor.
    Indices.Add(0); Indices.Add(3); Indices.Add(2);
    Indices.Add(0); Indices.Add(2); Indices.Add(1);
    const int32 baseEnd = (NRings - 1) * 4;
    Indices.Add(baseEnd + 0); Indices.Add(baseEnd + 1); Indices.Add(baseEnd + 2);
    Indices.Add(baseEnd + 0); Indices.Add(baseEnd + 2); Indices.Add(baseEnd + 3);

    MeshComponent->CreateMeshSection_LinearColor(
        SectionIdx, Vertices, Indices, Normals, UVs, VColors, Tangents,
        /*bCreateCollision=*/false);
}
