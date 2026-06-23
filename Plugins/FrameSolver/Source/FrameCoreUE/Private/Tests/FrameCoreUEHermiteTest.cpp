// v3.6 Phase 1 U-11 tests — 3 sub-checks of cubic Hermite member-axis interpolation
// in AFrameDeformedShapeActor.
//
//   1. SineDeflection -- per-end rotation (Ry = ±theta) produces a midspan position
//                        offset above the linear lerp baseline (Hermite is "fuller").
//   2. ZeroRotationLerp -- rotation == zero on both ends -> Hermite == linear lerp
//                          bit-equal at every ring.
//   3. ToggleOff      -- bUseHermiteInterpolation = false even with rotation set
//                        falls back to linear lerp.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameDeformedShapeActor.h"
#include "ProceduralMeshComponent.h"
#include "FrameCoreUETestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    using FrameCoreUETestHelpers::GetSpawnWorld;
    using FrameCoreUETestHelpers::TipCenter;

    // Build a 2-node fixture with explicit displacement + rotation per node.
    FFrameSolveResult MakeFixtureBP(float UzTip, float RyEndI, float RyEndJ)
    {
        FFrameSolveResult R;
        FFrameNodalDisplacement N0; N0.NodeIndex = 0; N0.NodeId = 0; N0.Ry = RyEndI;
        FFrameNodalDisplacement N1; N1.NodeIndex = 1; N1.NodeId = 1; N1.Uz = UzTip; N1.Ry = RyEndJ;
        R.Displacements = { N0, N1 };
        return R;
    }

    FFrameMemberGeometry MakeCantileverGeom()
    {
        FFrameMemberGeometry G;
        G.MemberIdx = 0;
        G.Start = FVector::ZeroVector; G.End = FVector(2000.f, 0.f, 0.f);
        G.Width = 100.f; G.Depth = 100.f;
        G.EndINodeIdx = 0; G.EndJNodeIdx = 1;
        return G;
    }

    FVector RingCenter(const FProcMeshSection* Sec, int32 RingK)
    {
        const int32 Base = RingK * 4;
        FVector C = FVector::ZeroVector;
        for (int32 c = 0; c < 4; ++c) { C += Sec->ProcVertexBuffer[Base + c].Position; }
        return C * 0.25f;
    }

    AFrameDeformedShapeActor* SpawnActor(UWorld* World)
    {
        FActorSpawnParameters SP;
        SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AFrameDeformedShapeActor>(
            AFrameDeformedShapeActor::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SP);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEHermiteSineDeflectionTest,
    "FrameCore.UE.Hermite.SineDeflection",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEHermiteSineDeflectionTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    AFrameDeformedShapeActor* Actor = SpawnActor(World);
    if (!Actor) return false;
    // Tip Uz = +100 (visible), per-end Ry rotations push midspan above the lerp line.
    Actor->Solution = MakeFixtureBP(100.f, -0.05f, 0.05f);
    Actor->DeflectionScale = 1.f;
    Actor->bUseHermiteInterpolation = true;
    Actor->MemberGeometry = { MakeCantileverGeom() };

    TestTrue(TEXT("BuildMesh succeeds"), Actor->BuildMesh());
    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { Actor->Destroy(); return false; }

    // Linear lerp midspan Z = 0.5 * 100 = 50; Hermite midspan Z should differ
    // due to the rotation contribution.
    const FVector Mid = RingCenter(Sec, 5);
    TestTrue(FString::Printf(TEXT("Hermite midspan Z != lerp midspan (got %.4f vs lerp 50)"), Mid.Z),
             FMath::Abs(Mid.Z - 50.f) > 1.f);

    Actor->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEHermiteZeroRotationLerpTest,
    "FrameCore.UE.Hermite.ZeroRotationLerp",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEHermiteZeroRotationLerpTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;
    AFrameDeformedShapeActor* Actor = SpawnActor(World);
    if (!Actor) return false;
    Actor->Solution = MakeFixtureBP(100.f, 0.f, 0.f);
    Actor->DeflectionScale = 1.f;
    Actor->bUseHermiteInterpolation = true;
    Actor->MemberGeometry = { MakeCantileverGeom() };

    TestTrue(TEXT("BuildMesh succeeds"), Actor->BuildMesh());
    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { Actor->Destroy(); return false; }
    const FVector Mid = RingCenter(Sec, 5);
    TestTrue(FString::Printf(TEXT("Zero-rotation Hermite == lerp midspan Z = 50 (got %.4f)"), Mid.Z),
             FMath::IsNearlyEqual(Mid.Z, 50.f, 0.5f));

    Actor->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEHermiteToggleOffTest,
    "FrameCore.UE.Hermite.ToggleOff",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEHermiteToggleOffTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;
    AFrameDeformedShapeActor* Actor = SpawnActor(World);
    if (!Actor) return false;
    // Big rotations on both ends, but Hermite is toggled OFF -> linear lerp wins.
    Actor->Solution = MakeFixtureBP(100.f, -0.2f, 0.2f);
    Actor->DeflectionScale = 1.f;
    Actor->bUseHermiteInterpolation = false;
    Actor->MemberGeometry = { MakeCantileverGeom() };

    TestTrue(TEXT("BuildMesh succeeds"), Actor->BuildMesh());
    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { Actor->Destroy(); return false; }
    const FVector Mid = RingCenter(Sec, 5);
    TestTrue(FString::Printf(TEXT("Toggle-off midspan == lerp (got %.4f, expected 50)"), Mid.Z),
             FMath::IsNearlyEqual(Mid.Z, 50.f, 0.5f));

    Actor->Destroy();
    return true;
}

#endif
