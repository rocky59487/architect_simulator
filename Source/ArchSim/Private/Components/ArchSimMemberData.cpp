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
    // its subsystems yet -- silently skipping is safe; the registry will pick this
    // component up on the next placement burst via Get().
    UWorld* World = GetWorld();
    if (!World || !World->GetGameInstance())
    {
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
