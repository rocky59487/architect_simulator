# v0.3.1 — S-04 patch (carryover cleanup + cosmetic NITs bundle)

**Sprint:** S-04 patch path close
**Date:** 2026-06-27
**Repo:** `architect_simulator` (game-body line)
**Baseline tag:** `v0.3.0` (`442670c`)
**HEAD at tag time:** release-hardening commit (see tag plan below)

> **Headline.** Closes two S-03 deferred backlog items (AS-20 `LogTemp` →
> `LogArchSim` umbrella sweep + AS-24 FrameCoreUE `NewObject`
> `GetTransientPackage()` outer 3-site fix) and bundles seven cosmetic
> Phase-5 NITs from S-03's review trail. Engine source delta = **0**
> (FrameCore v4.0.0 FROZEN + LevelSim v1 FROZEN both honoured). UE
> automation test count unchanged at 145 (cuDSS) / 143 (non-cuDSS) —
> NIT-f's test rename for namespace parity does not change count.

---

## 1. What landed

### 1a. Closed S-03 deferred backlog

| AS-XX | Commit | Title | LOC | Notes |
|---|---|---|---|---|
| **AS-20** | `4b6f094` | `LogTemp` → `LogArchSim` umbrella sweep | production +1 include (no net code delta) / tests +1 include | 3-site flip (1 production at `ArchSimMemberData.cpp:26` + 2 test diagnostic at `ArchSimSaveLoadTest.cpp:86,294`) using the pre-existing module-level umbrella declared at `ArchSimGameInstance.h:27` and defined at `ArchSimGameInstance.cpp:19`. `LogArchSimRegistry` per-class precedent untouched. `#include "ArchSimGameInstance.h"` added to both touched .cpp files for EXTERN visibility |
| **AS-24** | `2883d40` | FrameCoreUE NewObject `GetTransientPackage()` outer (3-site) | tests +3 / -3 (with AS-24 WHY comments) | Mechanical fix in the namespace-anonymous `GetSubsystem()` headless fallback in `FrameCoreUEInteractiveSubsystemTest.cpp:49`, `FrameCoreUELoadPatchTest.cpp:41`, `FrameCoreUERedundancyFieldTest.cpp:40`. **Honest disclosure:** Phase 3 review independently verified UE 5.7 `UObjectGlobals.h:1918-1920` — `NewObject<T>()` default outer **already IS** `GetTransientPackageAsObject()` (i.e. `GetTransientPackage()`). Explicit-pass and no-arg are runtime-identical; the `ClassWithin(UGameInstance)` `ensure()` still fires (non-fatal, first-time only). Fix value is therefore **intent documentation + future-proofing**, not behavioural change. HANDOFF_v0.3.0.md §4 specified this exact fix verbatim as the AS-24 first action |

### 1b. PHASE5-NITS-u1 bundle (7 cosmetic NITs)

| NIT | Site | Change |
|---|---|---|
| **a** | `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` L51-55 | "always has at least one" docstring overclaim → empirical "consistently has at least one world context…in our verified test runs" |
| **b** | `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` L108-112 | `TestTrue(..., true)` literal tautology → `TestNotNull(World->GetCurrentLevel())` real-invariant assertion. (`UWorld::IsTickable()` is not a member; subagent found this on first build attempt and fell back to `GetCurrentLevel()` non-null check — robust post-Tick world-integrity oracle that doesn't depend on headless time advancing) |
| **c** | `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` 8 sites (L130/L148/L177/L203/L216/L231/L246/L261) | Uniform `[Input]` category prefix added to all 8 `UE_LOG(LogArchSim, Warning, ...)` lines in `NotifyControllerChanged` + `SetupPlayerInputComponent`. Verb levels preserved at Warning; Display/Verbose/Error/Log levels untouched. (NIT spec originally said ≤2 sites; user authorized the 8-site scope expansion at Round 2 ESCALATE) |
| **d** | `Scripts/run_gate.ps1` L29 | Comment block trim: removed intermediate count progression (137/138/139/140/141/142), kept 8 major version anchors (v0.1.1 / v0.1.3 / v0.1.4 / v0.1.5 / v0.2.0 / v0.3.0 ×2 + v3.x one-liner). `$ExpectedUeTests = 145` value and script logic unchanged |
| **e** | `docs/ARCHITECTURE_INDEX.md` §5 + §6 | §5 `< 96` reading ambiguity → `rebaseline when PendingRankAccumulation > 96` matching the actual condition at `ArchSimModelRegistry.cpp:289`. §6 `cpp:281` stale cite was already absent (AS-11 v0.3.0 cleared it; spec inherited the stale reference from RELEASE_v0.3.0.md history) |
| **f** | `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` L243 | Test path-string renamed for namespace parity: `"FrameCore.UE.EmptyModelStartSession"` → `"FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession"`. Test class name `FFrameCoreUEEmptyModelStartSessionTest` preserved (only the `IMPLEMENT_SIMPLE_AUTOMATION_TEST` macro's 2nd argument changes). Old name now invalid for `Automation RunTests`; new name discoverable + PASS in isolated single-test run. Counted-as: rename, not add/remove → `$ExpectedUeTests = 145` unchanged. ARCH_INDEX §6 recent-additions L237 + §7 backlog L270 both synced to the new name |
| **g** | 3 FrameCoreUE test files post-AS-24 comments | 3-site AS-24 comment unification: `FrameCoreUEInteractiveSubsystemTest.cpp` had a 3-line detailed comment (citing the `NotNull.cpp` cascade rationale); `FrameCoreUELoadPatchTest.cpp` and `FrameCoreUERedundancyFieldTest.cpp` had only 2-line short versions. Picked the 3-line detailed variant as canonical and applied verbatim to all 3 sites. Comment style stays WHY-not-WHAT |

All 7 NITs landed in commit `e763fa9`, plus a Phase 3 closeout inline fix at `docs/ARCHITECTURE_INDEX.md:270` (NIT-f sync leak the subagent missed — reviewer caught, main thread fixed inline before commit).

### 1c. Ceremonial accept — U-INFRA-u1 (hook fix; outside repo)

| Unit | Mode | Files | Notes |
|---|---|---|---|
| **U-INFRA-u1** | No commit (hook lives outside repo) | `~/.claude/hooks/work-phase-guard.ps1` (+46 LOC: +14 code, +29 comment, +3 blank) + `.bak` backup | Closes S-03 retrospective lesson #6 (foreign-project state file race). Dual-layer defence: (1) per-project state dir resolution from stdin JSON `cwd` field, falling back to legacy shared `~/.claude/state/work-phase.txt`; (2) content sniff treating any state whose first `/`-segment doesn't match `^S-\d+$` as foreign and exiting 0 (fail-open). Backward-compat preserved — idle / phase-4/release / phase-6/closing-session states behave identically to the original hook. Tested against 4 stdin scenarios live before atomic `Move-Item` swap. No ArchSim repo diff this unit |

### 1d. Numbers and counts

| Metric | v0.3.0 | v0.3.1 | Δ |
|---|---|---|---|
| UE automation tests (cuDSS) | 145 | **145** | 0 (NIT-f rename ≠ count change) |
| UE automation tests (non-cuDSS) | 143 | **143** | 0 |
| Standalone FrameCore F-fixtures | F1..F71 | F1..F71 | 0 |
| Linear deep audit checks | 104 | 104 | 0 |
| OpenSees compare oracle | strict | strict | 0 |
| ArchSim test files | 9 | 9 | 0 |
| FrameCore engine source files modified | 0 | 0 | 0 (FROZEN honoured) |
| LevelSim source files modified | 0 | 0 | 0 (FROZEN honoured) |
| External plugin (ALS/Prefabricator/SPUD/SUQS) source files modified | 0 | 0 | 0 |
| Files touched in repo (v0.3.0 → v0.3.1) | — | 11 (+5 sprint logs) | — |

### 1e. Iron rule compliance audit

Verified via grep on `git diff v0.3.0..HEAD`:

```
git diff --stat v0.3.0..HEAD -- Plugins/FrameSolver/Source/FrameCore/ Plugins/LevelSim/Source/LevelCore/
→ 0 lines (FROZEN integrity CONFIRMED both path-level and behavior-level)

git diff --name-only v0.3.0..HEAD | grep -E '^Plugins/(ALS|Prefabricator|SPUD|SUQS)/'
→ no match (external plugins untouched)

git diff --name-only v0.3.0..HEAD | grep -E '\.gitignore$|ArchSim\.uproject$|\.dll$|\.exp$|\.lib$|\.exe$'
→ no match (rule #5 honoured)

git diff v0.3.0..HEAD -- '*.cpp' '*.h' | grep -iE 'FROZEN|avoid.*touch|workaround|so we don.t'
→ 0 hits (no behavior-level FROZEN fork)
```

No verbal-amendment-without-CLAUDE.md-edit cases this sprint.

---

## 2. Verification matrix

All five legs run on the integrator host. Subagents re-ran the 5-leg gate after each unit's commit and on the combined working tree for PHASE5-NITS-u1; reviewer confirmed gate output verbatim in each agent log.

| Leg | What runs | Reproduce command | v0.3.1 status |
|---|---|---|---|
| 1 | Standalone FrameCore (F1..F71) | `Plugins/FrameSolver/Standalone/build.bat` | ALL PASS (failures=0) |
| 2 | UE headless automation (145 cuDSS / 143 non-cuDSS) | see one-liner below | 145 PASS, exit 0 |
| 3 | OpenSees offline compare | `python Tools/opensees_compare.py --relaxed` | PASS |
| 4 | Linear deep audit (104 checks) | `Plugins/FrameSolver/Standalone/build_linear_audit.bat` | PASS failures=0 checks=104 |
| 5 | CLI round-trip (J1 bridge) | `python Tools/cli_roundtrip.py` | ALL PASS (failures=0) |
| **Aggregate** | All five legs in one driver | `Scripts/run_gate.ps1 -RequireOpenSees` | GATE: PASS |

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

### 2c. NIT-f rename single-test verify

```powershell
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession; Quit" `
    -unattended -nullrhi -log
# Expect: 1 Result={成功} + EXIT CODE: 0
# Old name "FrameCore.UE.EmptyModelStartSession" now returns "No tests matched".
```

### 2d. NOT RUN this release

None of the five legs were marked NOT RUN at v0.3.1. Every leg returned exit 0 on the integrator host with cuDSS available.

---

## 3. Known issues / cosmetic items (deferred to future patch)

These are **not blockers** for v0.3.1; they're new backlog items opened during S-04 review for future patches.

- **AS-25** (LOW; hook): the per-project regex `^S-\d+$` in `~/.claude/hooks/work-phase-guard.ps1` won't match suffix sprint identifiers like `S-04a` or `S-04b`. Defer to future maintenance — no current sprint uses suffix naming.
- **AS-26** (MEDIUM; UE consumer): `UArchSimModelRegistry` is a `UGameInstanceSubsystem` (same `ClassWithin = UGameInstance` constraint as `UFrameInteractiveSubsystem`). `ArchSimPieHarness.cpp:81` uses `NewObject<UArchSimModelRegistry>()` without explicit outer. HANDOFF_v0.3.0.md §4 AS-24 first-action said this site "should also adopt the same pattern"; S-04 AS-24-u1 scope explicitly excluded it. Recommended follow-up: verify `UArchSimModelRegistry` ClassWithin status; if confirmed, mirror the AS-24 explicit-`GetTransientPackage()` pattern.
- **AS-27** (LOW; carryover docs): (a) `docs/ARCHITECTURE_INDEX.md` §8 gate cheat-sheet still says `140 expected / 138 on non-cuDSS` (current is 145/143; pre-existing stale not introduced by S-04). (b) `ArchSimPieDriverLoopTest.cpp` L54 + L58 sub-check 1 comments have empirical-overclaim `"always has at least one"` / `"always provides"` (NIT-a scope was PieHarness.h only; sub-check 1 carryover not in this unit's diff).

---

## 4. Deferred to S-05 (with first-action sketches in HANDOFF_v0.3.1.md §4)

| ID | Title | Why deferred | Audit origin |
|---|---|---|---|
| **Z-01** | v0.4.0 spike — UE5.8 upgrade + Scenario editor MVP | Eval-gated per S-04 scope contract Tasks #6-#8; remaining in S-04 scope (this session may or may not complete the spike portion per user "no cap" preference) | S-04 scope contract |
| **AS-25** | Hook regex broaden | Forward-looking maintenance; no current convention needs it | S-04 Round 1 U-INFRA-u1 review |
| **AS-26** | UArchSimModelRegistry ClassWithin + ArchSimPieHarness mirror | Out of AS-24-u1 scope; HANDOFF carryover | S-04 Round 1 AS-24-u1 review |
| **AS-27** | ARCH_INDEX §8 + DriverLoopTest sub-check 1 cosmetic carryover | Pre-existing; not introduced by any S-04 unit | S-04 Round 2 PHASE5-NITS-u1 review |

Pre-S-03 backlog items still open (no S-05 commitment yet):

- **AS-04** — Plugins panel visual confirmation (human, ~30 min)
- **AS-05** — Art assets (K1-T2 / K4) — parallel human-side
- **AS-06** — SPUD UE5.5 StructUtils deprecation (defer to UE5.8 upgrade window, see Z-01)
- **AS-08** — SPUD orchestration `RF_Transient` audit (when wiring SPUD)
- **AS-09** — Non-cuDSS host gate re-verify (opportunistic)

---

## 5. Reproducibility prerequisites

Carry forward from `RELEASE_v0.3.0.md §5` unchanged. Quick reminder:

- **UE 5.7 install** with `$env:UE_ENGINE_ROOT` set
- **conda env `framecore-direct`** for OpenBLAS / METIS / cuDSS DLLs on PATH (libs-only — no Python in this env)
- **`openseespy` in the system Python**, NOT in `framecore-direct`
- **MSVC C++20** toolchain (VS 2026 Community Preview works; `build.bat` resolves `cl` via vswhere)
- **cuDSS lane (optional)**: for the +2 GPU tests (F67/F67s family); without cuDSS, run with `-ExpectedUeTests 143`

The S-04 patch session changed nothing in the reproducibility surface — no new env variable, no new external dependency, no new conda package.

---

## 6. Breaking changes

**None.**

API-level audit performed by the release integrator pass:

- No `USTRUCT` field added/removed/reordered in `Source/ArchSim/Public/`.
- `AArchSimCharacter` Warning UE_LOG message text changes (NIT-c `[Input]` prefix) are observer-side only; signature and call sites unchanged.
- `FrameInteractiveSubsystem.h/.cpp` production UCLASS surface unchanged (AS-24 fix is test-side only).
- Test count growth (145 → 145) is zero; NIT-f rename keeps the path-string discoverable count identical.
- Test path `FrameCore.UE.EmptyModelStartSession` is removed; replaced by `FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession`. If any external script ran the old path string, it must be updated. (No external scripts in this repo reference the old name; subagent grep confirmed 0 occurrences post-rename.)

Game-body API at v0.3.1 is backwards-binary-compatible with v0.3.0 for the narrow purposes the API currently supports.

---

## 7. Tag plan

The v0.3.1 tag is **local-only and annotated** at the release-hardening commit (the commit that ships this file). Per project no-push-without-auth discipline, the integrator does NOT push or `gh release create`. To publish once you have reviewed:

```powershell
# (after this release commit lands)
git push origin main
git push origin v0.3.1
gh release create v0.3.1 `
    --title "v0.3.1 — S-04 patch (carryover cleanup + cosmetic NITs)" `
    --notes-file docs/RELEASE_v0.3.1.md
```

If the remote URL drifted, `git remote -v` will surface that during the push.

---

## 8. Per-engine and per-area status (carry forward from prior releases)

- **FrameCore engine** (`Plugins/FrameSolver/Source/FrameCore/`): FROZEN at v4.0.0; 0 lines changed in S-04; standalone gate F1..F71 ALL PASS; deep audit 104 checks PASS.
- **FrameCoreUE** (`Plugins/FrameSolver/Source/FrameCoreUE/`): 3 test files touched (AS-24-u1 + PHASE5-NITS NIT-f + NIT-g). UE module production surface (`FrameInteractiveSubsystem.h/.cpp`, all libraries / actors / USTRUCTs) unchanged.
- **LevelSim** (`Plugins/LevelSim/`): FROZEN at v1; 0 lines changed since v2.2+1 bundle.
- **External plugins** (ALS / Prefabricator / SPUD / SUQS): unchanged this sprint.

---

## 9. Process notes (durable lessons surfaced this sprint)

S-04 specific. The repo-wide CLAUDE.md continues to carry the global lessons; S-04 adds:

1. **Pre-flight grep can collapse a budget estimate.** AS-20-u1 was scoped at 1-1.5h; main-thread pre-flight grep on `LogTemp|DECLARE_LOG_CATEGORY_EXTERN|DEFINE_LOG_CATEGORY` found that the `LogArchSim` umbrella was already declared/defined at `ArchSimGameInstance.h:27` + `.cpp:19` from prior sprints. Budget collapsed to 30 min mechanical flip. The opposite happened on AS-24-u1: pre-flight grep on `NewObject<UFrameInteractive` revealed 3 sister sites (not 1 as scope assumed), expanding the work scope. Both are wins — knowing the real shape before dispatching.
2. **"Subagent honest disclosure" is a real signal.** AS-24-u1 subagent independently discovered (and disclosed) that `NewObject<T>()`'s template default already IS `GetTransientPackage()` per `UObjectGlobals.h:1918` — making the explicit-pass behaviourally identical. Could have silently shipped the fix as "warning suppression"; instead self-graded `[VERIFIED, expected]` with the comment "fix value = intent clarity + comment". Phase 3 reviewer independently verified the UE-source claim, agreed with NITS verdict, and approved on intent-documentation grounds. Honest disclosure preserved both the ship and the project's truth-discipline.
3. **Reviewer "find #1" inline-fix vs new backlog is a judgment call.** PHASE5-NITS-u1 review caught a NIT-f sync leak (ARCH_INDEX §7 L270 still had the old test name). Could have opened it as backlog; instead main thread applied the 1-line Edit inline as Phase 3 closeout before Phase 4 commit. Rule of thumb that worked: reviewer-found NITs that are direct extensions of the unit's own scope ("you did §6 sync but missed §7 of the same name") inline-fix; reviewer-found pre-existing or cross-cutting NITs go to backlog (AS-27).
4. **ESCALATE triggers can be over-conservative; user scope-confirm is fast.** PHASE5-NITS-u1 iteration 1 hit the spec's `>2 warn sites` ESCALATE for NIT-c when subagent found 8. The 8-site fix turned out to be mechanically trivial. The right pattern: spec writes conservative ESCALATE triggers; main thread surfaces scope-confirm via AskUserQuestion when subagent hits one; user clicks "authorize" and a fresh focused dispatch (within budget) finishes the work. Lesson: ESCALATE triggers protect against silent over-scoping, not from making them too narrow.
5. **Mid-sprint feature commits + final tag ceremony is the right cadence for patch releases.** S-04 patch path landed as 3 feature commits (4b6f094 / 2883d40 / e763fa9) without tags, then v0.3.1 release-hardening commit + tag at the end. Each per-unit commit was reviewable in isolation (git log --follow works correctly per-file); the tag captures the cumulative delta as one canonical reference. Matches v0.3.0 / v0.2.0 / v0.1.x cadence.

---

## 10. Quick links

- Scope contract: `docs/logs/S-04/scope_2026-06-26T2030.md`
- Execution plan: `docs/logs/S-04/plan_2026-06-26T2040.md`
- Manager log: `docs/logs/S-04/manager.md`
- Per-unit dispatch + adversarial review logs:
  - `docs/logs/S-04/agent_U-INFRA-u1.md`
  - `docs/logs/S-04/agent_AS-20-u1.md`
  - `docs/logs/S-04/agent_AS-24-u1.md`
  - `docs/logs/S-04/agent_PHASE5-NITS-u1.md`
- Handoff for S-05 (or v0.3.1 continuation): `docs/HANDOFF_v0.3.1.md`
- Prior release: `docs/RELEASE_v0.3.0.md`
- Architecture index: `docs/ARCHITECTURE_INDEX.md`
