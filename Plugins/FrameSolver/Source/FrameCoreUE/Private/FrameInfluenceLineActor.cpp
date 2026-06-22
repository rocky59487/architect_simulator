#include "FrameCoreUE/FrameInfluenceLineActor.h"
#include "ProceduralMeshComponent.h"

namespace
{
    // signed influence -> blue (neg) / white (zero) / red (pos), saturating to value 1.
    FLinearColor SignedRamp(float t)
    {
        const float ta = FMath::Clamp(FMath::Abs(t), 0.f, 1.f);
        if (t >= 0.f) { return FLinearColor(1.f, 1.f - ta, 1.f - ta, 1.f); }   // white -> red
        return FLinearColor(1.f - ta, 1.f - ta, 1.f, 1.f);                     // white -> blue
    }
}

AFrameInfluenceLineActor::AFrameInfluenceLineActor()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("InfluenceLineMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameInfluenceLineActor::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoBuildOnBeginPlay) { BuildMesh(); }
}

bool AFrameInfluenceLineActor::BuildMesh()
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();

    const int32 N = FMath::Min(Line.ReactionAtPosition.Num(), PathGeometry.Num());
    if (N < 2) { return false; }

    // Normalise by max abs value so the ramp paints in [-1, 1].
    float MaxAbs = 0.f;
    for (int32 i = 0; i < N; ++i)
    {
        MaxAbs = FMath::Max(MaxAbs, FMath::Abs(Line.ReactionAtPosition[i]));
    }
    if (MaxAbs < KINDA_SMALL_NUMBER) { MaxAbs = 1.f; }

    // Build a ribbon: 2 verts per path step (one at path-position, one offset +Z by
    // ReactionAtPosition[k] * HeightScale).
    TArray<FVector>          Vertices; Vertices.Reserve(N * 2);
    TArray<int32>            Indices;  Indices.Reserve((N - 1) * 6);
    TArray<FVector>          Normals;  Normals.Reserve(N * 2);
    TArray<FVector2D>        UVs;      UVs.Reserve(N * 2);
    TArray<FLinearColor>     VColors;  VColors.Reserve(N * 2);
    TArray<FProcMeshTangent> Tangents; Tangents.Reserve(N * 2);

    for (int32 k = 0; k < N; ++k)
    {
        const float Inf = Line.ReactionAtPosition[k];
        const float T   = Inf / MaxAbs;
        const FLinearColor Col = SignedRamp(T);
        const FVector PathPos = PathGeometry[k].Start;
        const float Height = Inf * HeightScale;

        Vertices.Add(PathPos);                          // bottom strip
        Vertices.Add(PathPos + FVector(0.f, 0.f, Height));   // top strip

        VColors.Add(Col); VColors.Add(Col);
        // v3.5 audit C-05 fix: ribbon normal must flip with sign(Height) so negative-
        // influence segments are not pure black under a directional light (NdotL < 0
        // when the +Z normal faces a face actually pointing -Z).
        const FVector RibbonNormal = (Height >= 0.f) ? FVector::UpVector : FVector::DownVector;
        Normals.Add(RibbonNormal); Normals.Add(RibbonNormal);
        UVs.Add(FVector2D((float)k / (float)(N - 1), 0.f));
        UVs.Add(FVector2D((float)k / (float)(N - 1), 1.f));
        Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
        Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
    }
    for (int32 k = 0; k + 1 < N; ++k)
    {
        const int32 b0 = k * 2;
        const int32 b1 = (k + 1) * 2;
        Indices.Add(b0);     Indices.Add(b1);     Indices.Add(b0 + 1);
        Indices.Add(b0 + 1); Indices.Add(b1);     Indices.Add(b1 + 1);
    }
    MeshComponent->CreateMeshSection_LinearColor(
        0, Vertices, Indices, Normals, UVs, VColors, Tangents,
        /*bCreateCollision=*/false);
    return true;
}
