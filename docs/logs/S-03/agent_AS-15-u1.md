# Agent log — AS-15-u1: Enhanced Input lifecycle refit

## Dispatch 2026-06-26T18:10 (iteration 1)

**Plan reference:** `docs/logs/S-03/plan_2026-06-26T1652.md` § Unit 3
**Domain skills loaded:** `ue5-engineer` (primary, Enhanced Input + UE lifecycle)
**Budget:** 4h / 180K tokens / 40 steps / 25min timeout
**Dispatch mode:** sequential (Round 2 of 4) — Unit 4 (AS-16) follows on the same file

### Pre-flight reads done by main thread

- `docs/logs/S-02/manager.md` AS-15 origin — confirms bundling 5 hardening findings (A-02 / D-01 / D-02 / D-03 / D-06)
- `Source/ArchSim/Public/Characters/ArchSimCharacter.h` — current header state (post Round 1 unchanged; AS-14 was .cpp-only)
- `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` L50-185 — current BeginPlay does the AddMappingContext (this is the refit target — moving to NotifyControllerChanged)
- `Plugins/ALS/Source/ALSExtras/Private/AlsCharacterExample.cpp` — gold-standard ALS precedent:
  - L19-49 `NotifyControllerChanged()` body (Remove from previous + Add to new with bNotifyUserSettings=true + Super::)
  - L62-89 `SetupPlayerInputComponent` with Canceled bindings on Look/LookMouse/Move/Sprint/Jump/Aim (NOT on Crouch/single-shot)
- `~/.claude/skills/domain/ue5-engineer/SUBAGENT_PREFIX.md` (310 lines)

### Confirmed ALS precedent pattern

```cpp
void AAlsCharacterExample::NotifyControllerChanged()
{
    // 1. Remove IMC from PreviousController (handles controller swap / unpossess)
    const auto* PreviousPlayer{Cast<APlayerController>(PreviousController)};
    if (IsValid(PreviousPlayer)) {
        auto* InputSubsystem{ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
            PreviousPlayer->GetLocalPlayer())};
        if (IsValid(InputSubsystem)) InputSubsystem->RemoveMappingContext(InputMappingContext);
    }
    // 2. Add IMC to new controller with bNotifyUserSettings=true (settings-driven rebind path)
    auto* NewPlayer{Cast<APlayerController>(GetController())};
    if (IsValid(NewPlayer)) {
        // (input scale defaults)
        auto* InputSubsystem{ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
            NewPlayer->GetLocalPlayer())};
        if (IsValid(InputSubsystem)) {
            FModifyContextOptions Options;
            Options.bNotifyUserSettings = true;
            InputSubsystem->AddMappingContext(InputMappingContext, 0, Options);
        }
    }
    Super::NotifyControllerChanged();
}
```

Canceled bindings (from ALS L69-82): added for **Look / Move / Sprint / Jump / Aim** (axis-style or hold-style). NOT added for Crouch / RotationMode / single-shot actions.

### File ownership

This unit owns:
- `Source/ArchSim/Public/Characters/ArchSimCharacter.h` (add NotifyControllerChanged virtual override)
- `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` (implement override + add Canceled bindings + remove IMC add from BeginPlay)
- `docs/logs/S-03/agent_AS-15-u1.md` (this log)

Unit 4 (AS-16-u1, follows sequentially) will also touch ArchSimCharacter.{h,cpp} for CalcCamera — NO merge conflict because AS-16 adds a separate virtual override.

### Composed prompt (verbatim)

```
[See full prompt in Agent tool invocation that follows.]
```

(Agent return will be appended after subagent finishes.)

---

## Agent return — 2026-06-26T18:08 (completed)

## Status
✅ DONE

## Files touched
| Path | LOC delta | Production / Test / Config / Docs | New? |
|---|---|---|---|
| `Source/ArchSim/Public/Characters/ArchSimCharacter.h` | +11 | Production | No |
| `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` | +82 / -43 (net +39) | Production | No |
| `docs/logs/S-03/agent_AS-15-u1.md` | +62 | Docs | No (append) |

Total production LOC delta: ~50 (within 30-50 plan estimate; under 80 hard limit).

## Design decisions (non-obvious only)

### NotifyControllerChanged 簽名確認
Grepped `E:\project\UE_5.7\Engine\Source\Runtime\Engine\Classes\GameFramework\Pawn.h` — signature is `ENGINE_API virtual void NotifyControllerChanged();` (no params, void return). UE Templates (`TP_TopDown/TwinStickCharacter.h:137`, `TP_ThirdPerson/CombatCharacter.h:325`) and ALS Example (`AlsCharacterExample.cpp:19`) all confirm `virtual void NotifyControllerChanged() override;`. Correct site, correct signature.

### bRegistered idempotency — NOT added (follows ALS precedent)
ALS example does not add a "is-IMC-already-registered?" guard. `UEnhancedInputLocalPlayerSubsystem::AddMappingContext` with the same pointer is benign (second call is effectively a no-op when priority is identical). UE guarantees `NotifyControllerChanged` does not double-fire for the same possession. Diverging from the ALS reference pattern without a concrete repro case would be premature. Decision: match ALS, skip guard, document reasoning in code comment.

### BeginPlay IMC add removed — clean
The old L59-97 block (IMC null check, PC cast, LocalPlayer fetch, subsystem add) is entirely replaced by a 4-line comment explaining the AS-15 refit. No double-add risk remains.

### Canceled binding on IA_Move / IA_Look
Handlers are graceful under zero input: `HandleMove` calls `ClampMagnitude012D` (zero vector → norm=0 → `AddMovementInput(0)` = no-op); `HandleLook` calls `AddControllerYawInput(0)` / `AddControllerPitchInput(0)` = no-op. No special-case needed.

### IA_Crouch Canceled NOT bound
ALS `AlsCharacterExample.cpp L78` binds `CrouchAction` with `Triggered` only (not Canceled). Our toggle semantics mean Canceled would spuriously re-toggle stance if the player holds C and alt-tabs. This matches ALS precedent exactly.

## Verification evidence (verbatim)

### UE incremental build
```
Result: Succeeded
Total execution time: 6.85 seconds
Exit code: 0
```

### AS-03d single test
```
LogAutomationController: Display: Test Completed. Result={成功} Name={CharacterInput} Path={ArchSim.Gameplay.CharacterInput}
LogAutomationCommandLine: Display: **** TEST COMPLETE. EXIT CODE: 0 ****
```

### 5-leg gate
```
[1/5] standalone FrameCore gate (build.bat)...
       standalone: ALL PASS  (failures=0) (exit 0)

[2/5] UE headless automation...
       UE automation: 141 tests run, exit code 0 (process exit 0; expected >= 141)

[3/5] OpenSees offline cross-validation...
       OpenSees compare: PASS (exit 0)

[4/5] linear-analysis deep audit...
       linear deep audit: PASS failures=0 checks=104 (exit 0)

[5/5] CLI round-trip (frame_cli J1 bridge)...
       CLI round-trip: ALL PASS  (failures=0) (exit 0)

======================================================
 GATE: PASS  (standalone OK, UE 141 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK)
Exit code: 0, elapsed: 360.3s
```

## Gotchas / discoveries
- `APlayerController* PreviousPlayer` in `NotifyControllerChanged` must be `const APlayerController*` on the Remove path (only calling `GetLocalPlayer()`, no mutation). Non-const on the Add path (need to set `InputYawScale_DEPRECATED`). Split declared separately.
- ALS `Input_OnSprint` uses a unified bool-value handler (`ActionValue.Get<bool>()` distinguishes press/release) rather than separate Started/Completed handlers. Our split Pressed/Released approach is functionally equivalent and consistent with existing code style — no change needed.
- `FModifyContextOptions` requires `#include "EnhancedInputSubsystems.h"` which was already in the .cpp include block (AS-03b). No new includes needed.

## Self-grading (honest)
- "NotifyControllerChanged is correct override site" — **[VERIFIED]** (grepped `UE_5.7/Engine/Source/Runtime/Engine/Classes/GameFramework/Pawn.h:382`; confirmed `ENGINE_API virtual void NotifyControllerChanged();` + 3 UE template usages)
- "RemoveMappingContext fires on previous controller" — **[VERIFIED]** (ALS example L21-29 confirms `PreviousController` is valid for the previous PlayerController at call time; UE APawn sets this before calling the virtual)
- "bNotifyUserSettings=true effect understood" — **[VERIFIED]** (flag activates `UEnhancedInputUserSettings` rebind pipeline; not cosmetic — required for saved key-binding profiles to apply on possession)
- "Canceled binding on hold-style only" — **[VERIFIED]** (grepped ALS L69-82: Move/Look/Sprint/Jump have Canceled; CrouchAction and single-shot actions do not)
- "BeginPlay IMC add removed (no double-add)" — **[VERIFIED]** (old L56-97 block fully deleted; only `Super::BeginPlay()` + Display log + explanatory comment remain)

## ESCALATE?
None. All 5 changes completed within scope. LOC delta ~50 (under 80 hard limit). 5-leg gate PASS.

---

## Adversarial review (iteration 1) 2026-06-26T18:25

**Verdict:** CLEAN

**Reviewer dimension coverage:** ✅ all 5/5 verified file:line
1. `NotifyControllerChanged` signature confirmed via `Pawn.h:382` grep (`ENGINE_API virtual void NotifyControllerChanged()`) + super-chain confirmed (`ACharacter` no override → APawn base)
2. `RemoveMappingContext` on PreviousController — APawn member confirmed at `APawn.h:202-204`; cpp impl L96-105
3. `bNotifyUserSettings=true` non-cosmetic — `EnhancedInputSubsystemInterface.h:99` field confirmed; reviewer added note that flag is silent no-op if project doesn't enable UEnhancedInputUserSettings class (gating, not bug)
4. Canceled bindings — verified cpp L198/L211/L226/L241 on 4 hold-style actions; L250-256 confirms IA_Crouch `Started` only (matches ALS L78)
5. No `bRegistered`-style double-add — APawn engine has `bNotifyControllerChange` gate guaranteeing engine-level no double-fire

**鐵則 compliance:** ALL CONFIRMED
- FROZEN engine 0 行 / LevelSim 0 行 / 4 ext plugins 0 行 (READ-only AlsCharacterExample for precedent)
- Phase 5 territory (run_gate.ps1 / ARCHITECTURE_INDEX.md / SPRINT_NOTES.md) 0 行
- All 5 [VERIFIED] claims have real file:line oracle

**Findings (2 LOW, no blocker):**

| # | severity | file:line | issue | action |
|---|---|---|---|---|
| 1 | LOW | `ArchSimCharacter.cpp:153` | `DefaultMappingContext null` warn msg lacks `(ArchSim\|Input)` prefix vs other warn messages | **→ Phase 5 docs sync inline polish** (cosmetic, no backlog needed) |
| 2 | LOW | `agent_AS-15-u1.md:16` | ALS cite says "L62-89" but actually L62-88 (line 89 is blank) | **→ no action** (doc-only ±1 line cite, log accuracy NIT) |

**Missed edge cases (acknowledged-but-OK):**
- ALS uses unified bool-handler for Sprint (Triggered only with `Get<bool>()`); ArchSim split Started/Completed/Canceled — both equivalent, subagent documented in Gotchas
- EndPlay doesn't actively RemoveMappingContext — correct because UnPossessed (which fires NotifyControllerChanged with Previous=this) runs before EndPlay in destroy path
- `bIgnoreAllPressedKeysUntilRelease` default true (FModifyContextOptions ctor) — possession mid-keypress would drop that input until release; matches ALS behavior, known UE design

**Hidden assumptions (verified):**
- UE NotifyControllerChanged no double-fire — confirmed at `APawn.cpp:617-622` `bNotifyControllerChange` guard
- Same-IMC repeat AddMappingContext is not silent no-op (it rebuilds mappings) but result is correct; matches ALS unguarded behavior
- `bNotifyUserSettings=true` requires project UEnhancedInputUserSettings class for full effect; silent gating if missing (not bug)

**Reviewer evidence:** Read 6 files, grep'd 11 patterns, cross-checked 12 claims. Reviewer explicitly verified ALS precedent line-by-line and APawn::PreviousController member existence.

**Decision:** Accept CLEAN. 1 cosmetic warn-msg polish deferred to Phase 5 docs sync. Advance to Phase 4 feature commit (no tag).
