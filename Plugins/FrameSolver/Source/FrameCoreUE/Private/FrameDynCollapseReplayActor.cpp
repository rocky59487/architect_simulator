#include "FrameCoreUE/FrameDynCollapseReplayActor.h"
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

AFrameDynCollapseReplayActor::AFrameDynCollapseReplayActor()
{
    PrimaryActorTick.bCanEverTick = true;
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DynCollapseMesh"));
    RootComponent = MeshComponent;
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->bUseAsyncCooking = false;
}

void AFrameDynCollapseReplayActor::BeginPlay()
{
    Super::BeginPlay();
    CurrentTime = 0.f;
    RebuildAtCurrentTime();
}

void AFrameDynCollapseReplayActor::SetPlaybackTime(float NewTime)
{
    // Scrub = silent jump (no event dispatch). The next Tick's (Prev, Current] window
    // starts at the scrubbed CurrentTime, so events strictly *after* the scrub point
    // still fire normally when playback resumes.
    CurrentTime = FMath::Max(NewTime, 0.f);
    RebuildAtCurrentTime();
}

void AFrameDynCollapseReplayActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bPlaying) { return; }
    if (CollapseResult.Frames.Num() == 0) { return; }

    const float TotalT = CollapseResult.Frames.Last().Time;
    const float Prev   = CurrentTime;
    // Audit D-06: clamp PlaybackSpeed at the use site so a BP that bypassed the UI
    // ClampMin can't drive CurrentTime negative.
    const float Speed  = FMath::Max(PlaybackSpeed, 0.f);
    CurrentTime += DeltaTime * Speed;
    if (CurrentTime > TotalT)
    {
        if (bLoop)
        {
            // Audit A-05: fire end-of-cycle events BEFORE wrapping, then fire the
            // post-wrap window. Without this, events in (Prev, TotalT] would be
            // silently dropped because the wrapped CurrentTime < Prev.
            DispatchEventsIn(Prev, TotalT);
            CurrentTime = FMath::Fmod(CurrentTime, FMath::Max(TotalT, KINDA_SMALL_NUMBER));
            DispatchEventsIn(-KINDA_SMALL_NUMBER, CurrentTime);
        }
        else
        {
            CurrentTime = TotalT;
            bPlaying = false;
            DispatchEventsIn(Prev, CurrentTime);
        }
    }
    else
    {
        DispatchEventsIn(Prev, CurrentTime);
    }
    RebuildAtCurrentTime();
}

void AFrameDynCollapseReplayActor::DispatchEventsIn(float A, float B)
{
    if (A > B) { return; }
    for (const FFrameDynCollapseEvent& E : CollapseResult.Events)
    {
        if (E.Time > A && E.Time <= B)
        {
            OnEventReached.Broadcast(E);
        }
    }
}

bool AFrameDynCollapseReplayActor::RebuildAtCurrentTime()
{
    if (!MeshComponent) { return false; }
    MeshComponent->ClearAllMeshSections();
    if (CollapseResult.Frames.Num() == 0 || MemberGeometry.Num() == 0) { return false; }

    // Find bracketing frames k, k+1 such that Frames[k].Time <= CurrentTime <= Frames[k+1].Time.
    const TArray<FFrameDynCollapseFrame>& Frames = CollapseResult.Frames;
    int32 k = 0;
    while (k + 1 < Frames.Num() && Frames[k + 1].Time < CurrentTime) { ++k; }
    const FFrameDynCollapseFrame& F0 = Frames[k];
    const FFrameDynCollapseFrame& F1 = (k + 1 < Frames.Num()) ? Frames[k + 1] : F0;
    const float DT = F1.Time - F0.Time;
    const float Alpha = (DT <= KINDA_SMALL_NUMBER) ? 0.f
                       : FMath::Clamp((CurrentTime - F0.Time) / DT, 0.f, 1.f);

    // Slice the flat UFlat into 6-component per-node sub-vectors and lerp.
    // Audit C-03: ensure the flat array is well-formed; a truncated UFlat silently
    // floors the slice and the last node renders as zero displacement.
    ensureMsgf(F0.UFlat.Num() % 6 == 0,
               TEXT("FFrameDynCollapseFrame.UFlat length %d is not a multiple of 6"),
               F0.UFlat.Num());
    const int32 SliceSize = (NodeCount > 0)
        ? NodeCount
        : (F0.UFlat.Num() / 6);
    if (SliceSize <= 0) { return false; }

    TArray<float> Lerped; Lerped.SetNumUninitialized(F0.UFlat.Num());
    const int32 N = FMath::Min(F0.UFlat.Num(), F1.UFlat.Num());
    for (int32 i = 0; i < N; ++i)
    {
        Lerped[i] = FMath::Lerp(F0.UFlat[i], F1.UFlat[i], Alpha);
    }
    for (int32 i = N; i < Lerped.Num(); ++i) { Lerped[i] = F0.UFlat[i]; }

    int32 SectionsBuilt = 0;
    for (const FFrameMemberGeometry& G : MemberGeometry)
    {
        BuildOneMemberSection(SectionsBuilt, G, Lerped, SliceSize);
        ++SectionsBuilt;
    }
    return SectionsBuilt > 0;
}

void AFrameDynCollapseReplayActor::BuildOneMemberSection(int32 SectionIdx,
                                                        const FFrameMemberGeometry& Geom,
                                                        const TArray<float>& Disp,
                                                        int32 NodesInFrame)
{
    auto Get = [&](int32 NodeIdx) -> FVector
    {
        if (NodeIdx < 0 || NodeIdx >= NodesInFrame) { return FVector::ZeroVector; }
        const int32 Base = NodeIdx * 6;
        if (Base + 2 >= Disp.Num()) { return FVector::ZeroVector; }
        return FVector(Disp[Base], Disp[Base + 1], Disp[Base + 2]);
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
    const FLinearColor BaseColor(0.85f, 0.55f, 0.30f, 1.f);
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
