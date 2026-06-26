// ArchSim - AArchSimGameMode impl. Spec: docs/logs/S-02/plan_2026-06-26T1033.md § AS-03c.

#include "ArchSimGameMode.h"
#include "Characters/ArchSimCharacter.h"

AArchSimGameMode::AArchSimGameMode()
{
    // DefaultPawnClass is what GameMode spawns for each PlayerController on
    // entry. Wiring it to AArchSimCharacter means PIE / packaged game spawns
    // the ALS-driven third-person pawn automatically.
    DefaultPawnClass = AArchSimCharacter::StaticClass();
}
