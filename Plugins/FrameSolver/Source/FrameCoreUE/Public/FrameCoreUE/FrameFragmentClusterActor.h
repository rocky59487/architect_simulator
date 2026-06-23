// v3.5 Phase 5 — fragment cluster spawner. Consumes FFrameDynCollapseResult.Events[N].Detached
// (FFrameFragmentCluster array) and spawns one physics-enabled child actor per cluster at the
// cluster centroid, with mass set to Cluster.Mass and initial linear velocity set to Cluster.Vel.
//
// HONEST BOUNDARY (Phase 5 thin slice): v3.5 ships AStaticMeshActor-based debris, not Chaos
// GeometryCollection POD destruction. The engine's FragmentCluster tells us *which members
// detached*; Chaos chooses *how the chunks fall*. The engine does not produce post-collapse
// physics — Chaos owns that. Full Chaos POD integration is v3.6 work (UE 5.7's Chaos
// destruction API has rough edges; the StaticMesh path delivers the same end-user effect
// — "a chunk falls when a member detaches" — without the Chaos plugin dependency drift).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameFragmentClusterActor.generated.h"

class AStaticMeshActor;
class UStaticMesh;

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Fragment Cluster Actor"))
class FRAMECOREUE_API AFrameFragmentClusterActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameFragmentClusterActor();

    // Source of fragments. Spawning iterates Events[N].Detached for every event.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Fragments")
    FFrameDynCollapseResult CollapseResult;

    // Optional static mesh asset for each spawned debris chunk. If null, the spawned actor
    // has no visible mesh (still receives physics simulation — useful for headless tests).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Fragments")
    TObjectPtr<UStaticMesh> ChunkMesh;

    // Visual scale applied to each spawned chunk's mesh transform. Default 1.0.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Fragments",
              meta=(ClampMin="0.001", UIMin="0.001"))
    float ChunkScale = 1.f;

    // U-14 cap: SpawnFragmentDebris early-exits once SpawnedDebris.Num() reaches this
    // value. Repeated calls without ClearDebris would otherwise grow unbounded and
    // leak `AStaticMeshActor`s via the SpawnedDebris UPROPERTY anchor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Fragments",
              meta=(ClampMin="1", UIMin="1"))
    int32 MaxDebrisActors = 1024;

    // Spawn all fragments encoded in CollapseResult.Events[*].Detached. Returns the count
    // of spawned actors. Existing children are NOT cleared — call ClearDebris first if you
    // want a fresh re-spawn.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Fragments")
    int32 SpawnFragmentDebris();

    UFUNCTION(BlueprintCallable, Category="FrameCore|Fragments")
    void ClearDebris();

    // Read-only access to the currently-spawned debris actors. UFunction params + return
    // values cannot use TObjectPtr (UHT enforces this), so the BP accessor materialises
    // a raw-pointer copy. C++ callers can reach SpawnedDebris directly via the
    // friend declaration in test code.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Fragments")
    TArray<AStaticMeshActor*> GetSpawnedDebrisArray() const
    {
        TArray<AStaticMeshActor*> Out;
        Out.Reserve(SpawnedDebris.Num());
        for (const TObjectPtr<AStaticMeshActor>& Ptr : SpawnedDebris) { Out.Add(Ptr.Get()); }
        return Out;
    }

    // C++ accessor — preserves TObjectPtr ownership semantics for module-internal callers
    // (tests, debris-clear loops).
    const TArray<TObjectPtr<AStaticMeshActor>>& GetSpawnedDebris() const { return SpawnedDebris; }

private:
    UPROPERTY()
    TArray<TObjectPtr<AStaticMeshActor>> SpawnedDebris;

    AStaticMeshActor* SpawnOneChunk(const FFrameFragmentCluster& Cluster);
};
