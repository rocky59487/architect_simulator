#include "FrameCoreUE/FrameCoreStressFieldActor.h"
#include "ProceduralMeshComponent.h"

namespace
{
    // 3-stop blue -> green -> red ramp. t clamped to [0, 1].
    FLinearColor SigmaRamp(float t)
    {
        t = FMath::Clamp(t, 0.f, 1.f);
        if (t < 0.5f)
        {
            const float u = t * 2.f;  // 0..1 across blue->green
            return FLinearColor(0.f, u, 1.f - u, 1.f);
        }
        const float u = (t - 0.5f) * 2.f;  // 0..1 across green->red
        return FLinearColor(u, 1.f - u, 0.f, 1.f);
    }

    // Build an orthonormal {axis, refY, refZ} frame from a member axis. refY is chosen as
    // the projection of global up onto the plane perpendicular to axis, falling back to
    // global X when the member is vertical.
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

AFrameCoreStressFieldActor::AFrameCoreStressFieldActor()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("StressFieldMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;  // small meshes; sync build keeps tests deterministic.
}

void AFrameCoreStressFieldActor::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoBuildOnBeginPlay)
    {
        BuildMesh();
    }
}

bool AFrameCoreStressFieldActor::BuildMesh()
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();

    if (Field.Members.Num() == 0 || MemberGeometry.Num() == 0)
    {
        return false;
    }

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

void AFrameCoreStressFieldActor::BuildOneMemberSection(int32 SectionIdx,
                                                      const FFrameMemberStressTrace& Trace,
                                                      const FFrameMemberGeometry& Geom)
{
    const int32 NRings = Trace.Samples.Num();           // = samplesPerSpan
    const int32 NSeg   = NRings - 1;
    const int32 NVerts = NRings * 4;
    const int32 NSideTri = NSeg * 4 * 2;                // 4 sides per segment, 2 tris each
    const int32 NCapTri  = 2 + 2;                       // 2 tris each end cap

    TArray<FVector>      Vertices;        Vertices.Reserve(NVerts);
    TArray<int32>        Indices;         Indices.Reserve((NSideTri + NCapTri) * 3);
    TArray<FVector>      Normals;         Normals.Reserve(NVerts);
    TArray<FVector2D>    UVs;             UVs.Reserve(NVerts);
    TArray<FLinearColor> VertexColors;    VertexColors.Reserve(NVerts);
    TArray<FProcMeshTangent> Tangents;    Tangents.Reserve(NVerts);

    FVector Axis, RefY, RefZ;
    const FVector AxisRaw = Geom.End - Geom.Start;
    MemberLocalAxes(AxisRaw, Axis, RefY, RefZ);

    const float halfW = Geom.Width  * 0.5f;
    const float halfD = Geom.Depth  * 0.5f;
    const float GlobalMax = FMath::Max(Field.GlobalMaxFiberSigma, KINDA_SMALL_NUMBER);

    // ring k corner ordering (consistent with the +RefY/+RefZ cross axes):
    //   0 = +RefY +RefZ, 1 = +RefY -RefZ, 2 = -RefY -RefZ, 3 = -RefY +RefZ
    auto CornerOffset = [&](int32 cornerIdx) -> FVector
    {
        switch (cornerIdx)
        {
            case 0: return  RefY * halfW + RefZ * halfD;
            case 1: return  RefY * halfW - RefZ * halfD;
            case 2: return -RefY * halfW - RefZ * halfD;
            case 3: return -RefY * halfW + RefZ * halfD;
        }
        return FVector::ZeroVector;
    };

    // Build all ring vertices first.
    for (int32 k = 0; k < NRings; ++k)
    {
        const float t = (NRings == 1) ? 0.f : (float)k / (float)(NRings - 1);
        const FVector Center = FMath::Lerp(Geom.Start, Geom.End, t);
        const float sigma = FMath::Max(Trace.Samples[k].SigmaCompMax,
                                       Trace.Samples[k].SigmaTensMax);
        const FLinearColor Color = SigmaRamp(sigma / GlobalMax);

        for (int32 c = 0; c < 4; ++c)
        {
            const FVector V = Center + CornerOffset(c);
            Vertices.Add(V);
            VertexColors.Add(Color);
            // Flat-ish per-vertex normal pointing radially outward from member axis;
            // good enough for the stress-band visual (no per-face shading needed at this thin-slice).
            const FVector Radial = (V - Center).GetSafeNormal();
            Normals.Add(Radial);
            UVs.Add(FVector2D(t, (float)c * 0.25f));
            // Tangent along the member axis is the most useful UV-stable choice.
            Tangents.Add(FProcMeshTangent(Axis, false));
        }
    }

    // Side triangles between ring k and ring k+1. For face f (0..3), the quad spans
    // ring-k.corner(f) -> ring-k.corner((f+1)%4) -> ring-(k+1).corner((f+1)%4)
    //  -> ring-(k+1).corner(f), split into 2 triangles.
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
            // Triangle 1: a, b, c (CCW when viewed from outside)
            Indices.Add(a); Indices.Add(b); Indices.Add(c);
            // Triangle 2: a, c, d
            Indices.Add(a); Indices.Add(c); Indices.Add(d);
        }
    }

    // End caps: ring 0 (facing -axis) and ring NRings-1 (facing +axis). Each cap is the
    // quad of 4 corners, 2 triangles. Winding is chosen so the outward-facing normal
    // matches the member-end direction.
    {
        // ring 0 cap -- looking at it from -axis side, CCW winding = 0,3,2 then 0,2,1
        Indices.Add(0); Indices.Add(3); Indices.Add(2);
        Indices.Add(0); Indices.Add(2); Indices.Add(1);
        // ring NRings-1 cap -- looking from +axis side, CCW = base,1,2 then base,2,3
        const int32 base = (NRings - 1) * 4;
        Indices.Add(base + 0); Indices.Add(base + 1); Indices.Add(base + 2);
        Indices.Add(base + 0); Indices.Add(base + 2); Indices.Add(base + 3);
    }

    MeshComponent->CreateMeshSection_LinearColor(
        SectionIdx,
        Vertices,
        Indices,
        Normals,
        UVs,
        VertexColors,
        Tangents,
        /*bCreateCollision=*/false);
}

TArray<FFrameMemberGeometry> AFrameCoreStressFieldActor::MakeCantileverDemoGeometry(
    float L, float Width, float Depth)
{
    TArray<FFrameMemberGeometry> Out;
    FFrameMemberGeometry G;
    G.MemberIdx = 0;
    G.Start     = FVector::ZeroVector;
    G.End       = FVector(L, 0.f, 0.f);
    G.Width     = Width;
    G.Depth     = Depth;
    Out.Add(G);
    return Out;
}
