// ArchSim - Headless smoke test for AArchSimCharacter + AArchSimGameMode lifecycle.
// Sprint S-02 AS-03d. Spec: docs/logs/S-02/plan_2026-06-26T1033.md § AS-03d.
//
// What this test CAN verify (in headless -nullrhi -unattended):
//   - AArchSimCharacter class hierarchy: derives from AAlsCharacter -> ACharacter -> APawn
//   - AArchSimGameMode class hierarchy: derives from AGameModeBase
//   - AArchSimGameMode::DefaultPawnClass == AArchSimCharacter::StaticClass()
//   - AArchSimCharacter default UPROPERTY values
//     (bUseControllerRotationYaw/Pitch/Roll all false from AS-03a ctor)
//   - LogArchSim category defined and writable
//   - UAlsCameraComponent is a default subobject named "Camera"
//
// What this test CANNOT verify in headless (deferred to AS-13 PIE-world fixture):
//   - Enhanced Input bindings (require live PlayerController + LocalPlayer)
//   - ALS state machine transitions (require live World tick + animation graph)
//   - Camera attachment runtime (UAlsCameraComponent::TickCamera needs PIE)
//   - Actor movement via simulated input
//
// Honors AS-07 lesson #1 (also applied in AS-10 + AS-02c): test pins what
// production ACTUALLY does in the fixture it can construct.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Characters/ArchSimCharacter.h"
#include "ArchSimGameMode.h"
#include "ArchSimGameInstance.h"     // for LogArchSim DECLARE_LOG_CATEGORY_EXTERN
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "AlsCharacter.h"
#include "AlsCameraComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArchSimCharacterClassSmokeTest,
    "ArchSim.Gameplay.CharacterInput",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArchSimCharacterClassSmokeTest::RunTest(const FString& Parameters)
{
    // ---- Sub-check 1: AArchSimCharacter class hierarchy -----------------------
    // StaticClass() is valid in headless — it only needs the UHT-generated
    // reflection machinery, which is compiled in unconditionally.
    UClass* CharClass = AArchSimCharacter::StaticClass();
    if (!TestNotNull(TEXT("Sub-check 1: AArchSimCharacter::StaticClass()"), CharClass))
        return false;
    TestTrue(TEXT("Sub-check 1a: AArchSimCharacter inherits AAlsCharacter"),
             CharClass->IsChildOf(AAlsCharacter::StaticClass()));
    TestTrue(TEXT("Sub-check 1b: AArchSimCharacter inherits ACharacter"),
             CharClass->IsChildOf(ACharacter::StaticClass()));
    TestTrue(TEXT("Sub-check 1c: AArchSimCharacter inherits APawn"),
             CharClass->IsChildOf(APawn::StaticClass()));

    // ---- Sub-check 2: AArchSimGameMode class hierarchy + DefaultPawnClass wire --
    // AArchSimGameMode() sets DefaultPawnClass = AArchSimCharacter::StaticClass()
    // in its ctor (AS-03c). The CDO is constructed during module load and is
    // accessible without a live World in headless.
    UClass* GMClass = AArchSimGameMode::StaticClass();
    if (!TestNotNull(TEXT("Sub-check 2: AArchSimGameMode::StaticClass()"), GMClass))
        return false;
    TestTrue(TEXT("Sub-check 2a: AArchSimGameMode inherits AGameModeBase"),
             GMClass->IsChildOf(AGameModeBase::StaticClass()));
    AArchSimGameMode* GMCDO = GMClass->GetDefaultObject<AArchSimGameMode>();
    if (!TestNotNull(TEXT("Sub-check 2b: GameMode CDO accessible"), GMCDO))
        return false;
    // DefaultPawnClass is TSubclassOf<APawn>; StaticClass() returns UClass*.
    // Compare via explicit TSubclassOf conversion to avoid template ambiguity.
    TestTrue(TEXT("Sub-check 2c: DefaultPawnClass == AArchSimCharacter (AS-03c wire)"),
             GMCDO->DefaultPawnClass == AArchSimCharacter::StaticClass());

    // ---- Sub-check 3: AArchSimCharacter CDO + AS-03a controller-rotation flags --
    // AArchSimCharacter::AArchSimCharacter() sets all three to false in the ctor
    // so the ALS locomotion state machine controls rotation, not the controller.
    // CDO is constructed without a World — safe in headless.
    AArchSimCharacter* CharCDO = CharClass->GetDefaultObject<AArchSimCharacter>();
    if (!TestNotNull(TEXT("Sub-check 3: Character CDO accessible"), CharCDO))
        return false;
    TestFalse(TEXT("Sub-check 3a: bUseControllerRotationYaw=false (AS-03a)"),
              CharCDO->bUseControllerRotationYaw);
    TestFalse(TEXT("Sub-check 3b: bUseControllerRotationPitch=false (AS-03a)"),
              CharCDO->bUseControllerRotationPitch);
    TestFalse(TEXT("Sub-check 3c: bUseControllerRotationRoll=false (AS-03a)"),
              CharCDO->bUseControllerRotationRoll);

    // ---- Sub-check 4: UAlsCameraComponent default subobject (AS-03c) -----------
    // CreateDefaultSubobject<UAlsCameraComponent>(TEXT("Camera")) is called in the
    // AArchSimCharacter ctor, so the CDO holds the created component instance.
    // FindComponentByClass<> searches the CDO's owned component list.
    UAlsCameraComponent* Cam =
        CharCDO->FindComponentByClass<UAlsCameraComponent>();
    if (!TestNotNull(TEXT("Sub-check 4: UAlsCameraComponent default subobject exists"), Cam))
        return false;
    TestEqual(TEXT("Sub-check 4a: Camera component named 'Camera' (AS-03c)"),
              Cam->GetName(), FString(TEXT("Camera")));

    // ---- Sub-check 5: Enhanced Input UPROPERTY slots present + null in CDO ------
    // AS-03b declares TObjectPtr<UInputMappingContext> DefaultMappingContext and
    // five TObjectPtr<UInputAction> IA_* as UPROPERTY slots. Default initialisation
    // leaves them nullptr; the BP child class assigns UAssets in the Details panel.
    // Testing IsNull() via .Get() correctly handles TObjectPtr<> wrapping.
    TestNull(TEXT("Sub-check 5a: DefaultMappingContext default null (BP assigns in Details)"),
             CharCDO->DefaultMappingContext.Get());
    TestNull(TEXT("Sub-check 5b: IA_Move default null"),
             CharCDO->IA_Move.Get());
    TestNull(TEXT("Sub-check 5c: IA_Look default null"),
             CharCDO->IA_Look.Get());
    TestNull(TEXT("Sub-check 5d: IA_Jump default null"),
             CharCDO->IA_Jump.Get());
    TestNull(TEXT("Sub-check 5e: IA_Sprint default null"),
             CharCDO->IA_Sprint.Get());
    TestNull(TEXT("Sub-check 5f: IA_Crouch default null"),
             CharCDO->IA_Crouch.Get());

    // ---- Sub-check 6: LogArchSim symbol resolves at link time -------------------
    // UE_LOG returns void, so the only observable signal is compile + link success.
    // Wrap in TestTrue to produce an explicit sub-check entry in the automation log.
    UE_LOG(LogArchSim, Display,
           TEXT("ArchSim.Gameplay.CharacterInput smoke test executing sub-check 6"));
    TestTrue(TEXT("Sub-check 6: LogArchSim symbol resolves at link time (UE_LOG compiled)"),
             true);

    // ---- Sub-check 7: Reflection metadata class names --------------------------
    // UHT-generated StaticClass() metadata must carry the C++ class name exactly
    // as written. This catches renames that break existing BP asset references.
    TestEqual(TEXT("Sub-check 7a: CharClass->GetName() == 'ArchSimCharacter'"),
              CharClass->GetName(), FString(TEXT("ArchSimCharacter")));
    TestEqual(TEXT("Sub-check 7b: GMClass->GetName() == 'ArchSimGameMode'"),
              GMClass->GetName(), FString(TEXT("ArchSimGameMode")));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
