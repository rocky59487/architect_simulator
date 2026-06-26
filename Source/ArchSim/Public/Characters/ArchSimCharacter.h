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

UCLASS(Blueprintable, BlueprintType)
class ARCHSIM_API AArchSimCharacter : public AAlsCharacter
{
    GENERATED_BODY()

public:
    AArchSimCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Enhanced Input setup lives here; AS-03b fills the body. Stub kept so AS-03b
    // edits ONLY the body, no method-signature churn.
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
