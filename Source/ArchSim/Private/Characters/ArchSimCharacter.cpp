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

// FIX(v0.5.0 U-ALS iter 2): ALS data asset types needed at runtime-late timing.
// Root cause [INFERRED from ALS L400 code pattern, supported by ConstructorHelpers fail
// evidence at Saved/Logs/ArchSim-backup-2026.06.28-03.37.00.log:L916-929]:
//   AAlsCharacter::RefreshMeshProperties() (AlsCharacter.cpp:400) dereferences
//   AnimationInstance which is null when the mesh has no AnimBlueprint assigned.
//   ALS design expects a Blueprint child class to wire Settings / MovementSettings /
//   mesh / AnimBP in Details panel. Since AArchSimGameMode spawns AArchSimCharacter
//   directly (no BP child), we must wire them in C++.
//
//   Iter 1 used ConstructorHelpers::FObjectFinder in the ctor (CDO phase), but ALS
//   plugin content is NOT mounted at CDO construction time, causing all 4 finders to
//   fail (evidence: ArchSim-backup-2026.06.28-03.37.00.log:L916-929). Iter 2 switches
//   to LoadObject<T>() called from PostInitProperties() + BeginPlay() fallback, which
//   run after plugin content is mounted.
#include "Settings/AlsCharacterSettings.h"
#include "Settings/AlsMovementSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"

AArchSimCharacter::AArchSimCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // bUseControllerRotationYaw = false: ALS character handles its own rotation
    // via its locomotion state machine; ACharacter's default controller-driven
    // yaw would fight the ALS state machine and produce snap-spin artefacts.
    bUseControllerRotationYaw  = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll  = false;

    // FIX(v0.5.0 U-ALS iter 2): ALS asset wiring moved to PostInitProperties() /
    // BeginPlay() via LoadAlsAssetsLate(). ConstructorHelpers are NOT used here because
    // ALS plugin content is not mounted during CDO construction (ctor) phase.
    // See LoadAlsAssetsLate() below for the runtime-late approach.

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

// FIX(v0.5.0 U-ALS iter 2): Runtime-late ALS asset loader helper.
//
// WHY LoadObject not ConstructorHelpers:
//   ConstructorHelpers::FObjectFinder runs at CDO construction time (ctor call,
//   module load phase). At that point the ALS plugin content has not been mounted
//   yet — all 4 finders returned null (evidence: ArchSim-backup-2026.06.28-03.37.00.log
//   L916-929: "CDO Constructor (ArchSimCharacter): Failed to find /ALS/...").
//   LoadObject<T>() works at any point after module load when the asset registry
//   is available; PostInitProperties() / BeginPlay() satisfy this requirement.
//
// WHY guard each load individually:
//   AAlsCharacter already guards every AnimationInstance usage site with
//   .IsValid() (L141, L156, L215, L260, L271, L409); Settings/MovementSettings are
//   similarly guarded. If an asset path changes after an ALS upgrade, the null
//   pointer will produce a Warning log here rather than a silent miss, and the
//   ALS_ENSURE in AAlsCharacter::BeginPlay() (L139-141) will fire visibly.
//
// WHY SetAnimInstanceClass via TSubclassOf path:
//   LoadObject<UClass> with the _C generated-class path returns the anim instance
//   UClass directly (no separate FClassFinder step needed at runtime).
void AArchSimCharacter::LoadAlsAssetsLate()
{
    // ALS character settings — view / mantling / ragdoll parameters.
    if (!IsValid(Settings))
    {
        UAlsCharacterSettings* LoadedSettings = LoadObject<UAlsCharacterSettings>(
            nullptr, TEXT("/ALS/Data/Character/CS_Als_Default.CS_Als_Default"));
        if (IsValid(LoadedSettings))
        {
            Settings = LoadedSettings;
            UE_LOG(LogArchSim, Display,
                   TEXT("AArchSimCharacter [ALS] (%s): Settings loaded: %p"), *GetName(), Settings.Get());
        }
        else
        {
            UE_LOG(LogArchSim, Warning,
                   TEXT("AArchSimCharacter [ALS] (%s): LoadObject failed for CS_Als_Default — "
                        "ALS content may not be mounted yet or path changed."), *GetName());
        }
    }

    // ALS movement settings — walk/run/sprint speeds and acceleration curves.
    if (!IsValid(MovementSettings))
    {
        UAlsMovementSettings* LoadedMovement = LoadObject<UAlsMovementSettings>(
            nullptr, TEXT("/ALS/Data/Character/Movement/MS_Als_Normal.MS_Als_Normal"));
        if (IsValid(LoadedMovement))
        {
            MovementSettings = LoadedMovement;
            UE_LOG(LogArchSim, Display,
                   TEXT("AArchSimCharacter [ALS] (%s): MovementSettings loaded: %p"), *GetName(), MovementSettings.Get());
        }
        else
        {
            UE_LOG(LogArchSim, Warning,
                   TEXT("AArchSimCharacter [ALS] (%s): LoadObject failed for MS_Als_Normal — "
                        "ALS content may not be mounted yet or path changed."), *GetName());
        }
    }

    // ALS skeletal mesh — Manny-based rig the ALS state machine was authored for.
    if (IsValid(GetMesh()) && !IsValid(GetMesh()->GetSkeletalMeshAsset()))
    {
        USkeletalMesh* LoadedMesh = LoadObject<USkeletalMesh>(
            nullptr, TEXT("/ALS/Character/SKM_Als.SKM_Als"));
        if (IsValid(LoadedMesh))
        {
            GetMesh()->SetSkeletalMeshAsset(LoadedMesh);
            UE_LOG(LogArchSim, Display,
                   TEXT("AArchSimCharacter [ALS] (%s): SkeletalMesh loaded: %s"), *GetName(), *LoadedMesh->GetName());
        }
        else
        {
            UE_LOG(LogArchSim, Warning,
                   TEXT("AArchSimCharacter [ALS] (%s): LoadObject failed for SKM_Als — "
                        "ALS content may not be mounted yet or path changed."), *GetName());
        }
    }

    // ALS Animation Blueprint — UAlsAnimationInstance-derived graph driving the ALS state machine.
    // Without this, GetMesh()->GetAnimInstance() returns null, leaving AnimationInstance unset,
    // causing AAlsCharacter::RefreshMeshProperties():L400 to null-deref.
    // Load the generated _C UClass directly via LoadObject<UClass>.
    if (IsValid(GetMesh()) && GetMesh()->GetAnimClass() == nullptr)
    {
        UClass* LoadedAnimClass = LoadObject<UClass>(
            nullptr, TEXT("/ALS/Character/AB_Als.AB_Als_C"));
        if (LoadedAnimClass != nullptr)
        {
            GetMesh()->SetAnimInstanceClass(LoadedAnimClass);
            UE_LOG(LogArchSim, Display,
                   TEXT("AArchSimCharacter [ALS] (%s): AnimClass loaded: %s"), *GetName(), *LoadedAnimClass->GetName());
        }
        else
        {
            UE_LOG(LogArchSim, Warning,
                   TEXT("AArchSimCharacter [ALS] (%s): LoadObject failed for AB_Als_C — "
                        "ALS content may not be mounted yet or path changed."), *GetName());
        }
    }
}

void AArchSimCharacter::PostInitProperties()
{
    Super::PostInitProperties();
    // FIX(v0.5.0 U-ALS iter 2): attempt to load ALS assets at PostInitProperties timing.
    // This runs after ctor (CDO phase) but before BeginPlay — may succeed if plugin
    // content is mounted by this point (depends on plugin loading order).
    // BeginPlay() provides a second-chance fallback for cases where mount completes later.
    UE_LOG(LogArchSim, Display,
           TEXT("AArchSimCharacter [ALS] PostInitProperties: attempting LoadAlsAssetsLate"));
    LoadAlsAssetsLate();
}

void AArchSimCharacter::BeginPlay()
{
    // FIX(v0.5.0 U-ALS iter 2 fallback): if PostInitProperties() LoadObject returned null
    // (plugin content not yet mounted at that timing), try again here. BeginPlay fires
    // post-PIE-start, by which point all plugin content is fully mounted.
    if (!IsValid(Settings) || !IsValid(MovementSettings))
    {
        UE_LOG(LogArchSim, Display,
               TEXT("AArchSimCharacter [ALS] BeginPlay: Settings/MovementSettings still null — "
                    "re-attempting LoadAlsAssetsLate (BeginPlay fallback)"));
        LoadAlsAssetsLate();
    }

    Super::BeginPlay();
    UE_LOG(LogArchSim, Display,
           TEXT("AArchSimCharacter BeginPlay: %s"), *GetName());
    // AS-15: IMC registration moved to NotifyControllerChanged().
    // BeginPlay does NOT call AddMappingContext directly — doing so is wrong for
    // re-possess, server-side spawns, and late-join scenarios (audit A-02/D-01/D-02).
    // NotifyControllerChanged fires on every possession event and handles both
    // RemoveMappingContext (previous controller) and AddMappingContext (new controller).
}

// AS-15: Enhanced Input lifecycle refit.
// Mirrors Plugins/ALS/Source/ALSExtras/Private/AlsCharacterExample.cpp L19-49.
//
// Why NotifyControllerChanged (not BeginPlay)?
//   APawn::NotifyControllerChanged() fires on:
//     • initial possession (BeginPlay timing is fine here too, but redundant)
//     • controller swap mid-game (BeginPlay does NOT re-fire → stale IMC on new PC)
//     • unpossess (PreviousController populated → we can clean up cleanly)
//   BeginPlay fires exactly once at spawn — misses any subsequent controller change.
//
// Why RemoveMappingContext on PreviousController?
//   Without removal, a re-possess on a different PlayerController accumulates IMC
//   registrations.  The subsystem is per-LocalPlayer, so PreviousController's
//   subsystem is a different instance than NewController's — we must remove from
//   the old one explicitly (D-03).
//
// bNotifyUserSettings=true:
//   Triggers the UserSettings (UEnhancedInputUserSettings) rebind path so that
//   any player-customised key bindings stored in the save profile are applied
//   immediately on possession.  Cosmetically identical when no custom profile
//   exists, but required for the settings pipeline to work at all.
//
// Idempotency (Change 5 evaluation):
//   ALS does NOT add an "already registered?" guard.  UEnhancedInputLocalPlayerSubsystem
//   tracks registered contexts by pointer; AddMappingContext with the same IMC twice
//   is defined behaviour (second call bumps the priority if priority differs, otherwise
//   is a no-op in practice).  UE guarantees NotifyControllerChanged does not double-fire
//   for the same controller in a single possession.  We follow ALS precedent and skip
//   the guard — adding it would diverge from the reference pattern without evidence of
//   a real double-fire case on our platform.
void AArchSimCharacter::NotifyControllerChanged()
{
    // 1. Remove IMC from the previous controller's subsystem (handles controller swap
    //    and unpossess).  PreviousController is set by APawn before this virtual fires.
    const APlayerController* PreviousPlayer = Cast<APlayerController>(PreviousController);
    if (IsValid(PreviousPlayer))
    {
        UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
                PreviousPlayer->GetLocalPlayer());
        if (IsValid(InputSubsystem))
        {
            InputSubsystem->RemoveMappingContext(DefaultMappingContext);
        }
    }

    // 2. Register IMC with the new controller's local-player subsystem.
    APlayerController* NewPlayer = Cast<APlayerController>(GetController());
    if (IsValid(NewPlayer))
    {
        // ALS convention: reset deprecated input scale factors so legacy input
        // pipeline (pre-UE5 yaw/pitch multipliers) doesn't interfere with Enhanced
        // Input delta values.  Mirrors AlsCharacterExample.cpp L34-36.
        NewPlayer->InputYawScale_DEPRECATED   = 1.0f;
        NewPlayer->InputPitchScale_DEPRECATED = 1.0f;
        NewPlayer->InputRollScale_DEPRECATED  = 1.0f;

        UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
                NewPlayer->GetLocalPlayer());
        if (IsValid(InputSubsystem))
        {
            if (!IsValid(DefaultMappingContext))
            {
                // IMC null warning — fired at possession time rather than BeginPlay.
                // The asset must be assigned in the derived Blueprint's Details panel
                // (ArchSim|Input category).  See docs/INPUT_MAPPING.md.
                UE_LOG(LogArchSim, Warning,
                       TEXT("AArchSimCharacter [Input] (%s): DefaultMappingContext is null — "
                            "assign IMC_ArchSimDefault in the Blueprint Details panel "
                            "(ArchSim|Input). Input will not work."),
                       *GetName());
            }
            else
            {
                // bNotifyUserSettings=true: activates the UEnhancedInputUserSettings
                // rebind path so player-customised key mappings (saved in a profile)
                // are applied immediately on possession.
                FModifyContextOptions Options;
                Options.bNotifyUserSettings = true;
                InputSubsystem->AddMappingContext(DefaultMappingContext, /*Priority=*/0, Options);
            }
        }
        else
        {
            UE_LOG(LogArchSim, Warning,
                   TEXT("AArchSimCharacter [Input] (%s): UEnhancedInputLocalPlayerSubsystem "
                        "not found — is EnhancedInput enabled in project plugins?"),
                   *GetName());
        }
    }
    // Not locally controlled (AI pawn / dedicated server): no subsystem to touch.

    Super::NotifyControllerChanged();
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
               TEXT("AArchSimCharacter [Input] (%s): PlayerInputComponent is not a "
                    "UEnhancedInputComponent — Enhanced Input plugin may be "
                    "disabled.  Bindings skipped."),
               *GetName());
        return;
    }

    // Bind only if the IA asset is set; log warnings for missing slots so the
    // developer sees exactly which action is unassigned.

    // AS-15 / D-06: Axis-style and hold-style actions bind BOTH Triggered AND Canceled.
    // Canceled fires when a held key is released while the application loses focus
    // (ALT-tab, OS dialog, etc.) — without it the state machine stays in "Triggering"
    // because Completed never fires mid-focus-loss.
    // Precedent: Plugins/ALS/Source/ALSExtras/Private/AlsCharacterExample.cpp L69-82
    //   (Look / LookMouse / Move / Sprint / Jump each bind both events; Crouch does not).
    //
    // IA_Move — Axis2D.  Canceled delivers a zero vector → AddMovementInput(0) → no-op.
    if (IsValid(IA_Move))
    {
        EI->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ThisClass::HandleMove);
        EI->BindAction(IA_Move, ETriggerEvent::Canceled,  this, &ThisClass::HandleMove);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter [Input] (%s): IA_Move is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }

    // IA_Look — Axis2D.  Canceled delivers zero deltas → AddControllerYawInput(0) / PitchInput(0) = no-op.
    if (IsValid(IA_Look))
    {
        EI->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ThisClass::HandleLook);
        EI->BindAction(IA_Look, ETriggerEvent::Canceled,  this, &ThisClass::HandleLook);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter [Input] (%s): IA_Look is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }

    // IA_Jump — button (Started = press, Completed = release, Canceled = focus-lost while held).
    // Canceled → HandleJumpReleased: stops jump hold, prevents infinite jump-hold on return-focus.
    if (IsValid(IA_Jump))
    {
        EI->BindAction(IA_Jump, ETriggerEvent::Started,   this, &ThisClass::HandleJumpPressed);
        EI->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ThisClass::HandleJumpReleased);
        EI->BindAction(IA_Jump, ETriggerEvent::Canceled,  this, &ThisClass::HandleJumpReleased);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter [Input] (%s): IA_Jump is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }

    // IA_Sprint — hold-style.  Canceled → HandleSprintReleased: falls back to Running gait
    // on focus loss so the character doesn't sprint indefinitely when the player returns.
    if (IsValid(IA_Sprint))
    {
        EI->BindAction(IA_Sprint, ETriggerEvent::Started,   this, &ThisClass::HandleSprintPressed);
        EI->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ThisClass::HandleSprintReleased);
        EI->BindAction(IA_Sprint, ETriggerEvent::Canceled,  this, &ThisClass::HandleSprintReleased);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter [Input] (%s): IA_Sprint is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }

    // IA_Crouch — toggle (Started only).  Canceled intentionally NOT bound:
    // Canceled would fire on focus-loss while key is held and would toggle the stance
    // back to the wrong state.  ALS example (AlsCharacterExample.cpp L78) binds only
    // Triggered for CrouchAction — same reasoning.
    if (IsValid(IA_Crouch))
    {
        EI->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ThisClass::HandleCrouchToggle);
    }
    else
    {
        UE_LOG(LogArchSim, Warning,
               TEXT("AArchSimCharacter [Input] (%s): IA_Crouch is null — assign in BP Details (ArchSim|Input)."),
               *GetName());
    }
}

// AS-16 (D-08): CalcCamera override — route view through ALS camera pipeline.
// Mirrors Plugins/ALS/Source/ALSExtras/Private/AlsCharacterExample.cpp:51-60.
//
// Why IsValid(Camera) guard (divergence from ALS example)?
//   ALS's AlsCharacterExample::CalcCamera calls Camera->IsActive() directly without
//   an IsValid() check, relying on Camera being a DefaultSubobject (always non-null
//   after the ctor completes).  We add a defensive IsValid() because:
//     1. CDO construction is invoked before module finalisation; a partial CDO state
//        (e.g., a plugin loading mid-sequence) could leave Camera null momentarily.
//     2. PIE teardown GC may mark the component for collection before the owning
//        actor's EndPlay clears all camera update requests.
//     3. Cost is a single pointer validity check — effectively free on the hot path.
//   When Camera is null or GC-pending, we fall through to Super::CalcCamera(), which
//   returns the pawn's actor location + controller rotation (safe default).
//
// Why NO UE_LOG warning on Super fallback?
//   Camera->IsActive() returning false is a legitimate state (component deactivated,
//   first-person mode, etc.).  Logging every frame on the fallback path would flood
//   the output log with noise.  Real issues surface via the PIE camera being wrong —
//   which is observable and actionable without a per-frame log.
void AArchSimCharacter::CalcCamera(const float DeltaTime, FMinimalViewInfo& ViewInfo)
{
    if (IsValid(Camera) && Camera->IsActive())
    {
        Camera->GetViewInfo(ViewInfo);
        return;
    }

    Super::CalcCamera(DeltaTime, ViewInfo);
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
