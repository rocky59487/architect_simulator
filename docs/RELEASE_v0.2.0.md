# Release `v0.2.0` — Game body — ALS pawn end-to-end (Sprint S-02 close)

> Tagged 2026-06-26, branch `main`. **First minor bump** of the game-body
> v0.1.x → v0.2.x series. Bundles all four AS-03 units (a/b/c/d) on top of
> the v0.1.5 patch line. The user-visible feature: opening any default map
> now spawns the ALS-driven third-person character with a working camera
> and the Enhanced Input action slots ready for BP-side wiring.
>
> Sprint S-02 closes here. All 8 planned units shipped in one session,
> with 2 patch releases (v0.1.4, v0.1.5) along the way.

---

## 1. What `v0.2.0` is

One sentence: **the architect simulator now has a real playable foundation —
a third-person character driven by the ALS-Refactored v4.17 state machine,
spawned automatically by `AArchSimGameMode` on any default map, with
WASD/Mouse/Space/Shift/Ctrl bindings already wired in C++ awaiting the
`Content/Input/*` UAsset assets that the project author creates per
[`docs/INPUT_MAPPING.md`](INPUT_MAPPING.md).**

### Files changed (vs `v0.1.5`)

| Path | Type | Delta | Origin |
|---|---|---|---|
| `Source/ArchSim/Public/Characters/ArchSimCharacter.h` | Production (new) | +100 across a/b/c | AS-03a/b/c |
| `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` | Production (new) | +261 across a/b/c | AS-03a/b/c |
| `Source/ArchSim/Public/ArchSimGameMode.h` | Production (new) | +18 | AS-03c |
| `Source/ArchSim/Private/ArchSimGameMode.cpp` | Production (new) | +14 | AS-03c |
| `Source/ArchSim/Private/Tests/ArchSimCharacterTest.cpp` | Test (new) | +130 | AS-03d |
| `Source/ArchSim/ArchSim.Build.cs` | Build (mod) | `+"ALS"` + `+"ALSCamera"` | AS-03a + AS-03c |
| `Config/DefaultEngine.ini` | Config (mod) | `+GlobalDefaultGameMode` line | AS-03c |
| `Scripts/run_gate.ps1` | Build (mod) | L29 139 → 140 + AS-03d comment | AS-03d |
| `docs/INPUT_MAPPING.md` | Docs (new from v0.1.5 path; first tagged in v0.2.0) | ~100 lines | AS-03b |
| `docs/ARCHITECTURE_INDEX.md` | Docs | §2/§6/§7/§8 updates | release |
| `docs/RELEASE_v0.2.0.md` | Docs | new | release |
| `docs/HANDOFF_v0.2.0.md` | Docs | new | release |
| `docs/logs/S-02/agent_AS-03*.md` | Sprint logs | new (a/b/c/d) | release |
| `docs/logs/S-02/manager.md` | Sprint log | append | release |
| `README.md` | Docs | v0.2.0 status block prepended | release |

**Engine source delta vs v0.1.5 = 0 lines** under `Plugins/FrameSolver/Source/FrameCore/`.
**Engine source delta across the entire Sprint S-02 (v0.1.3 → v0.2.0) = 0 lines.**
**LevelSim source delta = 0 lines.**
**`UArchSimModelRegistry.{h,cpp}` delta this release = 0** (production logic
byte-identical; only AS-10's v0.1.4 telemetry getters stand).
**ArchSim production code delta vs v0.1.5 = ~394 lines** (Character + GameMode
across the 4 AS-03 units).

### What was NOT done

- FrameCore engine source (FROZEN)
- LevelSim engine (FROZEN)
- 4 external plugin clones (ALS / Prefabricator / SPUD / SUQS — only referenced)
- `ArchSim.uproject` / `.gitignore` / build artifacts
- `UArchSimModelRegistry.{h,cpp}` (registry production logic byte-identical since v0.1.4 AS-10's telemetry getters)
- v0.1.x already-shipped tests (`SaveLoadRoundTrip` / `MaxRankCeiling` / `RebaselineCeiling` / `TickDriver` all preserved)
- Actor position-change sync (still deferred; demo MVP has static buildings)
- UAsset binaries for `IMC_ArchSimDefault` + 5 IA assets (project-author task per `docs/INPUT_MAPPING.md`)

---

## 2. Sprint S-02 retrospective (the units that landed)

| Order | Unit | Title | Status | Tag |
|---|---|---|---|---|
| 1 | AS-02a | UArchSimGameInstance skeleton + FTickableGameObject + Config | ✅ CLEAN | v0.1.4 (bundled) |
| 2 | AS-10 | Real PendingRankAccumulation ceiling test + 3 telemetry getters | ✅ NITS (closeout in v0.1.4) | v0.1.4 |
| 3 | AS-02b | Tick driver loop (registered-count delta → RequestSolve) | ✅ CLEAN | v0.1.5 (bundled) |
| 4 | AS-02c | Headless smoke test ArchSim.Integration.TickDriver | ✅ CLEAN | v0.1.5 |
| 5 | AS-03a | AArchSimCharacter subclass AAlsCharacter | ✅ CLEAN | v0.2.0 (bundled) |
| 6 | AS-03b | Enhanced Input mapping + IMC/IA specs + INPUT_MAPPING.md | ✅ CLEAN (3 LOW NITS, 1 → AS-14) | v0.2.0 (bundled) |
| 7 | AS-03c | Camera + ArchSimGameMode wire | ✅ NITS (1 fixed inline, 1 dismissed) | v0.2.0 (bundled) |
| 8 | AS-03d | Character + GameMode CDO/reflection smoke test | ✅ CLEAN (2 LOW NITS) | **v0.2.0 (this)** |

Total: 8 dispatches / 8 adversarial reviews / 6 CLEAN + 2 NITS / 0 BLOCKER /
0 ESCALATE / 4 backlog items opened.

---

## 3. AS-03 narrative (a / b / c / d)

### AS-03a — Class foundation

`AArchSimCharacter : AAlsCharacter`. Constructor sets `bUseControllerRotation*` 
all to false (so controller-driven rotation doesn't fight the ALS state
machine). `BeginPlay` / `EndPlay` log via `LogArchSim` (the module-wide
category from `ArchSimGameInstance.h`). `SetupPlayerInputComponent` is a
Super-only stub at this stage. `Source/ArchSim.Build.cs` gains `"ALS"` as a
PublicDependencyModuleName because the public header includes
`AlsCharacter.h`.

### AS-03b — Enhanced Input

5 `TObjectPtr<UInputAction>` + 1 `TObjectPtr<UInputMappingContext>`
`UPROPERTY(EditAnywhere, BlueprintReadOnly)` slots. BeginPlay pushes the
default IMC into `UEnhancedInputLocalPlayerSubsystem` (null-guarded with
Warning). `SetupPlayerInputComponent` casts to `UEnhancedInputComponent`
(null-guarded), then 7 `BindAction` calls each guarded by `IsValid(IA_*)`.

7 handlers:
- `HandleMove` — **view-space** movement via `GetPlayerViewPoint` →
  `UAlsVector::AngleToDirectionXY` → `AddMovementInput`. Matches
  `AAlsCharacterExample::Input_OnMove` line-by-line. Camera-relative (not
  actor-relative) movement is the correct third-person feel.
- `HandleLook` — `AddControllerYawInput` / `AddControllerPitchInput`.
- `HandleJumpPressed` / `HandleJumpReleased` — `Jump()` / `StopJumping()`.
- `HandleSprintPressed` / `HandleSprintReleased` — `SetDesiredGait` to
  `Sprinting` / `Running` (ALS GameplayTag namespace).
- `HandleCrouchToggle` — `SetDesiredStance` toggles `Standing` ↔ `Crouching`
  based on current desired-stance read.

`docs/INPUT_MAPPING.md` documents the 6 UAssets the project author creates in
the UE Editor (`IMC_ArchSimDefault` + 5 IA files), the key mappings, and the
BP assignment steps.

### AS-03c — Camera + GameMode

`UAlsCameraComponent Camera` default subobject attached to the skeletal mesh
(ALS camera reads `GetParentComponent()` for pivot/eye location).
`SetRelativeRotation_Direct({0, 90, 0})` matches the
`AAlsCharacterExample::AlsCharacterExample` precedent verbatim (Yaw=90 only;
no roll drift). An earlier draft of this unit used
`SetRelativeRotation({0, 90, -90})` based on the incorrect claim that
`_Direct` wasn't available on the skeletal-mesh parent chain; the Phase 3
adversarial reviewer caught both the false-API claim and the Roll-value
drift, and the inline fix landed before commit.

`AArchSimGameMode : AGameModeBase` (not the older `AGameMode` with
`MatchState` lifecycle — single-player simulator has no match phases).
Constructor sets `DefaultPawnClass = AArchSimCharacter::StaticClass()`.
`Config/DefaultEngine.ini` gains `GlobalDefaultGameMode=
/Script/ArchSim.ArchSimGameMode` in the existing `[GameMapsSettings]`
section (next to AS-02a's `GameInstanceClass`).

### AS-03d — Headless CDO/reflection smoke + v0.2.0

`Source/ArchSim/Private/Tests/ArchSimCharacterTest.cpp` with 7 logical
sub-checks (24 `TestX` assertions):
- Class hierarchy: `AArchSimCharacter` derives from `AAlsCharacter` /
  `ACharacter` / `APawn` via `IsChildOf`.
- `AArchSimGameMode` derives from `AGameModeBase`.
- `GameMode CDO DefaultPawnClass == AArchSimCharacter::StaticClass()` (using
  `TestTrue(lhs == rhs)` workaround because `TestEqual` ambiguates between
  `TSubclassOf<APawn>` and `UClass*`).
- AS-03a's `bUseControllerRotationYaw/Pitch/Roll` all false on the
  character CDO.
- AS-03c's `UAlsCameraComponent` default subobject exists and is named
  `"Camera"`.
- AS-03b's 6 `UPROPERTY` slots (1 IMC + 5 IA) are nullptr in the CDO (BP
  authoring assigns later from the UAssets).
- `LogArchSim` symbol resolves at link time + reflection class names match.

`Scripts/run_gate.ps1` line 29 bumps `$ExpectedUeTests` 139 → 140 (non-cuDSS
fallback 137 → 138).

### Honest headless limitation (re-applies the AS-07 / AS-10 / AS-02c pattern)

In `-nullrhi -unattended` automation, there is no live `UEnhancedInputLocalPlayerSubsystem`,
no `UWorld` running ALS tick, no `UAlsCameraComponent::TickCamera` execution,
and no `AddMovementInput` consumer. The headless smoke test therefore verifies
the static surface (class hierarchy, default UPROPERTY values, GameMode
wire), and the full input-+-locomotion driver is queued as **AS-13** (the
PIE-world fixture that also unblocks AS-10's trip path observability and
AS-02c's driver-loop branch).

---

## 4. NITS / new backlog from Sprint S-02

| ID | Title | Origin | Severity |
|---|---|---|---|
| AS-11 | Header comment precision for rebaseline reset points | v0.1.4 AS-10 NITS-02 | LOW |
| AS-12 | `GetMaxRankBeforeRebaseline()` production consumer | v0.1.4 AS-10 NITS-03 | LOW |
| AS-13 | PIE-world fixture for driver-loop + trip-path + input runtime | v0.1.5 AS-02c + AS-10 + v0.2.0 AS-03d | MEDIUM |
| AS-14 | Analog stick / gamepad `ClampMagnitude012D` normalization in HandleMove | v0.2.0 AS-03b NITS-01 | LOW |

None block the v0.2.0 ship; AS-13 is the most load-bearing and should head
the Sprint S-03 backlog.

---

## 5. 5-leg gate evidence (AS-03d run, immediately before tag)

### Reproduce (cuDSS host)

```powershell
# Prerequisite: conda env active (run_gate.ps1 leg 3 needs OpenSeesPy)
conda activate framecore-direct       # or set $env:SUPERNODAL_CONDA

# Optional: set $env:UE_ENGINE_ROOT to your UE 5.7 install dir if not already set
# $env:UE_ENGINE_ROOT = "<path-to>\UE_5.7"

# Then from the repo root:
.\Scripts\run_gate.ps1 -RequireOpenSees
```

### Reproduce (non-cuDSS host)

```powershell
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 138
```

(F67 / F67s are CUDA-only and compile out when `FRAMECORE_CUDA=0` → 2 fewer tests.)

### Expected output (verbatim from the AS-03d run)

```
[1/5] standalone FrameCore gate (build.bat)...
       standalone: ALL PASS  (failures=0) (exit 0)
[2/5] UE headless automation...
       UE automation: 140 tests run, exit code 0 (process exit 0; expected >= 140)
[3/5] OpenSees offline cross-validation...
       OpenSees compare: PASS (exit 0)
[4/5] linear-analysis deep audit...
       linear deep audit: PASS failures=0 checks=104 (exit 0)
[5/5] CLI round-trip (frame_cli J1 bridge)...
       CLI round-trip: ALL PASS  (failures=0) (exit 0)
======================================================
 GATE: PASS  (standalone OK, UE 140 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK)
```

---

## 6. Process narrative (Sprint S-02 driven via /work)

8 dispatch units in one session, 2 patch tags (v0.1.4, v0.1.5) along the way,
final minor tag (v0.2.0) at AS-03d close. Each unit went through:

1. Phase 2 dispatch with composed prompt (iron rules + top-tier discipline + domain SUBAGENT_PREFIX content injected verbatim + unit spec + verification commands + ESCALATE protocol)
2. Phase 3 adversarial review (read-only general-purpose agent with file:line evidence requirement)
3. Phase 4 lightweight commit (no tag) for production-only units, OR full release ceremony for tag-bearing units

Token use (output, subagent-side, approximate):
- AS-02a 114K / AS-10 147K / AS-02b 119K / AS-02c 129K / AS-03a 97K / AS-03b 104K / AS-03c 122K / AS-03d 112K = **~944K subagent output across 8 dispatches**
- Plus 8 adversarial reviews ranging 84K-106K = **~750K review output**
- Plus main-thread orchestration (state files, agent logs, manager.md appends, release docs)

Wall-clock from session open (Phase 0 ~10:33) to v0.2.0 tag (~13:48) ≈ 3h 15min.

Sprint logs preserved under `docs/logs/S-02/`:
- [`scope_2026-06-26T1033.md`](logs/S-02/scope_2026-06-26T1033.md)
- [`plan_2026-06-26T1033.md`](logs/S-02/plan_2026-06-26T1033.md)
- [`agent_AS-02a.md`](logs/S-02/agent_AS-02a.md) through [`agent_AS-03d.md`](logs/S-02/agent_AS-03d.md)
- [`manager.md`](logs/S-02/manager.md)

---

## 7. Continuation pointer

- **Handoff:** [`docs/HANDOFF_v0.2.0.md`](HANDOFF_v0.2.0.md) — Z-01 first action for Sprint S-03 (AS-13 PIE fixture as the top priority)
- **Architecture index:** [`docs/ARCHITECTURE_INDEX.md`](ARCHITECTURE_INDEX.md) — Latest tag now v0.2.0; ArchSimCharacter + ArchSimGameMode in production class map
- **Prior anchors:** [`docs/RELEASE_v0.1.5.md`](RELEASE_v0.1.5.md) | [`docs/RELEASE_v0.1.4.md`](RELEASE_v0.1.4.md) | [`docs/RELEASE_v0.1.3.md`](RELEASE_v0.1.3.md)
