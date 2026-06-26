# Agent log — AS-16-u1: CalcCamera override for ALSCamera pipeline

## Dispatch 2026-06-26T18:35 (iteration 1)

**Plan reference:** `docs/logs/S-03/plan_2026-06-26T1652.md` § Unit 4
**Domain skills loaded:** `ue5-engineer` (primary, ALS camera component)
**Budget:** 3h / 140K tokens / 35 steps / 22min timeout
**Dispatch mode:** sequential (Round 2 of 4) — after Unit 3 (AS-15-u1) on same `ArchSimCharacter.{h,cpp}` files

### Pre-flight reads done

- Post-Unit-3 state of `Source/ArchSim/Public/Characters/ArchSimCharacter.h` — confirms `NotifyControllerChanged` declaration landed at L77; `Camera` `TObjectPtr<UAlsCameraComponent>` UPROPERTY at L39-40 (from AS-03c)
- Post-Unit-3 state of `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` — confirms BeginPlay properly stripped; Camera ctor at L45-47 with `SetRelativeRotation_Direct({0, 90, 0})`
- ALS precedent `Plugins/ALS/Source/ALSExtras/Private/AlsCharacterExample.cpp:51-60` — gold-standard 9-line override
- ALS header decl `Plugins/ALS/Source/ALSExtras/Public/AlsCharacterExample.h:82` — `virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo) override;` in `protected:` block
- `UAlsCameraComponent::GetViewInfo` API confirmed: `void GetViewInfo(FMinimalViewInfo& ViewInfo) const;` — const member, writes ViewInfo by ref
- `APawn/AActor::CalcCamera` signature verified from `UE_5.7/Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h:3686` — `virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult)`

### File ownership

This unit owns:
- `Source/ArchSim/Public/Characters/ArchSimCharacter.h` (add CalcCamera virtual override declaration)
- `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` (implement CalcCamera override)
- `docs/logs/S-03/agent_AS-16-u1.md` (this log)

No conflict with Unit 3 (sequential dispatch; CalcCamera is a separate virtual method).

### Design decisions

**1. `IsValid(Camera)` guard added (divergence from ALS example)**

ALS's `AlsCharacterExample::CalcCamera` calls `Camera->IsActive()` without `IsValid()`. We add the guard because:
- CDO construction and PIE teardown GC can leave `Camera` null/pending in edge cases
- Cost is a single pointer check (free on hot path)
- Falling through to `Super::CalcCamera()` is safe (returns pawn location + controller rotation)

**2. No `UE_LOG` warning on Super fallback**

`Camera->IsActive()` returning false is a legitimate state (component deactivated, first-person mode, etc.). Per-frame log spam would be worse than the silent fallback. Real issues are observable via the wrong camera in PIE.

**3. Change 3 (test sub-check) — NOT added**

`CalcCamera` is a C++ virtual function, not a UFUNCTION, so `FindFunctionByName()` cannot locate it via UHT reflection. There is no CDO-level reflection hook available to confirm the override without calling it (which requires a live World tick). Decision: leave real runtime verification to AS-13-u2 PIE harness. No `$ExpectedUeTests` bump needed.

### Changes applied

**Change 1 — Header declaration** (`ArchSimCharacter.h`, after `NotifyControllerChanged`):
```cpp
// AS-16 (D-08): Route camera through UAlsCameraComponent::GetViewInfo ...
// Gold-standard precedent:
//   Plugins/ALS/Source/ALSExtras/Public/AlsCharacterExample.h:82
//   Plugins/ALS/Source/ALSExtras/Private/AlsCharacterExample.cpp:51-60
virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo) override;
```

**Change 2 — Implementation** (`ArchSimCharacter.cpp`, new `CalcCamera` method before Input handlers):
```cpp
void AArchSimCharacter::CalcCamera(const float DeltaTime, FMinimalViewInfo& ViewInfo)
{
    if (IsValid(Camera) && Camera->IsActive())
    {
        Camera->GetViewInfo(ViewInfo);
        return;
    }
    Super::CalcCamera(DeltaTime, ViewInfo);
}
```

### Verification results

| Step | Command / check | Result |
|------|----------------|--------|
| UE build | `Build.bat ArchSimEditor Win64 Development` | **Result: Succeeded** (exit 0, 4.88s) |
| AS-03d single test | `Automation RunTests ArchSim.Gameplay.CharacterInput` | **Result={成功}** (exit 0) |
| Leg 1 standalone | `build.bat` F1..F71 | **ALL PASS (failures=0)** |
| Leg 2 UE automation | headless `-nullrhi` | **141 tests green** |
| Leg 3 OpenSees | `opensees_compare.py` | **PASS** |
| Leg 4 deep audit | `linear_deep_audit` 104 checks | **PASS failures=0** |
| Leg 5 CLI round-trip | `cli_roundtrip.py` | **ALL PASS** |
| **5-leg gate** | `run_gate.ps1 -RequireOpenSees` | **GATE: PASS** (exit 0) |

### LOC delta

- `ArchSimCharacter.h`: +17 LOC (block comment 15 lines + declaration 2 lines)
- `ArchSimCharacter.cpp`: +25 LOC (block comment 19 lines + implementation 6 lines)
- Total: **+42 LOC** (within HIGH-tolerance 40±5 band; comment density is intentional — explains IsValid divergence and fallback reasoning per CLAUDE.md §3)

### ESCALATE

None.

---

## Adversarial review (iteration 1) 2026-06-26T18:50

**Verdict:** NITS (3 cosmetic findings, no backlog needed)

**Reviewer dimension coverage:** ✅ all 5/5 verified file:line
1. CalcCamera signature `virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo) override;` matches UE base at `Actor.h:3686` (APawn does NOT own override; super chain goes directly to AActor base) — VERIFIED
2. Route via `Camera->GetViewInfo(ViewInfo)` bit-identical to ALS L51-60 except for `IsValid(Camera)` defensive prefix — VERIFIED
3. `Super::CalcCamera` only in else branch when Camera invalid/inactive — VERIFIED
4. `Camera` is `TObjectPtr<UAlsCameraComponent>` (header L40, ctor L45 `CreateDefaultSubobject<UAlsCameraComponent>`) — NOT a bare `ACharacter::FollowCamera` ghost — VERIFIED
5. No null-deref via `IsValid(Camera) &&` short-circuit — VERIFIED

**鐵則 compliance:** ALL CONFIRMED
- FROZEN engine 0 行 / LevelSim 0 行 / 4 ext plugins 0 行 (ALS read-only for precedent)
- AS-13 / AS-17 / Phase-5 territory 0 行 (Source/ArchSim/Private/Tests/, Scripts/run_gate.ps1, docs/ARCHITECTURE_INDEX.md, docs/SPRINT_NOTES.md)

**Findings (3 NITS, no blocker):**

| # | severity | issue | action |
|---|---|---|---|
| N-01 | NITS | `const float` impl vs `float` header asymmetry | **No action** — matches ALS's own convention (`AlsCharacterExample.h:82` vs `.cpp:51`) |
| N-02 | NITS | Super chain (when Camera invalid) → AAlsCharacter::CalcCamera (L183-188) which routes through `OnCalculateCamera` BlueprintNativeEvent hook; when Camera valid → bypassed | **No action** — intentional mirror of ALS example; BP-side hook silently bypassed when Camera active (by design) |
| N-03 | NITS | Comment ratio header 88% / cpp 76% (vs AS-15's 5-line:1-decl ratio) | **No action** — justified by documenting IsValid divergence + ALS cite |

**Hidden assumptions (all 3 verified by reviewer):**
- AAlsCharacter::CalcCamera super chain exists at L183-188 ✓
- Camera is DefaultSubobject (always non-null in normal lifecycle) — IsValid is over-engineering but harmless ✓
- `GetViewInfo` is const member function ✓

**Decision:** Accept NITS. No backlog opened (all 3 findings are intentional design choices with clear reasoning + ALS precedent). Advance to Phase 4 feature commit.

---

## Phase 4 — Feature commit (no tag) 2026-06-26T18:55

**Mode:** Feature-commit only (per scope contract: v0.3.0 tag deferred to Unit 7 RELEASE).

**Files committed (3 — explicit `git add`):**
- `Source/ArchSim/Public/Characters/ArchSimCharacter.h` (+17)
- `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` (+25)
- `docs/logs/S-03/agent_AS-16-u1.md` (this log)

**Stats:** 3 files / +47 LOC / -0.

**Verification:** subagent ran the full 5-leg gate (PASS UE 141 / standalone / OpenSees / audit 104 / CLI). Sequential dispatch, no race.

**Next:** **Round 2 COMPLETE** (Unit 3 + Unit 4 both done). Loop back to Phase 2 for Round 3 (Unit 5 AS-13-u1 PIE harness — HIGH-risk unit due to `-nullrhi` PIE-world bootstrap risk per scope contract).
