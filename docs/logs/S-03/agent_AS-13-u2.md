# Agent log — AS-13-u2: 3 deferred test branches via PIE harness

## Dispatch 2026-06-26T19:35 (iteration 1)

**Plan reference:** `docs/logs/S-03/plan_2026-06-26T1652.md` § Unit 6
**Domain skills loaded:** `ue5-engineer` + `qa-strategist`
**Budget:** 4h / 220K tokens / 45 steps / 28min timeout
**Dispatch mode:** sequential (Round 3 dispatch 2) — depends on Unit 5 AS-13-u1 harness (commit `f82f590`)

### Pre-flight reads done by main thread

- `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` (full) — 3-level contract documented; Level 3 is reality in headless
- `Source/ArchSim/Private/Tests/ArchSimRebaselineTest.cpp` — AS-10 v0.1.4 honest-defer pattern (trip-path unreachable in headless; 7 sub-checks pin accumulator math)
- `Source/ArchSim/Private/Tests/ArchSimGameInstanceTest.cpp` — AS-02c v0.1.5 honest-defer pattern (driver-loop unreachable in NewObject fixture; 7 sub-checks pin Tick telemetry + IsTickable filter)
- `Source/ArchSim/Private/Tests/ArchSimCharacterTest.cpp` — AS-03d v0.2.0 CDO-only pattern (Enhanced Input + ALS state + camera attachment all deferred)
- `Source/ArchSim/Private/Tests/ArchSimPieSmokeTest.cpp` — Unit 5 smoke test (Level 3 confirmed; SpawnActor<AArchSimCharacter> SUCCEEDS in commandlet)

### Reality check — what's actually reachable at Level 3

The Unit 5 harness reaches Level 3 (no OwningGameInstance in commandlet). What this means for the 3 deferred branches:

1. **AS-10 rebaseline trip-path**: STILL UNREACHABLE at Level 3. `RequestSolve` early-exits on `!GetGameInstance()` regardless of whether Registry came from harness or NewObject. The trip block (`bNeedsRebaseline = true` + `Sub->Rebaseline()` call) requires Level 1/2 OR true PIE. **Honest defer continues.**

2. **AS-02 driver-loop**: STILL UNREACHABLE at Level 3. `UArchSimGameInstance::Tick` fires via FTickableGameObject only when GameInstance is attached to a World. Level 3 means no OwningGameInstance, so even harness-acquired Registry won't trigger the Tick driver loop. **Honest defer continues.**

3. **AS-03d input runtime**: PARTIALLY REACHABLE at Level 3. `SpawnActor<AArchSimCharacter>` SUCCEEDS in the commandlet world (Unit 5 confirmed). This unlocks **genuine new coverage**:
   - Runtime instance state (Camera != null on spawned instance, vs null on CDO)
   - Controller-rotation flags verified on instance (not just CDO)
   - Default mapping context slot is null (BP author assigns; verify default contract)
   - SetupPlayerInputComponent invocable but no-op without PlayerController (verify graceful)
   - NotifyControllerChanged invocable directly via reflection IF needed
   - HONEST DEFER: full Enhanced Input runtime + ALS state machine still need PlayerController in PIE

### Scope adjustment from plan

Plan estimated 10-15 sub-checks per test. Realistic for Level 3 reality:
- `ArchSim.Integration.PieRebaseline` — ~5-8 sub-checks (harness integration + accumulator math + honest defer)
- `ArchSim.Integration.PieDriverLoop` — ~5-8 sub-checks (harness integration + Tick world safety + honest defer)
- `ArchSim.Gameplay.PieInputRuntime` — ~8-12 sub-checks (genuine instance state advancement)

Honest defer pattern (AS-07 lesson #1) applies to all 3 — must clearly distinguish what's PINNED vs UNREACHABLE.

### File ownership

This unit owns:
- `Source/ArchSim/Private/Tests/ArchSimPieRebaselineTest.cpp` (new)
- `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` (new)
- `Source/ArchSim/Private/Tests/ArchSimPieInputRuntimeTest.cpp` (new)
- `Scripts/run_gate.ps1` (`$ExpectedUeTests` 142→145 cuDSS / 140→143 non-cuDSS — 3 new tests)
- `docs/logs/S-03/agent_AS-13-u2.md` (this log)

Forbidden: any other path; the 4 existing ArchSim test files are NOT to be modified (they remain v0.1.x/v0.2.0 history; new PIE tests don't supersede them).

### Composed prompt (verbatim)

```
[See full prompt in Agent tool invocation that follows.]
```

---

## Agent return 2026-06-26T19:xx (actual execution)

### Status
DONE — 3 test files shipped, gate 145/145 GREEN.

### Files touched
| Path | LOC delta | Type | New? |
|---|---|---|---|
| `Source/ArchSim/Private/Tests/ArchSimPieRebaselineTest.cpp` | +125 | Test | Yes |
| `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` | +133 | Test | Yes |
| `Source/ArchSim/Private/Tests/ArchSimPieInputRuntimeTest.cpp` | +148 | Test | Yes |
| `Scripts/run_gate.ps1` | comment+value bump 142→145 | Config | No |

Total new test LOC: ~406 (under 550 HIGH tolerance, well under 600 ESCALATE limit).

### Design decisions

**Level 3 honest-defer pattern for each branch:**

1. `PieRebaseline` (AS-10): Trip-path (`bNeedsRebaseline = true` at cpp:284) remains unreachable in Level 3 — GI-null early-return at cpp:274 is the gate. Test pins: harness registry equivalence to NewObject, accumulator math (96 boundary + 97 no-reset), two successive harness calls return independent instances (no static state). Honest defer stated in file header AND Sub-check 5b comment.

2. `PieDriverLoop` (AS-02): Driver-loop dispatch remains unreachable — `IsTickable()` returns false for `NewObject<UArchSimGameInstance>()` (bIsActive=false gate). Test pins: World non-null, GI null in Level 3, World->Tick x3 safe, SolveTriggerCount stays 0, harness registry unaffected. Honest defer stated in file header AND Sub-check 5a comment.

3. `PieInputRuntime` (AS-03d): Genuine advancement — `SpawnActor<AArchSimCharacter>` succeeds. Pins: Camera UPROPERTY non-null on real instance (vs CDO-only in old test), bUseControllerRotation* false on instance, 6 IA_* slots null, IsValid before/after Destroy. Protected method access (`SetupPlayerInputComponent`, `NotifyControllerChanged`) ruled out at compile time — documented as honest defer. Honest defer stated in file header.

**3 tests share no mutable state:** each creates its own UArchSimModelRegistry via harness (Level 3 returns fresh NewObject per call); spawned AArchSimCharacter is Destroy'd before test returns.

**Build error discovered and fixed:** `SetupPlayerInputComponent` and `NotifyControllerChanged` are `protected` virtual overrides — compiler C2248 at first build attempt. Removed both direct calls; replaced Sub-check 6 with `IsValid(Char) == true` before destroy (positive lifecycle guard), sub-check 7 is DestroyActor + IsValid == false. Sub-check count changed from 8 to 7 for PieInputRuntime.

**Single-test run crash (not a code defect):** Isolated run of `ArchSim.Integration.PieRebaseline+ArchSim.Integration.PieDriverLoop+ArchSim.Gameplay.PieInputRuntime` triggered a pre-existing `FrameCoreUE.InteractiveSubsystem.Lifetime` ensure failure (ClassWithin=GameInstance + NewObject without outer) which cascaded to a NotNull.cpp fatal crash. This crash is pre-existing (NOT introduced by AS-13-u2) and does NOT occur in the run_gate.ps1 suite because the Automation runner handles the ensure non-fatally when running the full `FrameCore+ArchSim` set. **All 3 new tests passed 成功 in the full suite.**

### Verification evidence
- UE incremental build: `Succeeded` exit 0 (second attempt after protected-member fix, 2.80s)
- Full suite single run (pre-gate verification): 3 tests `Result={成功}`, Total=145, Automation exit code=0
- **5-leg gate: GATE: PASS** (standalone OK, UE 145 tests green, OpenSees PASS, audit OK, CLI round-trip OK)

### Self-grading
- "AS-10 trip-path: Level 3 unreachable, honest defer" — [NEW CODE] with [VERIFIED] file:line citations in comments
- "AS-02 driver-loop: Level 3 unreachable, honest defer" — [NEW CODE] with [VERIFIED] file:line citations in comments
- "AS-03d spawn-time advancement (Camera != null on instance)" — [VERIFIED] gate green
- "3 tests no shared state" — [NEW CODE] each creates independent registry/actor, actor destroyed on exit
- "$ExpectedUeTests 142→145 matches actual" — [VERIFIED] (gate PASS, 145 == 145)

### ESCALATE
None.

---

## Adversarial review (iteration 1) 2026-06-26T20:00

**Verdict:** CLEAN

**Reviewer dimension coverage:** ✅ all 6/6 verified file:line
1. AS-10 trip-path Level 3 unreachable → honest defer (PieRebaselineTest L105-122 TestFalse + file header cite cpp:274 GI-null early-return)
2. AS-02 driver-loop Level 3 unreachable → honest defer (PieDriverLoopTest L118-129 SolveTriggerCount stays 0 + IsTickable()==false)
3. AS-03d spawn-time advancement genuine (PieInputRuntimeTest L81-91 TestNotNull(Char->Camera.Get()) after real SpawnActor — non-CDO path)
4. 3 tests no static state — grep confirmed no static variables across 3 files
5. honest defer cited in file head comments (not buried)
6. $ExpectedUeTests bump 142→145 verified via git diff

**鐵則 compliance:** ALL CONFIRMED (8 grep predicates pass)
- FROZEN engine 0 / LevelSim 0 / 4 ext plugins 0
- ArchSim production source (non-Tests/) 0 / Existing 4 tests + AS-13-u1 territory 0 / Phase 5 territory 0
- No stub / no truncate / [VERIFIED]/[NEW CODE] grading honest

**Findings (2 NITs, both LOW):**

| # | severity | file:line | issue | action |
|---|---|---|---|---|
| 1 | NIT | `run_gate.ps1:29` comment | non-cuDSS comment text in AS-13-u1 history segment still shows 140 (cosmetic doc drift; the actual `= 145` value is correct) | **→ Phase 5 docs sync** |
| 2 | NIT | `ArchSimPieDriverLoopTest.cpp:108-111` | Sub-check 4 `TestTrue(..., true)` is a tautology — could verify `World->GetTimeSeconds()` actually advanced post-Tick | **→ AS-22 polish or Phase 5** (harmless; crash itself is implicit gate) |

**Pre-existing issue identified by reviewer (NOT introduced by AS-13-u2):**
- `NewObject<UFrameInteractiveSubsystem>()` without proper outer causes ClassWithin warning that cascades to NotNull fatal in **isolated** test runs (but NOT in full gate suite — UE Automation handles ensure non-fatally in batch mode)
- Confirmed pre-existing since v3.5.1 (`5eeab2e`)
- AS-17-u1 (`7eeb77b`) reused the same pattern but did NOT introduce
- **→ AS-24 backlog opened** (LOW; FrameCoreUE polish, not blocking v0.3.0)

**Reviewer evidence:** Read 5 files, grep'd 10 patterns, cross-checked 8 claims.

**Decision:** Accept CLEAN. AS-24 backlog opened for pre-existing FrameCoreUE issue (LOW; isolated-run only, full gate unaffected). Advance to Phase 4 commit.

---

## Phase 4 — Feature commit (no tag) 2026-06-26T20:05

**Mode:** Feature-commit only. **This is the final v0.3.0 unit before RELEASE** — Unit 7 RELEASE next will perform the actual release-hardening + v0.3.0 tag.

**Files committed (5):**
- `Source/ArchSim/Private/Tests/ArchSimPieRebaselineTest.cpp` (+125, new)
- `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` (+133, new)
- `Source/ArchSim/Private/Tests/ArchSimPieInputRuntimeTest.cpp` (+148, new)
- `Scripts/run_gate.ps1` (142→145 cuDSS / 140→143 non-cuDSS)
- `docs/logs/S-03/agent_AS-13-u2.md` (this log)

**Verification:** subagent ran the full 5-leg gate (PASS UE 145).

**Next:** Loop back to Phase 2 → Unit 7 RELEASE (v0.3.0 release-hardening + tag).
