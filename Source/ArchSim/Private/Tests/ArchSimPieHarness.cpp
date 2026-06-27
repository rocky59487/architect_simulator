// ArchSimPieHarness.cpp
// Implementation of PIE-world bootstrap helpers.
// See ArchSimPieHarness.h for design rationale and coverage-level contract.

#include "Tests/ArchSimPieHarness.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Subsystems/ArchSimModelRegistry.h"

// ---------------------------------------------------------------------------
// GetOrFindWorld
// ---------------------------------------------------------------------------
// Scan GEngine's world contexts and return the first one that has a valid
// World pointer. In -ExecCmds=Automation commandlet mode the engine always
// provides at least one context (the editor "preview" world).
// Precedent: FrameCoreUEActorStressMeshTest.cpp:35-51.
// NOT using UWorld::CreateWorld — see header rationale.
// ---------------------------------------------------------------------------
UWorld* ArchSimPieHarness::GetOrFindWorld()
{
    if (!GEngine) { return nullptr; }

    for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
    {
        if (Ctx.World()) { return Ctx.World(); }
    }

    // No valid world found — unexpected in any normal UE run. Return nullptr so
    // callers can emit a TestNotNull diagnostic rather than crashing.
    return nullptr;
}

// ---------------------------------------------------------------------------
// GetOrFindGameInstance
// ---------------------------------------------------------------------------
// Walk contexts looking for one that has an OwningGameInstance. In headless
// commandlet mode all contexts have OwningGameInstance == nullptr because no
// game has been launched. Returns nullptr honestly (Level 3 path).
// ---------------------------------------------------------------------------
UGameInstance* ArchSimPieHarness::GetOrFindGameInstance()
{
    if (!GEngine) { return nullptr; }

    for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
    {
        if (Ctx.OwningGameInstance) { return Ctx.OwningGameInstance; }
    }

    return nullptr;  // headless: no real GI available (Level 3, honest defer)
}

// ---------------------------------------------------------------------------
// GetOrCreateModelRegistry
// ---------------------------------------------------------------------------
// Level 1/2: obtain via GetSubsystem<UArchSimModelRegistry>() from a real GI.
// Level 3:   fallback NewObject<UArchSimModelRegistry>() — same pattern as
//            FrameCoreUEInteractiveSubsystemTest.cpp (NewObject<UFrameInteractiveSubsystem>).
// Always returns a non-null pointer.
// ---------------------------------------------------------------------------
UArchSimModelRegistry* ArchSimPieHarness::GetOrCreateModelRegistry()
{
    if (GEngine)
    {
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            if (Ctx.OwningGameInstance)
            {
                if (UArchSimModelRegistry* Reg =
                        Ctx.OwningGameInstance->GetSubsystem<UArchSimModelRegistry>())
                {
                    return Reg;  // Level 1 or Level 2
                }
            }
        }
    }

    // Level 3 fallback: subsystem API is available even without a real GI.
    // Matches the honest defer pattern established in AS-02c / AS-10.
    //
    // AS-26: GetTransientPackage() outer for ClassWithin(UGameInstance) consistency.
    // UArchSimModelRegistry inherits from UGameInstanceSubsystem which has the
    // explicit UCLASS macro `UCLASS(Abstract, Within = GameInstance, MinimalAPI)`
    // at UE5.7 GameInstanceSubsystem.h:15. Without an explicit outer here the
    // NewObject default outer is already GetTransientPackage() (per UE5.7
    // UObjectGlobals.h:1919 default arg `GetTransientPackageAsObject()`), so this
    // change is technically equivalent to the no-arg call. The value of this fix
    // is intent-documentation + parity with the AS-24 fix at FrameCoreUE 3 test
    // sites (commit 2883d40). See HANDOFF_v0.3.0.md §4 AS-24 first-action.
    return NewObject<UArchSimModelRegistry>(GetTransientPackage());
}

// ---------------------------------------------------------------------------
// IsRegistryFromRealGI
// ---------------------------------------------------------------------------
// Re-queries GEngine each call (no cached state) so it stays accurate across
// successive harness invocations in the same test run.
// ---------------------------------------------------------------------------
bool ArchSimPieHarness::IsRegistryFromRealGI()
{
    if (!GEngine) { return false; }

    for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
    {
        if (Ctx.OwningGameInstance)
        {
            // A subsystem is registered iff the GI's subsystem manager contains it.
            if (Ctx.OwningGameInstance->GetSubsystem<UArchSimModelRegistry>() != nullptr)
            {
                return true;  // Level 1 or Level 2
            }
        }
    }

    return false;  // Level 3: no real GI → NewObject fallback was (or would be) used
}
