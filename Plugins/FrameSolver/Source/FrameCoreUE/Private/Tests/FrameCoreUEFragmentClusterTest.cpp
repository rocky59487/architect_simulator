// v3.5 Phase 5 tests -- 3 sub-checks of AFrameFragmentClusterActor.
//
//   1. SpawnCountMatchesClusters -- 2 clusters across 1 event -> 2 spawned debris actors.
//   2. CentroidPosition          -- spawned actor's location == FFrameFragmentCluster.COM
//                                   bit-exact (no transform offset).
//   3. ClearAndRespawn           -- ClearDebris drops the actors; SpawnFragmentDebris repopulates.
//
// HONEST: tests verify the Phase 5 thin slice (StaticMesh debris path). Full Chaos POD
// behaviour validation is v3.6 work.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMeshActor.h"

#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUE/FrameFragmentClusterActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    UWorld* GetSpawnWorld()
    {
        if (!GEngine) return nullptr;
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            if (Ctx.World()) return Ctx.World();
        }
        return nullptr;
    }

    FFrameDynCollapseResult MakeStubWithClusters()
    {
        FFrameDynCollapseResult R;
        R.Outcome = EFrameDynCollapseOutcome::Collapsed;
        FFrameDynCollapseEvent E; E.Time = 0.25f; E.Mode = EFrameFailMode::Bending;
        FFrameFragmentCluster C0;
        C0.Mass = 2.f;     // 2 tonnes
        C0.COM  = FVector(1000.f, 0.f, 500.f);
        C0.Vel  = FVector(0.f, 0.f, -100.f);
        C0.Members = { 1, 2 };
        FFrameFragmentCluster C1;
        C1.Mass = 0.8f;
        C1.COM  = FVector(0.f, 1000.f, 500.f);
        C1.Vel  = FVector(0.f, 0.f, 0.f);
        C1.Members = { 3 };
        E.Detached = { C0, C1 };
        R.Events = { E };
        return R;
    }
}

// --- 1. SpawnCountMatchesClusters --------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEFragmentSpawnCountTest,
    "FrameCore.UE.FragmentCluster.SpawnCount",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEFragmentSpawnCountTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameFragmentClusterActor* Actor =
        World->SpawnActor<AFrameFragmentClusterActor>(AFrameFragmentClusterActor::StaticClass(),
                                                      FVector::ZeroVector, FRotator::ZeroRotator, SP);
    TestNotNull(TEXT("Actor spawned"), Actor);
    if (!Actor) return false;

    Actor->CollapseResult = MakeStubWithClusters();
    const int32 N = Actor->SpawnFragmentDebris();
    TestEqual(TEXT("Spawned debris count == 2"), N, 2);
    TestEqual(TEXT("GetSpawnedDebris size == 2"), Actor->GetSpawnedDebris().Num(), 2);

    Actor->ClearDebris();
    Actor->Destroy();
    return true;
}

// --- 2. CentroidPosition -----------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEFragmentCentroidPositionTest,
    "FrameCore.UE.FragmentCluster.CentroidPosition",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEFragmentCentroidPositionTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameFragmentClusterActor* Actor =
        World->SpawnActor<AFrameFragmentClusterActor>(AFrameFragmentClusterActor::StaticClass(),
                                                      FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (!Actor) return false;

    Actor->CollapseResult = MakeStubWithClusters();
    Actor->SpawnFragmentDebris();
    const TArray<TObjectPtr<AStaticMeshActor>>& Debris = Actor->GetSpawnedDebris();
    TestEqual(TEXT("2 chunks spawned"), Debris.Num(), 2);
    if (Debris.Num() < 2) { Actor->Destroy(); return false; }

    const FVector C0Pos = Debris[0]->GetActorLocation();
    const FVector C1Pos = Debris[1]->GetActorLocation();
    TestTrue(TEXT("Chunk 0 location == (1000, 0, 500)"),
             FVector::DistSquared(C0Pos, FVector(1000.f, 0.f, 500.f)) < 0.01f);
    TestTrue(TEXT("Chunk 1 location == (0, 1000, 500)"),
             FVector::DistSquared(C1Pos, FVector(0.f, 1000.f, 500.f)) < 0.01f);

    Actor->ClearDebris();
    Actor->Destroy();
    return true;
}

// --- 3. ClearAndRespawn ------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEFragmentClearRespawnTest,
    "FrameCore.UE.FragmentCluster.ClearAndRespawn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEFragmentClearRespawnTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameFragmentClusterActor* Actor =
        World->SpawnActor<AFrameFragmentClusterActor>(AFrameFragmentClusterActor::StaticClass(),
                                                      FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (!Actor) return false;

    Actor->CollapseResult = MakeStubWithClusters();
    Actor->SpawnFragmentDebris();
    TestEqual(TEXT("After first spawn: 2 chunks"), Actor->GetSpawnedDebris().Num(), 2);

    Actor->ClearDebris();
    TestEqual(TEXT("After clear: 0 chunks"), Actor->GetSpawnedDebris().Num(), 0);

    Actor->SpawnFragmentDebris();
    TestEqual(TEXT("After respawn: 2 chunks"), Actor->GetSpawnedDebris().Num(), 2);

    Actor->ClearDebris();
    Actor->Destroy();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
