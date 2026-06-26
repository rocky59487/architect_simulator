// ArchSim - AArchSimCharacter impl. Spec: docs/logs/S-02/plan_2026-06-26T1033.md § AS-03a/b.

#include "Characters/ArchSimCharacter.h"
#include "ArchSimGameInstance.h"   // for LogArchSim category

// AS-03b: Enhanced Input headers (kept in .cpp, not .h, to minimise include fan-out)
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

// AS-03b: ALS vector utility for view-space movement projection
#include "Utility/AlsVector.h"

// AS-03c: ALS camera component (included in .cpp to avoid fan-out from .h)
#include "AlsCameraComponent.h"

// AS-03b: ALS GameplayTag namespaces (AlsStanceTags, AlsGaitTags)
#include "Utility/AlsGameplayTags.h"

#include "InputActionValue.h"

AArchSimCharacter::AArchSimCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // bUseControllerRotationYaw = false: ALS character handles its own rotation
    // via its locomotion state machine; ACharacter's default controller-driven
    // yaw would fight the ALS state machine and produce snap-spin artefacts.
    bUseControllerRotationYaw  = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll  = false;

    // AS-03c: ALS camera component.
    // Attach to Mesh (not RootComponent) so the camera inherits the skeletal
    // hierarchy origin — ALS camera logic uses GetParentComponent() to resolve
    // the pivot and first-person eye location relative to the character mesh.
    // Use SetRelativeRotation_Direct (inherited from USceneComponent via the
    // skeletal-mesh parent chain) to bypass transform-dirty propagation in the
    // CDO ctor — this is the canonical form used by
    // AlsCharacterExample::AlsCharacterExample (Yaw=90 only; Roll=0).
    // (NITS-01 from S-02 AS-03c review: an earlier draft used
    // SetRelativeRotation with Roll=-90; fixed to align with the ALS example.)
    Camera = CreateDefaultSubobject<UAlsCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(GetMesh());
    Camera->SetRelativeRotation_Direct({0.0f, 90.0f, 0.0f});
}

void AArchSimCharacter::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogArchSim, Display,
           TEXT("AArchSimCharacter BeginPlay: %s"), *GetName());

    // AS-03b: Register the default Input Mapping Context with the local player's
    // Enhanced Input subsystem.  This runs on the client that owns this pawn;
    // listen-server or dedicated-server builds skip gracefully via LocalPlayer null.
    if (!IsValid(DefaultMappingContext))
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter (%s): DefaultMappingContext is null — "
                    "assign IMC_ArchSimDefault in the Blueprint Details panel "
                    "(ArchSim|Input). Input will not work."),
               *GetName());
        return;
    }

    const APlayerController* PC = Cast<APlayerController>(GetController());
    if (!IsValid(PC))
    {
        // Not locally controlled (e.g. AI pawn or dedicated server bot) — skip.
        return;
    }

    ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
    if (!IsValid(LocalPlayer))
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter (%s): GetLocalPlayer() returned null — "
                    "IMC not registered."),
               *GetName());
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
    if (!IsValid(InputSubsystem))
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter (%s): UEnhancedInputLocalPlayerSubsystem "
                    "not found — is EnhancedInput enabled in project plugins?"),
               *GetName());
        return;
    }

    InputSubsystem->AddMappingContext(DefaultMappingContext, /*Priority=*/0);
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

    // AS-03b: Bind Enhanced Input actions.
    // Cast guards against legacy UInputComponent in builds where Enhanced Input
    // is unexpectedly disabled; logs a warning so it's obvious in PIE.
    UEnhancedInputComponent* EI = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!IsValid(EI))
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter (%s): PlayerInputComponent is not a "
                    "UEnhancedInputComponent — Enhanced Input plugin may be "
                    "disabled.  Bindings skipped."),
               *GetName());
        return;
    }

    // Bind only if the IA asset is set; log warnings for missing slots so the
    // developer sees exactly which action is unassigned.

    if (IsValid(IA_Move))
    {
        EI->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ThisClass::HandleMove);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter (%s): IA_Move is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }

    if (IsValid(IA_Look))
    {
        EI->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ThisClass::HandleLook);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter (%s): IA_Look is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }

    if (IsValid(IA_Jump))
    {
        EI->BindAction(IA_Jump, ETriggerEvent::Started,   this, &ThisClass::HandleJumpPressed);
        EI->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ThisClass::HandleJumpReleased);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter (%s): IA_Jump is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }

    if (IsValid(IA_Sprint))
    {
        EI->BindAction(IA_Sprint, ETriggerEvent::Started,   this, &ThisClass::HandleSprintPressed);
        EI->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ThisClass::HandleSprintReleased);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter (%s): IA_Sprint is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }

    if (IsValid(IA_Crouch))
    {
        EI->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ThisClass::HandleCrouchToggle);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter (%s): IA_Crouch is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }
}

// ---------------------------------------------------------------------------
// AS-03b: Input handler implementations
// ---------------------------------------------------------------------------

void AArchSimCharacter::HandleMove(const FInputActionValue& Value)
{
    // IA_Move is Axis2D: X = right, Y = forward (standard UE Enhanced Input convention).
    // AS-14: clamp to unit circle before view-space projection. Analog stick corner
    // input and simultaneous keyboard W+D produce norm=sqrt(2)~1.414, which
    // over-drives AddMovementInput. UAlsVector::ClampMagnitude012D scales the vector
    // down to norm=1 when the magnitude exceeds 1, preserving direction and leaving
    // partial inputs (norm<1) unchanged. Matches AAlsCharacterExample::Input_OnMove
    // (AlsCharacterExample.cpp:109). Keyboard single-axis (norm=1.0) is unaffected.
    const FVector2D MoveInput = UAlsVector::ClampMagnitude012D(Value.Get<FVector2D>());

    // Use camera/view-space projection (same as AAlsCharacterExample::Input_OnMove).
    // Rationale: in ALS third-person, the character model faces the locomotion
    // direction, not the input direction, so we resolve W/S relative to where
    // the camera is pointing, not where the mesh faces.
    FRotator ViewRotation = GetViewState().Rotation;

    // Prefer the exact Controller view point when available (handles split-screen
    // and replicated view correctly).
    if (IsValid(GetController()))
    {
        FVector ViewLocation;
        GetController()->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }

    const FVector ForwardDir = UAlsVector::AngleToDirectionXY(UE_REAL_TO_FLOAT(ViewRotation.Yaw));
    const FVector RightDir   = UAlsVector::PerpendicularCounterClockwiseXY(ForwardDir);

    AddMovementInput(ForwardDir * MoveInput.Y + RightDir * MoveInput.X);
}

void AArchSimCharacter::HandleLook(const FInputActionValue& Value)
{
    // IA_Look is Axis2D: X = yaw delta, Y = pitch delta.
    const FVector2D LookInput = Value.Get<FVector2D>();
    AddControllerYawInput(LookInput.X);
    AddControllerPitchInput(LookInput.Y);
}

void AArchSimCharacter::HandleJumpPressed(const FInputActionValue& Value)
{
    // AAlsCharacterExample checks for ragdoll/mantle/crouch-stand before Jump().
    // AS-03b keeps it simple: straightforward jump. Future AS-03d / S-02 follow-up
    // can add the richer logic.
    Jump();
}

void AArchSimCharacter::HandleJumpReleased(const FInputActionValue& Value)
{
    StopJumping();
}

void AArchSimCharacter::HandleSprintPressed(const FInputActionValue& Value)
{
    // IA_Sprint Started event -> desired gait Sprinting.
    SetDesiredGait(AlsGaitTags::Sprinting);
}

void AArchSimCharacter::HandleSprintReleased(const FInputActionValue& Value)
{
    // IA_Sprint Completed event -> fall back to Running (not Walking).
    // Running is the ALS default gait; keeping Walking is a separate toggle.
    SetDesiredGait(AlsGaitTags::Running);
}

void AArchSimCharacter::HandleCrouchToggle(const FInputActionValue& Value)
{
    // Toggle between Standing and Crouching (ALS stance state machine).
    if (GetDesiredStance() == AlsStanceTags::Standing)
    {
        SetDesiredStance(AlsStanceTags::Crouching);
    }
    else
    {
        SetDesiredStance(AlsStanceTags::Standing);
    }
}
