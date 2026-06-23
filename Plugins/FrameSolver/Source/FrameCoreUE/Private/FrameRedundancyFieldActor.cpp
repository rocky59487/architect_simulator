#include "FrameCoreUE/FrameRedundancyFieldActor.h"
#include "FrameCoreUE/FrameInteractiveSubsystem.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"   // FFrameModelPatch
#include "FramePMCHelpers.h"
#include "ProceduralMeshComponent.h"

namespace
{
    FLinearColor JumpRamp(float t)
    {
        t = FMath::Clamp(t, 0.f, 1.f);
        // Low jump (high redundancy) -> blue; high jump (low redundancy) -> red.
        return FLinearColor(t, 0.3f, 1.f - t, 1.f);
    }
}

AFrameRedundancyFieldActor::AFrameRedundancyFieldActor()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RedundancyMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameRedundancyFieldActor::BeginPlay()
{
    Super::BeginPlay();
}

int32 AFrameRedundancyFieldActor::ComputeRedundancy()
{
    LastJumps.Reset();
    if (!Subsystem || !Subsystem->IsSessionActive()) { return 0; }

    // Baseline: resolve current active set, capture MaxDC.
    FFrameSolveResult Baseline;
    if (!Subsystem->ResolveCurrent(Baseline)) { return 0; }
    const float BaselineDC = Baseline.bSingular ? 0.f : Baseline.Utilization.MaxDC;

    LastJumps.Reserve(WatchedMemberIds.Num());
    int32 Probed = 0;
    for (int32 MemberId : WatchedMemberIds)
    {
        FFrameModelPatch DeactivatePatch;
        DeactivatePatch.DeactivateMemberIds = { MemberId };
        FFrameSolveResult Probe;
        const bool bOk = Subsystem->ApplyPatchAndResolve(DeactivatePatch, Probe);
        const float Jump = (Probe.bSingular)
            ? TNumericLimits<float>::Max()
            : (Probe.Utilization.MaxDC - BaselineDC);
        LastJumps.Add(Jump);
        ++Probed;

        // Reactivate to restore baseline state.
        FFrameModelPatch ReactivatePatch;
        ReactivatePatch.ReactivateMemberIds = { MemberId };
        FFrameSolveResult Restore;
        Subsystem->ApplyPatchAndResolve(ReactivatePatch, Restore);
    }
    OnRedundancyComputed.Broadcast();
    return Probed;
}

bool AFrameRedundancyFieldActor::BuildMesh()
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();
    if (MemberGeometry.Num() == 0) { return false; }

    const float SatGuard = FMath::Max(SaturationJump, 1e-6f);
    int32 Built = 0;
    for (int32 i = 0; i < MemberGeometry.Num(); ++i)
    {
        const float Jump = LastJumps.IsValidIndex(i) ? LastJumps[i] : 0.f;
        const float t = FMath::Min(Jump / SatGuard, 1.f);
        BuildOneMemberSection(Built, MemberGeometry[i], t);
        ++Built;
    }
    return Built > 0;
}

void AFrameRedundancyFieldActor::BuildOneMemberSection(int32 SectionIdx,
                                                      const FFrameMemberGeometry& Geom,
                                                      float JumpValueNormalised)
{
    const FLinearColor Col = JumpRamp(JumpValueNormalised);
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
    const float halfW = Geom.Width * 0.5f;
    const float halfD = Geom.Depth * 0.5f;
    auto Corner = [&](int32 c) { return FrameCorePMC::CornerOffset(c, RefY, RefZ, halfW, halfD); };

    for (int32 k = 0; k < NRings; ++k)
    {
        const float t = (NRings == 1) ? 0.f : (float)k / (float)(NRings - 1);
        const FVector Center = FMath::Lerp(Geom.Start, Geom.End, t);
        for (int32 c = 0; c < 4; ++c)
        {
            const FVector V = Center + Corner(c);
            Vertices.Add(V);
            VColors.Add(Col);
            Normals.Add((V - Center).GetSafeNormal());
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
