# v0.3.0 — Hardening + PIE-world test harness foundation

**Sprint:** S-03 close
**Date:** 2026-06-26
**Repo:** `architect_simulator` (game-body line)
**Baseline tag:** `v0.2.0` (`58705d0`)
**HEAD at tag time:** release-hardening commit (see tag plan below)

> **Headline.** Closes five v0.2.0 hardening audit findings (AS-15 Enhanced Input
> lifecycle, AS-16 CalcCamera override, AS-17 empty-model audit, AS-11/12/14/18/19
> LOW cleanup batch) and fully unblocks the previously deferred AS-13 PIE-world
> fixture (u1 helper namespace + u2 three new harness-based tests). **Engine
> source delta = 0** across the entire sprint; FrameCore v4.0.0 FROZEN and
> LevelSim v1 FROZEN both honoured. UE automation test count 140 → 145 (cuDSS)
> / 138 → 143 (non-cuDSS).

---

## 1. What landed

### 1a. Closed v0.2.0 hardening audit findings

| AS-XX | Commit | Title | Production LOC | Notes |
|---|---|---|---|---|
| **AS-11** | `8c6d14a` | Rebaseline header comment line-ref precision | comment-only | 6 stale `cpp:NNN` cites rewritten to stable form (`see RequestSolve body` / `see ExecuteSolve top + 3 early-exit paths`) to avoid future drift |
| **AS-12** | `8c6d14a` | `GetMaxRankBeforeRebaseline()` production consumer plan | comment-only | Backlog'd TODO comment added; HUD rank-budget indicator is the intended caller, out of S-03 scope |
| **AS-14** | `8c6d14a` | Analog stick / gamepad `ClampMagnitude012D` clamp | `ArchSimCharacter.cpp` `HandleMove` +1 line | Wraps `Value.Get<FVector2D>()` in `UAlsVector::ClampMagnitude012D`; ALS API signature verified via 3-point grep |
| **AS-15** | `6a8e97a` | Enhanced Input lifecycle refit | `ArchSimCharacter.{h,cpp}` net +50 | `NotifyControllerChanged` override + `RemoveMappingContext` on previous controller + `AddMappingContext` with `FModifyContextOptions::bNotifyUserSettings=true` on new controller; `Canceled` event bindings on Move/Look/Sprint/Jump (not Crouch — toggle protection). Bundles A-02/D-01/D-02/D-03/D-06. Mirrors `AlsCharacterExample.cpp:19-49` |
| **AS-16** | `8ca4008` | `CalcCamera` override for ALS camera pipeline | `ArchSimCharacter.{h,cpp}` +8 code (+42 LOC with comments) | Routes through `UAlsCameraComponent::GetViewInfo` per ALS `L51-60`; `IsValid(Camera) && Camera->IsActive()` defensive prefix; Super fallback otherwise |
| **AS-17** | `7eeb77b` | empty-`CurrentModel` `StartSession` audit | 0 production (Case A: no guard needed) | Engine `FrameModel::validate()` returns `false "no nodes"` → diagnostic `"invalid model: no nodes"` → existing `if (!Session->valid())` guard at `FrameInteractiveSubsystem.cpp:81-88` cleans up both Session + Cached pointers and returns false. New test pins the contract |
| **AS-18** | `8c6d14a` | Two-GameInstanceSubsystem teardown order doc | docs/ARCHITECTURE_INDEX.md ~+30 lines | Documents why both teardown directions (Registry first vs Sub first) are race-safe via `EndSession` idempotency + `GetFrameSubsystem` null guard. Closes hardening C-04 |
| **AS-19** | `8c6d14a` | `UArchSimMemberData::BeginPlay` early-out warn/retry | `ArchSimMemberData.cpp` +9 lines | Option A warn-only (`UE_LOG(LogTemp, Warning, ...)`); Option B retry-via-timer rejected at 35-45 LOC > 30 LOC threshold |

### 1b. Closed previously deferred backlog

| AS-XX | Commit | Title | LOC | Notes |
|---|---|---|---|---|
| **AS-13-u1** | `f82f590` | PIE-world bootstrap test harness | +315 (test-only) | New `namespace ArchSimPieHarness` with five helpers (`GetOrFindWorld`, `GetOrFindGameInstance`, `GetOrCreateModelRegistry`, `IsRegistryFromRealGI`, `SpawnActor<T>`). Three-level honest contract (Level 1 real UArchSimGameInstance attached / Level 2 generic GameInstance / Level 3 no GameInstance — NewObject fallback). Uses the proven `GEngine->GetWorldContexts()` pattern from `FrameCoreUEActorStressMeshTest.cpp:35-51` instead of the plan's `FAutomationEditorCommonUtils::CreateNewMap` (Editor-module-dep) or the HANDOFF_v0.2.0 §6 fallback `UWorld::CreateWorld(EWorldType::Game)` (documented as crashing in `-ExecCmds=Automation` commandlet). New `ArchSim.Integration.PieHarnessSmoke` (8 sub-checks) verifies the contract. Surprising bonus: `SpawnActor<AArchSimCharacter>(World)` succeeds in the commandlet world |
| **AS-13-u2** | `8c702a5` | 3 deferred test branches via PIE harness | +406 (test-only) | Three new automation tests honouring AS-07 lesson #1 honest defer: `ArchSim.Integration.PieRebaseline` (7 sub-checks — accumulator math + honest defer of trip-path), `ArchSim.Integration.PieDriverLoop` (7 sub-checks — World non-null + GI null in Level 3 + Tick safety + honest defer of driver-loop firing), `ArchSim.Gameplay.PieInputRuntime` (7 sub-checks — instance Camera != null genuine new coverage vs CDO null). `SetupPlayerInputComponent` + `NotifyControllerChanged` direct invocation removed at compile-time (`protected` virtual → C2248) and honestly deferred to true PIE |

### 1c. Numbers and counts

| Metric | v0.2.0 | v0.3.0 | Δ |
|---|---|---|---|
| UE automation tests (cuDSS) | 140 | **145** | +5 |
| UE automation tests (non-cuDSS) | 138 | **143** | +5 |
| Standalone FrameCore F-fixtures | F1..F71 | F1..F71 | 0 |
| Linear deep audit checks | 104 | 104 | 0 |
| OpenSees compare oracle | strict | strict | 0 |
| ArchSim test files | 4 | 9 | +5 |
| FrameCore engine source files modified | 0 | 0 | 0 (FROZEN honoured) |
| LevelSim source files modified | 0 | 0 | 0 (FROZEN honoured) |
| External plugin (ALS/Prefabricator/SPUD/SUQS) source files modified | 0 | 0 | 0 (read for precedent only) |

### 1d. Iron rule compliance audit

Verified via grep on `git diff v0.2.0..HEAD`:

```
git diff --stat v0.2.0..HEAD -- Plugins/FrameSolver/Source/FrameCore/ Plugins/LevelSim/Source/LevelCore/
→ 0 lines (FROZEN integrity CONFIRMED)

git diff --name-only v0.2.0..HEAD | grep -E '^Plugins/(ALS|Prefabricator|SPUD|SUQS)/'
→ no match (external plugins untouched)

git diff --name-only v0.2.0..HEAD | grep -E '\.gitignore$|ArchSim\.uproject$|\.dll$|\.exp$|\.lib$|\.exe$'
→ no match (rule #5 honoured)

git diff v0.2.0..HEAD -- '*.cpp' '*.h' | grep -iE 'FROZEN|avoid.*touch|workaround|so we don.t'
→ 0 hits (no behavior-level FROZEN fork)
```

No verbal-amendment-without-CLAUDE.md-edit cases this sprint.

---

## 2. Verification matrix

All legs run on the integrator host. Subagents re-ran the 5-leg gate after each
unit's commit (sequential mode from Round 2 onward, no race).

| Leg | What runs | Reproduce command | v0.3.0 status |
|---|---|---|---|
| 1 | Standalone FrameCore (F1..F71) | `Plugins/FrameSolver/Standalone/build.bat` (separate cmd or PowerShell) | ALL PASS (failures=0) |
| 2 | UE headless automation (145 cuDSS / 143 non-cuDSS) | see below | 145 / 145 PASS, exit 0 |
| 3 | OpenSees offline compare | `python Tools/opensees_compare.py --relaxed` (system Python — see §5 reproducibility) | PASS |
| 4 | Linear deep audit (104 checks) | `Plugins/FrameSolver/Standalone/build_linear_audit.bat` | PASS failures=0 checks=104 |
| 5 | CLI round-trip (J1 bridge) | `python Tools/cli_roundtrip.py` | ALL PASS (failures=0) |
| **Aggregate** | All five legs in one driver | `Scripts/run_gate.ps1 -RequireOpenSees` (see below for cuDSS host) | GATE: PASS |

### 2a. One-liner reproduce (cuDSS host)

```powershell
# Pre-req: %UE_ENGINE_ROOT% set to your UE 5.7 install root; conda env
# 'framecore-direct' must be on PATH for OpenBLAS/METIS DLLs (see §5)
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS  (UE 145 tests, standalone OK, OpenSees PASS,
#         deep audit 104, CLI round-trip OK)
```

### 2b. Non-cuDSS host (143 UE tests)

```powershell
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 143
```

The two skipped tests are the cuDSS-only GPU lane mirrors (F67 / F67s family).

### 2c. NOT RUN this release

None of the five legs were marked NOT RUN at v0.3.0. Every leg returned exit 0
on the integrator host with cuDSS available.

---

## 3. Known issues / cosmetic items (deferred to S-04 inline fix)

These are **not blockers** for v0.3.0; they fold cleanly into the next docs-sync
pass. Listed here so anyone reading the release notes can find them quickly.

- `Source/ArchSim/Private/Tests/ArchSimPieHarness.h:52-54` — docstring says
  the world is "always" present in commandlet mode. The impl is defensively
  correct (returns nullptr if `GEngine` is null or no context has a world);
  the word "always" overclaims the empirical observation as a contract.
- `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp:108-111` —
  Sub-check 4 uses `TestTrue(..., true)` as a positive marker that the
  `World->Tick` x3 didn't crash. Functionally correct but a tautology; a
  follow-up could `TestTrue(World->GetTimeSeconds() > t0)` instead.
- `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp:153` — the warn
  message in `NotifyControllerChanged` for a null `DefaultMappingContext`
  lacks the `(ArchSim|Input)` prefix that the other warns in this file use.
- `Scripts/run_gate.ps1` comment block — the AS-13-u1 history segment still
  mentions the intermediate 142 / 140 count; only the latest `=145` value is
  used at runtime, but the comment trail is one revision stale.
- `docs/ARCHITECTURE_INDEX.md` §5 data-flow figure — `PendingRankAccumulation < 96`
  reads ambiguously; the production trip condition is `> 96` so it fires at 97.
  Pre-existing since S-02.
- `docs/ARCHITECTURE_INDEX.md` §6 RebaselineCeiling row — still cites a `cpp:281`
  line number that drifted post-v0.2.0 hardening. AS-11 swept the header
  comments to stable form but did not also rewrite §6.

---

## 4. Deferred to S-04 (with first-action sketches in HANDOFF_v0.3.0.md §4)

| ID | Title | Why deferred | Audit origin |
|---|---|---|---|
| **AS-20** | Upgrade `ArchSimMemberData.cpp` log category `LogTemp` → shared `LogArchSim` | Convention drift; non-blocking; needs a category-definition site decision (umbrella `LogArchSim` vs per-class `LogArchSimMember`) | Round 1 LOW-batch-u1 Phase 3 review |
| **AS-24** | FrameCoreUE NewObject outer for InteractiveSubsystem isolated runs | Pre-existing since v3.5.1 — `NewObject<UFrameInteractiveSubsystem>()` without proper outer produces ClassWithin warning that cascades to NotNull.cpp fatal in isolated test runs (full gate suite handles non-fatally). AS-17-u1 and ArchSimPieHarness both reuse the pattern; NOT introduced by S-03 | Round 3 AS-13-u2 Phase 3 review |
| **Z-01** | v0.4.0 spike — UE5.8 upgrade + Scenario editor MVP | Eval-gated per S-03 scope contract; user explicitly deferred to a new session for v0.4.0 scope evaluation | S-03 scope contract Tier 2 round 2 |

Pre-S-03 backlog items still open (no S-04 commitment yet):

- **AS-04** — Plugins panel visual confirmation (human, ~30 min)
- **AS-05** — Art assets (K1-T2 / K4) — parallel human-side
- **AS-06** — SPUD UE5.5 StructUtils deprecation (defer to UE5.8 upgrade window, see Z-01)
- **AS-08** — SPUD orchestration `RF_Transient` audit (when wiring SPUD)
- **AS-09** — Non-cuDSS host gate re-verify (opportunistic)

---

## 5. Reproducibility prerequisites

The five-leg gate's reproduction commands assume:

- **UE 5.7 install** with `$env:UE_ENGINE_ROOT` pointing at its root
  (`$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat` must exist).
- **conda env `framecore-direct`** for OpenBLAS / METIS / cuDSS DLLs on PATH.
  This env is **libs-only** — it does NOT have Python installed.
- **`openseespy` in the system Python**, NOT in the `framecore-direct` env.
  S-02 hardening surfaced this distinction; the conda env is a DLL container,
  not a Python interpreter. `Tools/opensees_compare.py` imports
  `openseespy.opensees as ops` from whichever Python is on PATH first
  (system Python where you installed `pip install openseespy`).
- **MSVC C++20** toolchain (VS 2026 Community Preview works; `cl` does not
  need to be on PATH because `build.bat` resolves it via vswhere).
- **cuDSS lane (optional)**: cuDSS DLL on PATH for the +2 GPU tests
  (F67/F67s family). Without cuDSS, run with `-ExpectedUeTests 143`.

If `Scripts/run_gate.ps1` can't find a leg's binary or returns NOT RUN, check
the env probe section at the script head before assuming the build is broken.

---

## 6. Breaking changes

**None.**

API-level audit performed by the release integrator pass:

- No `USTRUCT` field added/removed/reordered in `Source/ArchSim/Public/`.
- Two new `virtual void ... override` declarations on `AArchSimCharacter`
  (`NotifyControllerChanged`, `CalcCamera`) — **additive override**, does not
  change inherited signatures, BP/save-game schema unchanged.
- No new public functions on `UArchSimModelRegistry` or `UArchSimMemberData`.
- Test count growth (140 → 145) is additive; no test renamed or removed.

Game-body API at v0.3.0 is backwards-binary-compatible with v0.2.0 for the
narrow purposes the API currently supports.

---

## 7. Tag plan

The v0.3.0 tag is **local-only and annotated** at the release-hardening
commit (the commit that ships this file). Per project no-push-without-auth
discipline, the integrator does NOT push or `gh release create`. To publish
once you have reviewed:

```powershell
# (after this release commit lands)
git push origin main
git push origin v0.3.0
gh release create v0.3.0 `
    --title "v0.3.0 — Hardening + PIE-world test harness foundation (Sprint S-03)" `
    --notes-file docs/RELEASE_v0.3.0.md
```

If the remote URL drifted since v0.2.0 (rename/transfer), `git remote -v`
will surface that during the push; update with
`git remote set-url origin <new>` before retrying.

---

## 8. Per-engine and per-area status (carry forward from prior releases)

- **FrameCore engine** (`Plugins/FrameSolver/Source/FrameCore/`):
  FROZEN at v4.0.0; 0 lines changed in S-03; standalone gate F1..F71
  ALL PASS; deep audit 104 checks PASS.
- **FrameCoreUE** (`Plugins/FrameSolver/Source/FrameCoreUE/`):
  +1 new test (`FrameCore.UE.EmptyModelStartSession`) for AS-17. UE
  module surface unchanged otherwise.
- **LevelSim** (`Plugins/LevelSim/`): FROZEN at v1; 0 lines changed
  since v2.2+1 bundle.
- **External plugins** (ALS / Prefabricator / SPUD / SUQS):
  read for precedent (especially `AlsCharacterExample.cpp` for AS-15
  + AS-16 pattern matching) but never edited.

---

## 9. Process notes (durable lessons surfaced this sprint)

These are S-03 specific. The repo-wide CLAUDE.md continues to carry
the global lessons; S-03 adds:

1. **The "plan said X, FrameCoreUE precedent says Y" pattern is healthy.**
   AS-13-u1's plan suggested `FAutomationEditorCommonUtils::CreateNewMap`
   with a `UWorld::CreateWorld(EWorldType::Game)` fallback. Main-thread
   pre-flight surfaced the proven `GEngine->GetWorldContexts()` pattern
   already used in `FrameCoreUEActorStressMeshTest.cpp:35-51` (which
   explicitly documents `UWorld::CreateWorld` crashing in commandlet
   mode). The dispatch prompt was updated; the unit landed with much
   lower real risk than the plan estimated. Pre-flight reads continue
   to be worth their token cost.
2. **Honest defer at Level 3 unlocks real work.** AS-13-u2 ships three
   tests where only one (`PieInputRuntime`) genuinely advances coverage
   in headless Level 3. The other two (`PieRebaseline`, `PieDriverLoop`)
   are explicit AS-07-lesson-#1 honest-defer documents — they pin
   accumulator math and Tick safety in Level 3 and clearly state which
   sub-paths require Level 1/2 or true PIE. This is better than either
   (a) faking trip-path firing or (b) skipping the test bodies entirely.
3. **The `protected` access spec is a real planning input.**
   `SetupPlayerInputComponent` and `NotifyControllerChanged` are
   `protected` virtual overrides — direct invocation from a test TU
   fails with C2248. AS-13-u2's plan estimated 8-12 sub-checks per
   PieInputRuntime; the access spec cut it to 7 with honest defer.
   Future test-planning should grep `protected:` on the surface
   being exercised before committing sub-check budgets.
4. **Compile errors surfacing pre-existing issues stay in their backlog.**
   AS-13-u2 surfaced the FrameCoreUE `NewObject` ClassWithin warning
   that cascades to NotNull.cpp fatal in *isolated* test runs (full
   gate suite handles non-fatally). Confirmed pre-existing since
   v3.5.1 (`5eeab2e`); AS-17-u1 (`7eeb77b`) reuses the same fallback
   helper without introducing the issue. Filed as AS-24 (LOW) and
   left out of S-03's commit scope.
5. **Parallel-dispatch race lessons.** Round 1 parallel (LOW-batch +
   AS-17) showed that subagents sharing `Saved/Logs/ArchSim.log` will
   trample each other if both call `run_gate.ps1`; instruct each
   parallel agent to skip the gate and let the main thread run it once
   after both return. Sequential rounds 2 and 3 let each subagent run
   the gate themselves, which is the simpler protocol.

---

## 10. Quick links

- Scope contract: `docs/logs/S-03/scope_2026-06-26T1652.md`
- Execution plan: `docs/logs/S-03/plan_2026-06-26T1652.md`
- Manager log: `docs/logs/S-03/manager.md`
- Per-unit dispatch + adversarial review logs:
  - `docs/logs/S-03/agent_LOW-batch-u1.md`
  - `docs/logs/S-03/agent_AS-17-u1.md`
  - `docs/logs/S-03/agent_AS-15-u1.md`
  - `docs/logs/S-03/agent_AS-16-u1.md`
  - `docs/logs/S-03/agent_AS-13-u1.md`
  - `docs/logs/S-03/agent_AS-13-u2.md`
- Handoff for S-04: `docs/HANDOFF_v0.3.0.md`
- Prior release: `docs/RELEASE_v0.2.0.md`
- Architecture index: `docs/ARCHITECTURE_INDEX.md`
