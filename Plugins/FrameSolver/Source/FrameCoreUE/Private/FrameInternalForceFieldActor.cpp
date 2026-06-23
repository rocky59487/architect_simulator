#include "FrameCoreUE/FrameInternalForceFieldActor.h"
#include "FramePMCHelpers.h"
#include "ProceduralMeshComponent.h"

namespace
{
    // Signed ramp: blue (negative) -> white (zero) -> red (positive). t in [-1, 1].
    FLinearColor SignedRamp(float t)
    {
        const float ta = FMath::Clamp(FMath::Abs(t), 0.f, 1.f);
        if (t >= 0.f) { return FLinearColor(1.f, 1.f - ta, 1.f - ta, 1.f); }
        return FLinearColor(1.f - ta, 1.f - ta, 1.f, 1.f);
    }
}

float AFrameInternalForceFieldActor::ExtractComponent(const FFrameStressFieldSample& S,
                                                     EFrameForceComponent Comp)
{
    switch (Comp)
    {
        case EFrameForceComponent::AxialN:   return S.N;
        case EFrameForceComponent::ShearVy:  return S.Vy;
        case EFrameForceComponent::ShearVz:  return S.Vz;
        case EFrameForceComponent::TorsionT: return S.T;
        case EFrameForceComponent::MomentMy: return S.My;
        case EFrameForceComponent::MomentMz: return S.Mz;
    }
    return 0.f;
}

AFrameInternalForceFieldActor::AFrameInternalForceFieldActor()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ForceFieldMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameInternalForceFieldActor::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoBuildOnBeginPlay) { BuildMesh(); }
}

bool AFrameInternalForceFieldActor::BuildMesh()
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

void AFrameInternalForceFieldActor::BuildOneMemberSection(int32 SectionIdx,
                                                         const FFrameMemberStressTrace& Trace,
                                                         const FFrameMemberGeometry& Geom)
{
    const int32 N = Trace.Samples.Num();
    FVector Axis, RefY, RefZ;
    FrameCorePMC::MemberLocalAxes(Geom.End - Geom.Start, Axis, RefY, RefZ);

    // Pick extrusion direction by component family. Moments project along the
    // axis perpendicular to the moment's rotation axis: Mz uses +RefY, My uses +RefZ.
    // Forces project on RefY/RefZ likewise.
    FVector ExtrudeDir = RefY;
    switch (Component)
    {
        case EFrameForceComponent::ShearVz:
        case EFrameForceComponent::MomentMy:
            ExtrudeDir = RefZ; break;
        default: break;
    }

    // Find max abs to normalise the colour ramp; ribbon height stays in mm.
    float MaxAbs = 0.f;
    for (int32 k = 0; k < N; ++k)
    {
        MaxAbs = FMath::Max(MaxAbs, FMath::Abs(ExtractComponent(Trace.Samples[k], Component)));
    }
    if (MaxAbs < KINDA_SMALL_NUMBER) { MaxAbs = 1.f; }

    TArray<FVector>          Vertices; Vertices.Reserve(N * 2);
    TArray<int32>            Indices;  Indices.Reserve((N - 1) * 6);
    TArray<FVector>          Normals;  Normals.Reserve(N * 2);
    TArray<FVector2D>        UVs;      UVs.Reserve(N * 2);
    TArray<FLinearColor>     VColors;  VColors.Reserve(N * 2);
    TArray<FProcMeshTangent> Tangents; Tangents.Reserve(N * 2);

    for (int32 k = 0; k < N; ++k)
    {
        const float t      = (N == 1) ? 0.f : (float)k / (float)(N - 1);
        const FVector PathPos = FMath::Lerp(Geom.Start, Geom.End, t);
        const float Val    = ExtractComponent(Trace.Samples[k], Component);
        const float Norm   = Val / MaxAbs;                        // [-1, 1]
        const float UseVal = bDualSidedSigned ? Val : FMath::Abs(Val);
        const FVector Tip  = PathPos + ExtrudeDir * (UseVal * HeightScale);
        Vertices.Add(PathPos);
        Vertices.Add(Tip);
        const FLinearColor Col = SignedRamp(Norm);
        VColors.Add(Col); VColors.Add(Col);
        const FVector RibbonNormal = (UseVal >= 0.f) ? ExtrudeDir : -ExtrudeDir;
        Normals.Add(RibbonNormal); Normals.Add(RibbonNormal);
        UVs.Add(FVector2D((float)k / (float)(N - 1), 0.f));
        UVs.Add(FVector2D((float)k / (float)(N - 1), 1.f));
        Tangents.Add(FProcMeshTangent(Axis, false));
        Tangents.Add(FProcMeshTangent(Axis, false));
    }
    for (int32 k = 0; k + 1 < N; ++k)
    {
        const int32 b0 = k * 2;
        const int32 b1 = (k + 1) * 2;
        Indices.Add(b0);     Indices.Add(b1);     Indices.Add(b0 + 1);
        Indices.Add(b0 + 1); Indices.Add(b1);     Indices.Add(b1 + 1);
    }
    MeshComponent->CreateMeshSection_LinearColor(
        SectionIdx, Vertices, Indices, Normals, UVs, VColors, Tangents,
        /*bCreateCollision=*/false);
}
