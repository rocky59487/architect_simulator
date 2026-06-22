#include "FrameCoreUE/FrameResponseSpectrumActor.h"
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

AFrameResponseSpectrumActor::AFrameResponseSpectrumActor()
{
    PrimaryActorTick.bCanEverTick = true;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ResponseSpectrumMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameResponseSpectrumActor::BeginPlay()
{
    Super::BeginPlay();
    BuildAtPhase(0.f);
}

void AFrameResponseSpectrumActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    CurrentPhase += DeltaTime;
    BuildAtPhase(CurrentPhase);
}

FVector AFrameResponseSpectrumActor::LookupPeakDisplacement(int32 NodeIdx) const
{
    if (NodeIdx < 0 || NodeIdx >= Response.PeakDisplacements.Num()) { return FVector::ZeroVector; }
    const FFrameNodalDisplacement& D = Response.PeakDisplacements[NodeIdx];
    return FVector(D.Ux, D.Uy, D.Uz);
}

bool AFrameResponseSpectrumActor::BuildAtPhase(float PhaseSec)
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();
    if (MemberGeometry.Num() == 0) { return false; }

    // Audit C-09: SRSS / CQC mode-combined peaks are direction-LESS scalar envelopes.
    // Driving them with cos(...) makes Scal swing -1..+1 -> nodes oscillate through
    // reverse peaks that have no physical meaning. Use a 0..1 unipolar pulse envelope
    // 0.5*(1 + cos(...)) so node displacements stay in the +direction with peak ==
    // the engine response, 0 == momentarily at rest. Documented as visual-only in the
    // class doc + RELEASE Honest Boundaries.
    const float Env  = 0.5f * (1.f + FMath::Cos(2.f * PI * EnvelopeHz * PhaseSec));
    const float Scal = Amplitude * Env;
    for (int32 i = 0; i < MemberGeometry.Num(); ++i)
    {
        BuildOneMemberSection(i, MemberGeometry[i], Scal);
    }
    return true;
}

void AFrameResponseSpectrumActor::BuildOneMemberSection(int32 SectionIdx,
                                                       const FFrameMemberGeometry& Geom,
                                                       float Scale)
{
    const FVector UI = LookupPeakDisplacement(Geom.EndINodeIdx) * Scale;
    const FVector UJ = LookupPeakDisplacement(Geom.EndJNodeIdx) * Scale;
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
    const FLinearColor BaseColor(0.30f, 0.85f, 0.55f, 1.f);   // teal -> seismic motif
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
