// ArchSim - AArchSimCharacter : ALS-driven third-person pawn for the architect
// simulator. Subclasses AAlsCharacter (advanced locomotion). Sprint S-02 AS-03a.
// Spec: docs/logs/S-02/plan_2026-06-26T1033.md § AS-03a.
//
// Why subclass AAlsCharacter (not ACharacter)?
//   AAlsCharacter ships with: state machine driving locomotion / mantling /
//   ragdolling / rolling / view modes / pawn-side replication of desired states.
//   Building on top gives us a production-grade movement feel for free; we add
//   project-specific bindings (Enhanced Input -> ALS desired-state setters) in
//   AS-03b without touching ALS internals.

#pragma once

#include "AlsCharacter.h"        // from Plugins/ALS/Source/ALS/Public/
#include "ArchSimCharacter.generated.h"

// Forward declarations — avoid pulling in EnhancedInput headers into every TU
// that includes this header. Implementation TU includes the full headers.
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

// AS-03c: forward-decl only; AlsCameraComponent.h included in .cpp
class UAlsCameraComponent;

UCLASS(Blueprintable, BlueprintType)
class ARCHSIM_API AArchSimCharacter : public AAlsCharacter
{
    GENERATED_BODY()

public:
    AArchSimCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // ---- AS-03c: ALS Camera component ------------------------------------
    // Third-person camera driven by ALS state machine (view modes, shoulder
    // switch, FOV override, post-process weight). Attach to Mesh so it follows
    // the skeletal hierarchy. Assign UAlsCameraSettings asset in Blueprint
    // Details panel (Settings category).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ArchSim|Camera")
    TObjectPtr<UAlsCameraComponent> Camera;

    // ---- AS-03b: Enhanced Input asset bindings ----------------------------
    // The Input Mapping Context pushed on possession. Assign the
    // Content/Input/IMC_ArchSimDefault UAsset in the derived Blueprint's
    // Details panel (ArchSim|Input category). See docs/INPUT_MAPPING.md.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArchSim|Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    // Per-action InputAction asset references. Assign in BP Details panel.
    // See docs/INPUT_MAPPING.md for the full UAsset creation guide.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArchSim|Input")
    TObjectPtr<UInputAction> IA_Move;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArchSim|Input")
    TObjectPtr<UInputAction> IA_Look;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArchSim|Input")
    TObjectPtr<UInputAction> IA_Jump;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArchSim|Input")
    TObjectPtr<UInputAction> IA_Sprint;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArchSim|Input")
    TObjectPtr<UInputAction> IA_Crouch;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // ---- AS-03b: Enhanced Input handlers ----------------------------------
    // Design decision (HandleMove): uses camera/view-rotation XY projection,
    // NOT GetActorForwardVector. Rationale: third-person ALS characters decouple
    // visual facing from movement intent — pressing W should move towards the
    // camera's look direction, not the character model's current facing.
    // This matches AAlsCharacterExample::Input_OnMove exactly and avoids
    // snap-spin when the model is mid-turn animation.
    //
    // Called on ETriggerEvent::Triggered for IA_Move (Axis2D, X=right, Y=forward).
    void HandleMove(const FInputActionValue& Value);

    // Called on ETriggerEvent::Triggered for IA_Look (Axis2D, X=yaw delta, Y=pitch delta).
    void HandleLook(const FInputActionValue& Value);

    // Called on ETriggerEvent::Started for IA_Jump (button pressed).
    void HandleJumpPressed(const FInputActionValue& Value);

    // Called on ETriggerEvent::Completed for IA_Jump (button released).
    void HandleJumpReleased(const FInputActionValue& Value);

    // Called on ETriggerEvent::Started for IA_Sprint (hold begin -> Sprinting).
    void HandleSprintPressed(const FInputActionValue& Value);

    // Called on ETriggerEvent::Completed for IA_Sprint (hold end -> Running).
    void HandleSprintReleased(const FInputActionValue& Value);

    // Called on ETriggerEvent::Started for IA_Crouch (toggle Standing <-> Crouching).
    void HandleCrouchToggle(const FInputActionValue& Value);
};
