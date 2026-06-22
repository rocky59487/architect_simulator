#include "FrameCoreUE/FrameFragmentClusterActor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"

AFrameFragmentClusterActor::AFrameFragmentClusterActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

int32 AFrameFragmentClusterActor::SpawnFragmentDebris()
{
    int32 SpawnCount = 0;
    for (const FFrameDynCollapseEvent& E : CollapseResult.Events)
    {
        for (const FFrameFragmentCluster& Cluster : E.Detached)
        {
            if (AStaticMeshActor* Spawned = SpawnOneChunk(Cluster))
            {
                SpawnedDebris.Add(Spawned);
                ++SpawnCount;
            }
        }
    }
    return SpawnCount;
}

void AFrameFragmentClusterActor::ClearDebris()
{
    for (TObjectPtr<AStaticMeshActor>& Actor : SpawnedDebris)
    {
        if (Actor) { Actor->Destroy(); }
    }
    SpawnedDebris.Reset();
}

AStaticMeshActor* AFrameFragmentClusterActor::SpawnOneChunk(const FFrameFragmentCluster& Cluster)
{
    UWorld* World = GetWorld();
    if (!World) { return nullptr; }

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Spawned =
        World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
                                            Cluster.COM, FRotator::ZeroRotator, SP);
    if (!Spawned) { return nullptr; }
    // Static-mesh actors default to non-movable; flip to movable so we can apply physics.
    Spawned->SetMobility(EComponentMobility::Movable);

    UStaticMeshComponent* MeshComp = Spawned->GetStaticMeshComponent();
    if (MeshComp)
    {
        if (ChunkMesh) { MeshComp->SetStaticMesh(ChunkMesh); }
        MeshComp->SetRelativeScale3D(FVector(ChunkScale));
        MeshComp->SetSimulatePhysics(true);
        MeshComp->SetMobility(EComponentMobility::Movable);
        // Mass override: Cluster.Mass is in tonnes, UE physics body expects kg.
        const float MassKg = FMath::Max(Cluster.Mass * 1000.f, 1.f);
        MeshComp->SetMassOverrideInKg(NAME_None, MassKg, /*bOverrideMass=*/true);
        // Initial velocity hint: Cluster.Vel is in mm/s, UE units are cm/s -> divide by 10.
        const FVector InitVel = Cluster.Vel * 0.1f;
        MeshComp->SetPhysicsLinearVelocity(InitVel);
        const FVector InitAngVel = Cluster.AngVel;
        MeshComp->SetPhysicsAngularVelocityInRadians(InitAngVel);
    }
    return Spawned;
}
