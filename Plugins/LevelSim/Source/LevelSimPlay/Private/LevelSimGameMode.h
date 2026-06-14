// ALevelSimGameMode — spawns the whole station scene at runtime (no content assets:
// engine basic shapes + runtime lights only), wires the staff targets into the pawn,
// and drives the deterministic -levelsim.smoke screenshot sequence.
//
// Launch (no .umap needed — scene is built in BeginPlay on the engine Entry map):
//   UnrealEditor.exe ArchSim.uproject /Engine/Maps/Entry?game=/Script/LevelSimPlay.LevelSimGameMode -game -windowed
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LevelSimGameMode.generated.h"

class ALevelSimPawn;

UCLASS()
class ALevelSimGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ALevelSimGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void SpawnScene();
    ALevelSimPawn* PlayerPawn() const;

    // smoke sequence
    bool   bSmoke = false;
    int32  SmokeStep = 0;
    double SmokeNextTime = 0;
    void   SmokeAdvance();
};
