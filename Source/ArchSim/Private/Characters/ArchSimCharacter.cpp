// ArchSim - AArchSimCharacter impl. Spec: docs/logs/S-02/plan_2026-06-26T1033.md § AS-03a.

#include "Characters/ArchSimCharacter.h"
#include "ArchSimGameInstance.h"   // for LogArchSim category

AArchSimCharacter::AArchSimCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // bUseControllerRotationYaw = false: ALS character handles its own rotation
    // via its locomotion state machine; ACharacter's default controller-driven
    // yaw would fight the ALS state machine and produce snap-spin artefacts.
    bUseControllerRotationYaw  = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll  = false;
}

void AArchSimCharacter::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogArchSim, Display,
           TEXT("AArchSimCharacter BeginPlay: %s"), *GetName());
}

void AArchSimCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UE_LOG(LogArchSim, Display,
           TEXT("AArchSimCharacter EndPlay: %s (reason=%d)"),
           *GetName(), static_cast<int32>(EndPlayReason));
    Super::EndPlay(EndPlayReason);
}

void AArchSimCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // AS-03b will bind Enhanced Input actions here.
    // Stub kept clean so AS-03b's diff is purely additive.
}
