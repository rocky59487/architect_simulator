// v3.6 Phase 4 tests for AFrameUtilizationFieldActor (C7 沿桿 D/C).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameUtilizationFieldActor.h"
#include "ProceduralMeshComponent.h"
#include "FrameCoreUETestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    using FrameCoreUETestHelpers::GetSpawnWorld;

    AFrameUtilizationFieldActor* Spawn(UWorld* W)
    {
        FActorSpawnParameters SP;
        SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return W->SpawnActor<AFrameUtilizationFieldActor>(
            AFrameUtilizationFieldActor::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SP);
    }

    FFrameMemberGeometry MakeGeom()
    {
        FFrameMemberGeometry G;
        G.MemberIdx = 0; G.Start = FVector::ZeroVector; G.End = FVector(2000.f, 0.f, 0.f);
        G.Width = 100.f; G.Depth = 100.f;
        return G;
    }

    FFrameCapacity MakeCap()
    {
        FFrameCapacity C;
        C.Comp = 300.f; C.Tens = 300.f; C.Shear = 180.f;
        return C;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEUtilFieldCantileverDCTest,
    "FrameCore.UE.UtilField.CantileverDC",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEUtilFieldCantileverDCTest::RunTest(const FString&)
{
    UWorld* W = GetSpawnWorld(); if (!W) return false;
    AFrameUtilizationFieldActor* A = Spawn(W); if (!A) return false;

    // Build a stress trace where root sigma == 300 (DC=1.0) and tip sigma == 0.
    FFrameStressField F;
    FFrameMemberStressTrace T;
    for (int32 k = 0; k < 11; ++k)
    {
        FFrameStressFieldSample S;
        const float t = (float)k / 10.f;
        S.SigmaCompMax = 300.f * (1.f - t);
        T.Samples.Add(S);
    }
    F.Members = { T };
    A->Field = F;
    A->MemberGeometry = { MakeGeom() };
    A->Capacities = { MakeCap() };
    A->SaturationDC = 1.f;
    TestTrue(TEXT("Build"), A->BuildMesh());

    const FProcMeshSection* Sec = A->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { A->Destroy(); return false; }
    // Root ring 0 -- DC=1 -> red (R high). Tip ring 10 -- DC=0 -> blue (B high).
    const FColor Root = Sec->ProcVertexBuffer[0].Color;
    const FColor Tip  = Sec->ProcVertexBuffer[10 * 4].Color;
    TestTrue(FString::Printf(TEXT("root red R>128 (got %d)"), Root.R), Root.R > 128);
    TestTrue(FString::Printf(TEXT("tip blue B>200 (got %d)"), Tip.B),  Tip.B > 200);

    A->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEUtilFieldExceedanceTest,
    "FrameCore.UE.UtilField.ExceedanceFilter",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEUtilFieldExceedanceTest::RunTest(const FString&)
{
    UWorld* W = GetSpawnWorld(); if (!W) return false;
    AFrameUtilizationFieldActor* A = Spawn(W); if (!A) return false;

    // Mixed: half samples DC=0.5, half samples DC=1.5.
    FFrameStressField F;
    FFrameMemberStressTrace T;
    for (int32 k = 0; k < 11; ++k)
    {
        FFrameStressFieldSample S;
        S.SigmaCompMax = (k < 5) ? 150.f : 450.f;   // Cap=300; DC=0.5 or 1.5
        T.Samples.Add(S);
    }
    F.Members = { T };
    A->Field = F;
    A->MemberGeometry = { MakeGeom() };
    A->Capacities = { MakeCap() };
    A->bShowExceedanceOnly = true;
    A->SaturationDC = 1.f;
    TestTrue(TEXT("Build"), A->BuildMesh());

    const FProcMeshSection* Sec = A->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { A->Destroy(); return false; }
    // Ring 0 (DC=0.5, below cap) -> transparent blue placeholder (R<32 ideally).
    // Ring 8 (DC=1.5, above cap) -> red end of ramp.
    const FColor Under = Sec->ProcVertexBuffer[0 * 4].Color;
    const FColor Over  = Sec->ProcVertexBuffer[8 * 4].Color;
    TestTrue(TEXT("under-cap blue placeholder"), Under.B > 200 && Under.R < 32);
    TestTrue(TEXT("over-cap red"), Over.R > 128);

    A->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEUtilFieldUnstressedTest,
    "FrameCore.UE.UtilField.Unstressed",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEUtilFieldUnstressedTest::RunTest(const FString&)
{
    UWorld* W = GetSpawnWorld(); if (!W) return false;
    AFrameUtilizationFieldActor* A = Spawn(W); if (!A) return false;

    FFrameStressField F;
    FFrameMemberStressTrace T;
    for (int32 k = 0; k < 11; ++k) { T.Samples.Add(FFrameStressFieldSample{}); }
    F.Members = { T };
    A->Field = F;
    A->MemberGeometry = { MakeGeom() };
    A->Capacities = { MakeCap() };
    A->SaturationDC = 1.f;
    TestTrue(TEXT("Build"), A->BuildMesh());

    const FProcMeshSection* Sec = A->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { A->Destroy(); return false; }
    const FColor C = Sec->ProcVertexBuffer[0].Color;
    TestTrue(FString::Printf(TEXT("unstressed blue (got R=%d B=%d)"), C.R, C.B),
             C.B > 200 && C.R < 32);

    A->Destroy();
    return true;
}

#endif
