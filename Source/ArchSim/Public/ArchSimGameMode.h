// ArchSim - AArchSimGameMode : the game mode for the architect-simulator
// gameplay session. Sprint S-02 AS-03c. Spec: docs/logs/S-02/plan_2026-06-26T1033.md § AS-03c.
//
// Inherits AGameModeBase (the modern subclass) rather than AGameMode (the
// pre-UE4.14 actor-with-match-state base). AGameModeBase has no match
// lifecycle / no waiting state, which is correct for a single-player
// experience.

#pragma once

#include "GameFramework/GameModeBase.h"
#include "ArchSimGameMode.generated.h"

UCLASS()
class ARCHSIM_API AArchSimGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AArchSimGameMode();
};
