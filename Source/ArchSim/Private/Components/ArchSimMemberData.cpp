#include "Components/ArchSimMemberData.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UArchSimMemberData::UArchSimMemberData()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UArchSimMemberData::BeginPlay()
{
    Super::BeginPlay();

    // Guard against early-startup PIE paths where GameInstance hasn't initialised
    // its subsystems yet. When this fires the component will NOT be registered in
    // the model; the designer must ensure the Actor is spawned after GameInstance
    // is ready (e.g. placed in Level, not dynamically spawned in GameMode::StartPlay).
    // AS-19: warn so the developer sees the miss instead of silently getting a
    // zero-utilisation member that never appears in solve results. Retry-via-timer
    // was considered but deferred: weak-ptr capture + retry-cap + bRegistered
    // idempotency would exceed the S-03 scope budget for this LOW item.
    UWorld* World = GetWorld();
    if (!World || !World->GetGameInstance())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("UArchSimMemberData::BeginPlay (%s): World or GameInstance not "
                    "ready — component will NOT auto-register with UArchSimModelRegistry. "
                    "Check Actor spawn order; consider placing in Level rather than "
                    "spawning during GameMode::StartPlay."),
               IsValid(GetOwner()) ? *GetOwner()->GetName() : TEXT("<no owner>"));
        return;
    }

    if (UArchSimModelRegistry* Registry = UArchSimModelRegistry::Get(World))
    {
        Registry->RegisterMember(this);
    }
}

void UArchSimMemberData::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bRegistered && MemberIdx >= 0)
    {
        if (UWorld* World = GetWorld())
        {
            if (UArchSimModelRegistry* Registry = UArchSimModelRegistry::Get(World))
            {
                Registry->DeactivateMember(MemberIdx);
            }
        }
    }
    Super::EndPlay(EndPlayReason);
}
