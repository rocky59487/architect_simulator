# v0.4.0 — Scenario MVP minor bump (S-05)

**Sprint:** S-05 close
**Date:** 2026-06-27
**Repo:** `architect_simulator` (game-body line)
**Baseline tag:** `v0.3.1` (`994be68`)
**HEAD at tag time:** release-hardening commit (see tag plan below)

> **Headline.** First **minor** bump on the game-body line since `v0.2.0`.
> Closes the S-04 deferred backlog (AS-25 hook regex broaden, AS-26
> `ClassWithin` mirror, AS-27 cosmetic doc/comment cleanup) and ships the
> three-unit Scenario MVP spike: K-set (K1/K2/K4) placement via a new
> Editor Utility Widget, full Registry → Solve → Heatmap wire, and a
> non-modal Tutorial state machine. Includes a UE5.8 install-availability
> evaluation document (NO-GO; no UE5.8 install detected on the integrator
> host). Engine source delta = **0** under both FROZEN paths (FrameCore
> `v4.0.0` + LevelSim `v1` both honoured). UE automation test count
> 145 → **148** (cuDSS) / 143 → **146** (non-cuDSS).

---

## 1. What landed

### 1a. Closed S-04 deferred backlog (Path B)

| AS-XX | Commit | Title | LOC | Notes |
|---|---|---|---|---|
| **AS-25** | (no in-repo commit) | Hook regex broaden for `S-XXa` suffix sprints | hook file `~/.claude/hooks/work-phase-guard.ps1` only | Ceremonial OUTSIDE-repo: foreign-state content-sniff regex `^S-\d+$` → `^S-[\w]+$` plus a WHY comment block. 4-scenario stdin test PASS including the new `S-04a`-suffix scenario the old regex would have fail-open'd. ArchSim repo 0 lines touched |
| **AS-26** | `26153c3` | `UArchSimModelRegistry` `ClassWithin` verify + `ArchSimPieHarness::GetOrCreateModelRegistry` `NewObject` outer mirror | production +1 line code + 9 lines WHY comment at `ArchSimPieHarness.cpp:81` | Mirrors the S-04 AS-24-u1 pattern. Reviewer independently verified `UE_5.7\Engine\Source\Runtime\Engine\Public\Subsystems\GameInstanceSubsystem.h:15` shows the explicit `UCLASS(Abstract, Within = GameInstance, MinimalAPI)` macro (the plan-stated hypothesis said "implicit"; honest correction noted in code comment + Phase 3 closeout). `NewObject<T>()` default outer IS already `GetTransientPackage()` per `UE_5.7\…\UObject\UObjectGlobals.h:1919` default arg — fix value is intent-documentation + parity with AS-24 |
| **AS-27** | `21a06d9` | Stale doc references in ARCH_INDEX §8 + `ArchSimPieDriverLoopTest` empirical comments | docs §8 +1 line / test 7 lines | (a) `docs/ARCHITECTURE_INDEX.md` §8 gate cheat-sheet `140 expected / 138 on non-cuDSS` → `145 expected / 143 on non-cuDSS`. (b) `ArchSimPieDriverLoopTest.cpp` L54-56 + L59 empirical phrasing mirroring NIT-a precedent at `ArchSimPieHarness.h:52` (`"consistently provides ... in our verified test runs"` instead of `"always has"` / `"always provides"`). No logic change |

### 1b. Scenario MVP spike (Path A) — three sequential units

| Unit | Commit | Title | LOC | Notes |
|---|---|---|---|---|
| **SPIKE-Scenario-u1** | `ea6ce65` | Editor Utility Widget skeleton + K1 column placement | +302 production / +120 test / +17 Build.cs / +2 gate config | `UArchSimScenarioWidget : UEditorUtilityWidget` wrapped in `#if WITH_EDITOR`. `PlaceK1Column(FVector)` BlueprintCallable spawns an `AActor` placeholder, attaches a `UArchSimMemberData` with default 1 m beam geometry, registers it with `UArchSimModelRegistry::RegisterMember`. `ArchSim.Build.cs` adds `if (Target.Type == TargetType.Editor)` block with `Blutility / UMG / UMGEditor / UnrealEd` private deps. New headless smoke test `ArchSim.Gameplay.ScenarioWidget` (7 sub-checks; CDO + reflection + BP-callable signature). **Honest gotcha during dispatch**: subagent removed `EditorScriptingUtilities` from the Build.cs block (it required a `.uproject` Plugins entry that iron rule #5 forbids touching, and `PlaceK1Column` does not actually use any `UEditorActorUtilities` API); added `UnrealEd` for `GEditor`/`GetEditorWorldContext` (LNK2019 fix); removed `MinimalAPI` from the UCLASS macro (UE5 mutually exclusive with `ARCHSIM_API`). `$ExpectedUeTests` 145 → 146 (cuDSS) / 143 → 144 (non-cuDSS) |
| **SPIKE-Scenario-u2** | `b2204e3` | Wire Registry → Solve → Heatmap visualization | +197 production / +220 test / +5 gate config | `RequestSolveAndVisualize()` BlueprintCallable triggers the debounced solve via `UArchSimModelRegistry::RequestSolve(FFrameModelPatch{})` and lazy-spawns an `AFrameUtilizationHeatmapActor` in the PIE world when the `OnSolveComplete` delegate fires. PIE world preferred via `GEditor->PlayWorld`; falls back to Editor world; graceful-fail with `LogArchSim Warning "Enter PIE first"` + `return false` when no Registry. `HeatmapActor` UPROPERTY wrapped in `#if WITH_EDITORONLY_DATA` per UHT 5.4+ best practice. `BeginDestroy` explicitly calls `Registry->OnSolveComplete.Remove(Handle)` (cached via a new `TWeakObjectPtr<UArchSimModelRegistry> SubscribedRegistry` member; per Phase 3 reviewer Finding #1 — `AddUObject`'s implicit weak-object protection is implementation detail not API contract). Member geometry assembled via file-scope-static `BuildMemberGeometryFromRegistry` (avoids transitive include pollution to unity TUs). New headless smoke test `ArchSim.Gameplay.ScenarioSolveWire` (7 sub-checks including `RequestSolveAndVisualize` UFunction reflection, `HeatmapActor` UPROPERTY type check, graceful-fail without Registry). Live PIE-world solve→delegate→heatmap chain deferred to u3 PIE fixture (AS-13 precedent). `$ExpectedUeTests` 146 → 147 / 144 → 145 |
| **SPIKE-Scenario-u3** | `aa6fd71` | K2+K4 placement + tutorial overlay + reload smoke + PIE 5min smoke doc | +230 .h / +280 .cpp / +310 test / +2 gate config / +280 doc | `PlaceK2Beam(FVector)` (2 m horizontal beam along +X; offsets ±100,0,0 cm) + `PlaceK4Brace(FVector)` (2 m diagonal at 45° in XZ plane; offsets ±71,0,±71 cm; hypotenuse ≈ 200.8 cm) factor through a new private `PlaceKSetMember` shared helper so K1/K2/K4 share the SpawnActor → MemberData → RegisterMember sequence. `EArchSimTutorialState` UENUM with 6 states (Welcome → PromptPlaceK1 → PromptPlaceK2 → PromptPlaceK4 → PromptPressTest → FreeExplore-terminal). `AdvanceTutorialStep()` linear transition + fires `OnTutorialStateChanged` + `OnVoicePromptShouldPlay` BlueprintImplementableEvent (text-only — no third-party TTS SDK linked in C++). `GetCurrentPromptText()` BlueprintPure FText accessor (LOCTEXT namespace `ArchSimTutorial`). `ResetWidgetState()` unsubscribes delegate + destroys HeatmapActor if valid + clears the new `PlacedActors` TArray reference list + resets state to Welcome — does NOT destroy K-set actors (student-owned data; intentional). New headless smoke test `ArchSim.Gameplay.ScenarioTutorial` (8 sub-checks). New USER-DRIVEN doc `docs/logs/S-05/u3_pie_smoke.md` with 9 sections + 16 specific PASS criteria + FAIL recovery triage table — the v0.4.0 hard gate. `$ExpectedUeTests` 147 → 148 / 145 → 146. **Build error during integration**: subagent guessed at a UE5 reflection constant `FUNC_BlueprintImplementableEvent` that does not exist in UE5.7 (only `FUNC_BlueprintEvent` exists, covering both Implementable and NativeEvent per `UE_5.7\…\UObject\Script.h:163`); main thread inline-fixed L306+L325 of the test file + the assertion text |

### 1c. UE5.8 install-availability evaluation (Z-01 spike, NO-GO)

| Unit | Commit | Title | Files | Notes |
|---|---|---|---|---|
| **SPIKE-UE5.8-eval** | `6af889a` | UE5.8 install detection + 4-plugin compat eval | `docs/logs/S-05/ue58_eval.md` + sandbox `Research/ue58_attempt/README.md` (kept untracked) | Phase A early-return: UE5.8 not installed on the integrator host. 4 candidate install paths probed via `Test-Path` + Windows registry enumerated (`HKLM\SOFTWARE\EpicGames\Unreal Engine` + 32-bit `Wow6432Node` mirror) — only UE 4.0 and UE 5.7 keys present. Phase B (sandbox plugin build) skipped per scope contract honest-fail. Static `.uplugin` / `Build.cs` analysis of the 4 external plugins (ALS / Prefabricator / SPUD / SUQS): SPUD flagged CONDITIONAL-HIGH-RISK because `SPUD.Build.cs:35` lists `StructUtils` in `PrivateDependencyModuleNames` and `StructUtils.uplugin` carries `"DeprecatedEngineVersion": "5.5"` — Epic may remove the Experimental module entirely in UE5.8 leaving only the CoreUObject path. **Include-side mitigation already in SPUD source**: `SpudPropertyUtil.cpp:7` + `TestSaveObject.h:7` carry an `#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5` guard so the `#include "StructUtils/InstancedStruct.h"` resolves from CoreUObject when the Experimental plugin disappears. Remaining risk is purely the `Build.cs` module dep (see § 4 below). Decision doc spells out S-06 first action with PowerShell scripts and a minimal `UE58Probe.uproject` template |

### 1d. Numbers and counts

| Metric | v0.3.1 | v0.4.0 | Δ |
|---|---|---|---|
| UE automation tests (cuDSS) | 145 | **148** | +3 (`ScenarioWidget` + `ScenarioSolveWire` + `ScenarioTutorial`) |
| UE automation tests (non-cuDSS) | 143 | **146** | +3 |
| Standalone FrameCore F-fixtures | F1..F71 | F1..F71 | 0 (FROZEN engine) |
| Linear deep audit checks | 104 | 104 | 0 |
| OpenSees compare oracle | strict | strict | 0 |
| ArchSim test files | 9 | **12** | +3 (`ArchSimScenarioWidgetTest.cpp` + `ArchSimScenarioSolveWireTest.cpp` + `ArchSimScenarioTutorialTest.cpp`) |
| ArchSim production classes | 5 (`UArchSimMemberData`/`UArchSimModelRegistry`/`UArchSimGameInstance`/`AArchSimCharacter`/`AArchSimGameMode`) | **6** | +1 (`UArchSimScenarioWidget` `#if WITH_EDITOR`-guarded Editor Utility Widget) |
| FrameCore engine source files modified | 0 | 0 | 0 (FROZEN honoured) |
| LevelSim source files modified | 0 | 0 | 0 (FROZEN honoured) |
| External plugin (ALS / Prefabricator / SPUD / SUQS) source files modified | 0 | 0 | 0 (READ-only per scope anti-goal) |
| Build.cs Editor-only block | (none) | `if (Target.Type == TargetType.Editor)` with Blutility / UMG / UMGEditor / UnrealEd | NEW (gated; no packaged-shipping bloat) |

Cumulative since `v0.3.1` (`994be68`): **23 files in-repo / +6260 / -11**.

---

## 2. How to reproduce the verification

```powershell
# Pre-req
$env:UE_ENGINE_ROOT          # points to UE 5.7 install root (e.g. E:\project\UE_5.7)
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"
# openseespy lives in system Python (Windows Store 3.12), pip install openseespy

# UE editor incremental build
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# 5-leg gate (cuDSS host: 148 / non-cuDSS: 146)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS, 148 tests, exit 0

# Non-cuDSS fallback
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 146

# Isolated Scenario suite (3 tests; useful for u1/u2/u3 surface only)
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Gameplay.Scenario; Quit" `
    -unattended -nullrhi -log
# Expect: Result={成功} for ScenarioWidget + ScenarioSolveWire + ScenarioTutorial + EXIT CODE: 0
```

---

## 3. Verification matrix

| Leg | Status | What ran | Reproduction command |
|---|---|---|---|
| **[1/5] standalone FrameCore F1..F71** | ✅ PASS (failures=0, exit 0) | `Plugins\FrameSolver\Standalone\build.bat` + `frametest.exe` | `Scripts\run_gate.ps1 -RequireOpenSees` leg 1 OR `Plugins\FrameSolver\Standalone\build.bat` direct |
| **[2/5] UE headless automation** | ✅ PASS (148 / 148, exit 0) | `UnrealEditor-Cmd.exe` running all `FrameCore.UE.*` + `FrameCore.*` + `ArchSim.*` namespaces | `Scripts\run_gate.ps1 -RequireOpenSees` leg 2 |
| **[3/5] OpenSees offline cross-validation** | ✅ PASS (exit 0) | `Tools\opensees_compare.py` strict mode (ours-vs-OpenSees on shallow-arch + cantilever fixtures) | `Scripts\run_gate.ps1 -RequireOpenSees` leg 3 |
| **[4/5] linear deep audit** | ✅ PASS (failures=0, checks=104, exit 0) | `linear_deep_audit.exe` (104 cross-checks against dense independent solver) | `Scripts\run_gate.ps1 -RequireOpenSees` leg 4 |
| **[5/5] CLI round-trip** | ✅ PASS (failures=0, exit 0) | `Tools\cli_roundtrip.py` (frame_cli J1 bridge round-trip) | `Scripts\run_gate.ps1 -RequireOpenSees` leg 5 |
| **Isolated Scenario suite** | ✅ PASS (3 tests, exit 0) | `ArchSim.Gameplay.ScenarioWidget` + `ScenarioSolveWire` + `ScenarioTutorial` | command in § 2 above |
| **PIE 5-minute student trial smoke** (v0.4.0 hard gate) | ⏳ DEFERRED to USER per `docs/logs/S-05/u3_pie_smoke.md` | Real PIE session: tutorial → free explore → 5 min unattended → close cleanly | Open UE Editor → Editor Utility Widget BP child → PIE → follow `docs/logs/S-05/u3_pie_smoke.md` 9-section flow |
| **CUDA / cuDSS GPU gate** | ⏳ NOT RUN this release | `Scripts\run_gpu_gate.ps1 -Strict` (F67s strict + r2_bench --gpu 90k margin) | Reproduction same as v0.3.0 / v0.3.1 (FrameCore source unchanged; v3.0.0 / v3.1.0 GPU evidence carries forward) |
| **6th leg D2/D4 exit-tests** | ⏳ NOT RUN this release | `Scripts\run_exit_tests.ps1` D1 property / D3 strict-mode oracle | Inherits v3.6.0 / v4.0.0 carry-forward; engine source unchanged this release |

---

## 4. Deferred to v0.4.x / S-06

The "First action on day 1" for each item lives in [`docs/HANDOFF_v0.4.0.md`](HANDOFF_v0.4.0.md) §4.

| ID | Title | Why deferred |
|---|---|---|
| **AS-28** | Hook case-sensitivity + `.bak` header comment sync (LOW; OUTSIDE repo) | PowerShell `-notmatch` at `~/.claude/hooks/work-phase-guard.ps1` L104 is case-insensitive by default so `s-04a` lowercase currently matches `^S-[\w]+$`. Current `/work` convention is all-uppercase so no real impact; bundle with a future hook maintenance window. Origin: S-05 AS-25-u1 review (Findings #1 + #2) |
| **AS-29** | `run_gate.ps1` standalone leg PowerShell environment race diagnosis (LOW) | AS-27-u1 subagent observed `[1/5] standalone: exit 1` under PowerShell session while direct `build.bat` yields `ALL PASS`; AS-26-u1 subagent on the same host got `[1/5] standalone: ALL PASS`. Likely shell-state / cwd / PATH race during parallel dispatches. Workaround documented (direct `build.bat`). Origin: S-05 AS-27-u1 review (Finding #3) |
| **Z-01-PhaseB** | UE5.8 sandbox plugin build (resume Phase B from `docs/logs/S-05/ue58_eval.md` §5 once UE5.8 installed) | Phase A early-return (NO-GO; UE5.8 not installed). S-06 first action: install UE5.8 via Epic Games Launcher to `E:\project\UE_5.8\` and re-run Phase B sandbox per `ue58_eval.md` §5 |
| **SPUD `Build.cs` `StructUtils` dep removal** (UE5.8 only) | Block to UE5.8 upgrade once install detected | `SPUD.Build.cs:35` `PrivateDependencyModuleNames` `"StructUtils"` entry; UE5.8 may remove the Experimental plugin entirely (`"DeprecatedEngineVersion": "5.5"`). Include side already mitigated by SPUD's `ENGINE_MINOR_VERSION >= 5` guards. Fix is sandbox-copy-only until upgrade is GO; main `Plugins/SPUD/` stays UE5.7-compatible |
| **PIE 5-minute student trial smoke (real-world adjudication)** | v0.4.0 hard gate proxy was the headless gate + reviewer assessment of `u3_pie_smoke.md` actionability | User authorized the proxy at release time; the actual PIE smoke can be run any time post-publish per `docs/logs/S-05/u3_pie_smoke.md`. If it FAILs post-publish, route via the post-publish hotfix protocol (mark `v0.4.0` prerelease + ship `v0.4.0.1` hotfix or revert to `v0.3.2`-style patch path) |
| **AS-04** | Plugins panel visual confirmation (human-side, ~30 min) | Pre-S-05 carry; no S-05 commitment |
| **AS-05** | K1-T2 / K4 art assets (parallel human-side) | Pre-S-05 carry; placeholders OK at v0.4.0 (K-set elements spawn as plain `AActor` with default `UStaticMeshComponent` and no mesh asset; BP child class assigns the mesh in the Details panel) |
| **AS-06** | SPUD UE5.5 `StructUtils` deprecation handling (couples to Z-01) | Pre-S-05 carry; closely tied to the Z-01-PhaseB item above |
| **AS-08** | SPUD `RF_Transient` audit (when wiring SPUD orchestration) | Pre-S-05 carry; not needed for v0.4.0 |
| **AS-09** | Non-cuDSS host re-verify (opportunistic) | Pre-S-05 carry; no host change |

---

## 5. Honest limitations

- **Scenario MVP requires PIE** — `UArchSimModelRegistry` is a `UGameInstanceSubsystem`. The `RequestSolveAndVisualize()` BP-callable graceful-fails with a Warning log when no PIE is active. The K-set placement methods (`PlaceK1Column` / `PlaceK2Beam` / `PlaceK4Brace`) operate on the Editor world directly, but member registration is deferred to the next `BeginPlay` if Registry is null at placement time.
- **Tutorial overlay UI lives in BP** — the `OnTutorialStateChanged` + `OnVoicePromptShouldPlay` `BlueprintImplementableEvent` stubs let the BP child render the prompt overlay and (optionally) wire any BP-callable TTS node. C++ ships only the state machine and the prompt text; no third-party TTS SDK is linked.
- **`UCLASS(Abstract)` on `UArchSimScenarioWidget`** — instantiation is intentional via a Blueprint child class. Headless smoke tests `NewObject<UArchSimScenarioWidget>()` on the Abstract class log a benign `LogUObjectGlobals` warning but proceed; this is by design for CDO/reflection coverage.
- **`HeatmapActor` lifecycle** — lazy-spawned on first `OnSolveComplete` in the PIE world; cleared by `ResetWidgetState()` or by PIE world teardown. Across multiple PIE sessions, a stale `TObjectPtr` may briefly persist until UE GC; the `IsValid()` guard in the lazy-spawn path handles this correctly.
- **PIE 5-minute smoke is USER-driven** — `docs/logs/S-05/u3_pie_smoke.md` is the actionable spec; headless commandlet cannot exercise live input + tutorial walkthrough + 5-minute unattended PIE. The v0.4.0 release uses the headless gate + reviewer doc-actionability assessment as the proxy; the real smoke can be run any time post-publish.

---

## 6. Breaking changes

**None.** The 6 commits since `v0.3.1` are all additive:
- AS-26 mirrors an existing test-fixture pattern (no production API change)
- AS-27 is docs + comments only
- u1/u2/u3 add new BP-callable methods + a new `UCLASS(Abstract)` widget; no existing
  `UArchSimMemberData` / `UArchSimModelRegistry` / `UArchSimGameInstance` / `AArchSimCharacter` /
  `AArchSimGameMode` API was modified
- UE5.8 eval is sandbox-only + decision doc
- ArchSim FRAMECORE_API / FrameCoreUE public USTRUCT layout is unchanged (engine FROZEN)

---

## 7. Tag plan

```bash
# Release-hardening commits the 4 release-time artefacts via this skill's Phase 5
git add docs/RELEASE_v0.4.0.md docs/HANDOFF_v0.4.0.md docs/ARCHITECTURE_INDEX.md docs/logs/S-05/manager.md
git commit -m "release: v0.4.0 -- Scenario MVP minor bump (Sprint S-05 close)"

# Annotated tag pinned at the release commit
git tag -a "v0.4.0" -m "v0.4.0 -- Scenario MVP minor bump"

# (USER ACTION) After review, push branch then tag
git push origin main
git push origin v0.4.0

# (USER ACTION) GitHub release
gh release create v0.4.0 \
    --title "v0.4.0 — Scenario MVP minor bump (Sprint S-05 close)" \
    --notes-file docs/RELEASE_v0.4.0.md
```

Local tag only; no remote push or `gh release create` is performed by the
release-hardening skill — those are explicit USER actions per the project's
no-push-without-authorization rule.

---

## 8. Acknowledgements

S-05 ran the full `/work` 7-phase chain end-to-end across two sessions
(Anthropic session-limit hit during SPIKE-Scenario-u2; recovered with
main-thread substitute-oracle verification + Phase 3 reviewer agreeing the
working-tree code was substantively complete). 7 adversarial reviews ran
across the sprint (4 in parallel for Round 1; one each for u1/u2/u3 in
Rounds 2-4). NITS-with-inline-fix outcomes across the spike units;
zero BLOCKER cycles.
