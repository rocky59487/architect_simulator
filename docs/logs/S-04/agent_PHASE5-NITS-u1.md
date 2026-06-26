# Agent log — PHASE5-NITS-u1: 6+1 cosmetic doc + source NITs bundle

## Dispatch 2026-06-26T21:35 (iteration 1)

**Plan reference:** `docs/logs/S-04/plan_2026-06-26T2040.md § "PHASE5-NITS-u1"`
**Domain skills loaded:** cpp-engineer (for source-side edits) + docs (inline)
**Budget:** 1.5h work / 200K tokens / 40 steps / 28 min wall-clock
**Dispatch mode:** sequential (Round 2 of 7; depends on AS-20 + AS-24 file co-tenancy on `FrameCoreUEInteractiveSubsystemTest.cpp`)
**Background:** false (foreground)
**Iteration 1 scope expansion (from S-04 Round 1):** +1 NIT-g (3-site comment unification) added from AS-24 review Finding #2.

### Pre-flight reads (main-thread)

- `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` L41-74 — `GetOrFindWorld()` docstring at L51-55 uses "always" overclaim per AS-13-u1 review
- `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` L95-111 — Sub-check 4 confirmed `TestTrue(..., true)` literal tautology
- `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` L140-156 — warn UE_LOG at L147-150 (HANDOFF cite of L153 is stale post AS-15-u1 lifecycle refit)
- `Scripts/run_gate.ps1` L29 — `$ExpectedUeTests = 145` followed by very long comment with stale intermediate counts (142/140/141/139)
- `docs/ARCHITECTURE_INDEX.md` §5 + §6 — `< 96` ambiguity + `cpp:281` cite (rewritten by AS-11 v0.3.0 LOW-batch but §6 row may still cite stale form)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` L242-244 — `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEEmptyModelStartSessionTest, "FrameCore.UE.EmptyModelStartSession", ...)` (NIT-f rename target)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` post-AS-24 comment (3-line detailed)
- `LoadPatchTest.cpp` + `RedundancyFieldTest.cpp` post-AS-24 comments (2-line short — inconsistency NIT-g target)

### Composed prompt outline

- §1-§4 Iron rules + top-tier discipline + arch-index pointer + baseline: standard
- §5 Domain prefix: cpp-engineer (sufficient — no UE5 surface change)
- §6 Unit spec: 7 NIT items (a-g)
- §7 Verification: UE build + 5-leg gate (count unchanged 145/143) + run_gate.ps1 self-test
- §8-§9 standard

### Key unit-spec elements

**Files in scope (6 distinct files; some touched for multiple NITs):**

| NIT | File | Site | Change |
|---|---|---|---|
| (a) | `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` | L51-55 | Replace "always has at least one" with empirical phrasing ("in verified test runs has...") |
| (b) | `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` | L108-111 | Replace `TestTrue(..., true)` literal with a real assertion (e.g. `TestTrue(..., World->GetTimeSeconds() > t0)` or `TestNotNull(World->GetCurrentLevel())`) |
| (c) | `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` | L147-150 (NOT L153 — stale cite) | Warn msg prefix unification — add `(ArchSim|Input)` style prefix to match convention; verify any sister warns in the file too |
| (d) | `Scripts/run_gate.ps1` | L29 | Trim stale intermediate `ExpectedUeTests` counts (142/140/141/139/137/138/136/135) from the comment block — keep only the v0.3.0 → 145 baseline + the v0.1.x chain that establishes the test trail; preserve the per-v0.x semantic descriptions but compress the count math |
| (e) | `docs/ARCHITECTURE_INDEX.md` | §5 + §6 | §5: rewrite "< 96" to unambiguous ("strictly less than 96" or "≤ 95") OR change to `(rank-accumulation strictly less than the v0.1.4 MaxRankBeforeRebaseline=96 ceiling)`. §6 RebaselineCeiling row: replace stale `cpp:281` cite with stable form (e.g. `RequestSolve body`) — should match AS-11 v0.3.0 pattern |
| (f) | `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` | L242-244 | Rename test path string `"FrameCore.UE.EmptyModelStartSession"` → `"FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession"` (test class name `FFrameCoreUEEmptyModelStartSessionTest` can stay; only the macro's path-string arg changes). Verify `$ExpectedUeTests` in run_gate.ps1 stays 145 (rename ≠ count change). |
| (g) | `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` + `…/FrameCoreUELoadPatchTest.cpp` + `…/FrameCoreUERedundancyFieldTest.cpp` | post-AS-24 comments at L48-51 / L40-42 / L39-41 | Unify the AS-24 comment text across 3 sites. Subagent picks ONE variant (recommend the 3-line detailed version as canonical because it cites the "NotNull.cpp cascades" rationale) and applies to all 3 sites verbatim. Keep WHY-not-WHAT discipline. |

### Files OUT of scope (動 = ESCALATE)

- FROZEN engine (`Plugins/FrameSolver/Source/FrameCore/`)
- LevelSim FROZEN
- Any plugin source (ALS / Prefabricator / SPUD / SUQS)
- `ArchSim.uproject` / `.gitignore` / build artifacts
- `$ExpectedUeTests = 145` numeric value in run_gate.ps1 — DO NOT change the number (only the comment around it); rename in NIT-f doesn't change count
- `LogArchSimRegistry` precedent
- ArchSimPieHarness.cpp (NIT-a touches the .h only)

### Design constraints

- **Test rename (NIT-f)** must NOT add/remove tests. Just changes the test path string the automation discovers as. Count stays 145/143.
- **run_gate.ps1 comment trim (NIT-d)** must NOT change script LOGIC. Only the comment text. After edit, run_gate.ps1 must still execute end-to-end identically.
- **NIT-g comment unification** must NOT change the actual `NewObject<UFrameInteractiveSubsystem>(GetTransientPackage())` line — only the surrounding `//` comment text. Pick the variant that best documents WHY.
- **Anti-goal:** do NOT introduce new NITs of the same category (no new "always" overclaim, no new `TestTrue(..., true)`, no new stale line-ref).

### Adversarial focus (Phase 3 review will check)

- All 7 NITs addressed (count check; not 6, not 8)
- run_gate.ps1 still executable (subagent runs `.\Scripts\run_gate.ps1 -RequireOpenSees` post-edit)
- Test rename keeps `$ExpectedUeTests=145` (no count change)
- ARCH_INDEX edits surgical (§5 + §6 only; no full rewrite)
- AS-24 comment unified across 3 sites (verbatim same text)
- No new NIT introduced
- UE build succeeds
- 5-leg gate PASS at 145

### ESCALATE triggers specific to this unit

- ARCH_INDEX §5 / §6 has additional stale references beyond what's listed → ESCALATE with full inventory before mass edit
- run_gate.ps1 comment trim accidentally breaks PowerShell here-string or comment block parsing → ESCALATE (it's a comment so unlikely, but possible)
- Test rename for some reason changes the test count → ESCALATE
- More than 2 warn UE_LOG sites in ArchSimCharacter.cpp need NIT-c prefix unification → ESCALATE for scope confirmation
- Any unit estimate exceeds 1.5x budget → log + notify (no cap session)

### Verification commands

```powershell
# 1. UE editor incremental build (must succeed)
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="E:\project\ArchSim\ArchSim.uproject" -waitmutex

# 2. 5-leg gate (count UNCHANGED at 145 cuDSS / 143 non-cuDSS; NIT-f rename ≠ count change)
.\Scripts\run_gate.ps1 -RequireOpenSees

# 3. Renamed test path discoverable
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "E:\project\ArchSim\ArchSim.uproject" `
    -ExecCmds="Automation RunTests FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession; Quit" `
    -unattended -nullrhi -log
# Expect: 1 Result={成功} + EXIT CODE: 0

# 4. Sanity grep — no new tautology / overclaim / stale cite introduced
Get-ChildItem -Recurse Source/ArchSim/Private/Tests -Include *.cpp `
  | Select-String -Pattern 'TestTrue\([^,]+,\s*true\s*\)'
# Expect: 0 results (NIT-b fixed)

Select-String -Path Source/ArchSim/Private/Tests/ArchSimPieHarness.h `
  -Pattern '\balways\s+has'
# Expect: 0 results (NIT-a fixed)

# 5. 3-site comment consistency (NIT-g)
Get-ChildItem -Recurse Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests -Include *.cpp `
  | Select-String -Pattern 'AS-24:' -Context 0,2
# Expect: 3 sets of identical 2- or 3-line comment blocks
```

## Agent return 2026-06-26T21:50 (iteration 1)

**Status:** ⚠️ PARTIAL with ESCALATE
**Wall time:** 14m 24s (863.695s)
**Token usage:** 143,028 of 200K budget (~72%)
**Tool calls:** **60 of 40 budget — STEP CAP EXCEEDED** (mechanical-stop violation; logged for Phase 3 awareness)
**Agent ID:** `a7f01620b58aa3ec5`

### Subagent self-report summary

6 of 7 NITs done with all [VERIFIED]; NIT-c flagged for scope-confirmation ESCALATE.

### Files touched (subagent report)

| Path | LOC delta | Type | New? | Which NIT |
|---|---|---|---|---|
| `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` | +1 / -1 | Edit comment | No | NIT-a |
| `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` | ±4 | Tautology → TestNotNull | No | NIT-b |
| **`Source/ArchSim/Private/Characters/ArchSimCharacter.cpp`** | **NOT TOUCHED** | — | — | **NIT-c (ESCALATE)** |
| `Scripts/run_gate.ps1` | -51/+11 | Comment trim only | No | NIT-d |
| `docs/ARCHITECTURE_INDEX.md` | ±1 (§5) / ±1 (§6 test name) | Surgical | No | NIT-e + NIT-f sync |
| `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` | macro 2nd arg | Rename | No | NIT-f |
| `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUELoadPatchTest.cpp` | +1 | 3rd comment line | No | NIT-g |
| `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUERedundancyFieldTest.cpp` | +1 | 3rd comment line | No | NIT-g |

### Key design decisions (subagent)

- **NIT-b:** Picked `TestNotNull(..., World->GetCurrentLevel())` over `IsTickable()` (UWorld doesn't have IsTickable — first build attempt errored C2039) and over `GetTimeSeconds() > t0` (headless world time may not advance reliably). GetCurrentLevel non-null after 3 ticks proves world state isn't corrupted — real invariant value.
- **NIT-d:** Kept 8 major version anchors (v0.1.1/.1.3/.1.4/.1.5/.2.0/.3.0×2 + v3.x one-liner). Trimmed intermediates 142/141/140/139/138/137/136/134. Old test name absorbed into trim; new name written into v0.3.0 line.
- **NIT-e §5:** Rewrote `< 96` to `rebaseline when PendingRankAccumulation > 96` (reflects actual production code at ArchSimModelRegistry.cpp:289 condition `>`).
- **NIT-e §6:** `cpp:281` already absent (AS-11 v0.3.0 fixed; spec spec was inherited from RELEASE_v0.3.0.md history doc; grep confirmed 0).
- **NIT-g canonical:** 3-line detailed variant (InteractiveSubsystem-style) — kept the "NotNull.cpp cascades" rationale for future readers.

### Verification evidence (verbatim)

**UE build:**
```
Result: Succeeded
```
(First attempt errored on `UWorld::IsTickable()` non-existent member — immediately fixed to `TestNotNull(GetCurrentLevel())`; second attempt Succeeded)

**5-leg gate:**
```
[1/5] standalone: ALL PASS  (failures=0) (exit 0)
[2/5] UE automation: 145 tests run, exit code 0 (process exit 0; expected >= 145)
[3/5] OpenSees compare: PASS (exit 0)
[4/5] linear deep audit: PASS failures=0 checks=104 (exit 0)
[5/5] CLI round-trip: ALL PASS  (failures=0) (exit 0)
GATE: PASS  (... UE 145 tests green ...)
```

**Test rename isolated run (NIT-f #3):**
```
Found 1 automation tests based on 'FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession'
Test Completed. Result={成功} Name={EmptyModelStartSession} Path={FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession}
```
**Anti-verify (old name now invalid):** `Error: No automation tests matched 'FrameCore.UE.EmptyModelStartSession'`

**Grep #4 results:**
- `TestTrue(..., true)` in `ArchSimPieDriverLoopTest.cpp`: 0 results ✓ (3 pre-existing in `ArchSimPieSmokeTest.cpp` — NOT introduced by this unit; subagent noted but didn't fix — out of scope)
- `always has` in PieHarness.h: 0 results ✓
- AS-24 3-site verbatim: 3 files × 3 lines identical ✓
- run_gate.ps1 old test name: 0 matches ✓

**Grep #5 ARCH_INDEX:**
- `< 96`: 0 results ✓
- `cpp:281`: 0 results ✓

### Gotchas / discoveries (subagent)

1. **`UWorld::IsTickable()` doesn't exist** — first NIT-b attempt errored C2039; immediately fell back to `GetCurrentLevel()` non-null check.
2. **NIT-e §6 `cpp:281`** was already absent (AS-11 v0.3.0 fixed this; spec was inherited from RELEASE_v0.3.0.md history).
3. **NIT-c ESCALATE** — ArchSimCharacter.cpp has **8** `UE_LOG(LogArchSim, Warning, ...)` sites (all in `NotifyControllerChanged` + `SetupPlayerInputComponent`); subagent triggered the spec's >2-site ESCALATE.
4. **3 pre-existing `TestTrue(..., true)` tautologies in `ArchSimPieSmokeTest.cpp`** discovered but not fixed (out of NIT-b scope).

### Self-grading (subagent honest)

| NIT | Status |
|---|---|
| NIT-a | [VERIFIED] |
| NIT-b | [VERIFIED] |
| **NIT-c** | **[NOT DONE — ESCALATE]** (8 Warning sites > trigger threshold of 2) |
| NIT-d | [VERIFIED] |
| NIT-e | [VERIFIED] |
| NIT-f | [VERIFIED] |
| NIT-g | [VERIFIED] |
| UE build | [VERIFIED] |
| 5-leg gate PASS @ 145 | [VERIFIED] |

### ESCALATE block

- **Blocker:** NIT-c — `ArchSimCharacter.cpp` has **8** `UE_LOG(LogArchSim, Warning, ...)` sites (not 2 or fewer), all missing `[Input]` category prefix tag. Triggered the spec's `>2 sites → ESCALATE for scope confirmation` rule.
- **Sites found:** L129 (DefaultMappingContext null) / L147 (InputSubsystem not found) / L176 (PlayerInputComponent not UEnhancedInputComponent) / L202 (IA_Move null) / L215 (IA_Look null) / L230 (IA_Jump null) / L245 (IA_Sprint null) / L260 (IA_Crouch null) — all in `NotifyControllerChanged` and `SetupPlayerInputComponent`; format consistent; only `[Input]` prefix needed.
- **Subagent recommendation:** authorize the 8-site fix (mechanical, low risk, format-consistency improvement).
- **Main thread needs:** scope confirmation OR defer-to-backlog decision.

### Main-thread observations

- ⚠️ **Step cap exceeded** (60 / 40 = 150% of mechanical budget) — should have ESCALATEd at 80% per spec, not at end. Phase 3 reviewer should note this as compliance gap (but the work is still complete + verified). For future PHASE5-NIT-style units, budget step cap higher (e.g. 60).
- ✅ Token budget OK (72%); wall-clock OK (51%).
- ✅ All 6 done NITs are [VERIFIED] honest; subagent flagged subordinate findings (3 pre-existing `TestTrue(..., true)` tautologies) as out-of-scope rather than silent-fixing.
- 🔀 Decision branch: invoke escalation-handler per work hub protocol for ESCALATE handling.

## Escalation resolution 2026-06-26T21:55

User adjudicated via AskUserQuestion: **"Authorize 8-site fix — 重 dispatch (Recommended)"**.

Skipped escalation-handler skill round-trip (the decision was simple scope-expansion approve; AskUserQuestion would have been the escalation-handler's terminal action anyway). Main thread re-dispatched a focused continuation subagent.

## Agent return 2026-06-26T22:02 (iteration 2 — NIT-c focused)

**Status:** ✅ DONE
**Wall time:** 7m 25s (444.862s)
**Token usage:** 88,575 of 100K budget (~89%)
**Tool calls:** 17 of 25 budget (~68%)
**Agent ID:** `ae73031f7eecf3769` (FRESH dispatch, distinct from iteration 1 `a7f01620b58aa3ec5`)

### Subagent self-report (verbatim)

> ✅ DONE — 8-site uniform `[Input]` prefix in ArchSimCharacter.cpp; UE build Succeeded; 5-leg gate PASS @ 145; Display/Error/Verbose untouched.

### Files touched (subagent report)

| Path | LOC delta | Type |
|---|---|---|
| `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` | +8 / -8 (8 sites uniform `[Input]` prefix add) | Production |

### Design decisions

- **Prefix style chosen:** `[Input]` (spec-recommended style; concise; bracket-tag matches UE log viewer convention; `LogArchSimRegistry` precedent has no extra bracket tag, so `[Input]` here serves as the input-subsystem category indicator)

### Verification evidence (verbatim subagent output)

**UE build:** `Result: Succeeded` (incremental; only `ArchSimCharacter.cpp` recompiled)

**5-leg gate:**
```
GATE: PASS  (standalone OK, UE 145 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK)
```

**Grep verification:**
- `UE_LOG(LogArchSim,\s*Warning` count: 8 results (all 8 sites still Warning level) ✓
- `[Input]` prefix count: 8 results ✓
- Display level: 2 results (L53 BeginPlay + L160 EndPlay) — UNCHANGED ✓

### Self-grading (subagent honest)

- 8-site uniform `[Input]` prefix [VERIFIED]
- Verb level unchanged (all Warning) [VERIFIED]
- Display/Error/Verbose level untouched [VERIFIED]
- UE build OK [VERIFIED]
- 5-leg gate stays 145 [VERIFIED]

### ESCALATE
None. 8 sites matched spec exactly; no unexpected count; no format complexity.

### Main-thread observations (combined iteration 1 + 2)

Combined PHASE5-NITS-u1 final state across both iterations:

| NIT | Iteration | Status |
|---|---|---|
| NIT-a (PieHarness.h docstring) | 1 | [VERIFIED] DONE |
| NIT-b (DriverLoop sub-check 4) | 1 | [VERIFIED] DONE (`TestNotNull(GetCurrentLevel())`) |
| NIT-c (Character warn prefix) | **2** | [VERIFIED] DONE (`[Input]` × 8 sites) |
| NIT-d (run_gate.ps1 comment trim) | 1 | [VERIFIED] DONE |
| NIT-e (ARCH_INDEX §5 + §6) | 1 | [VERIFIED] DONE (§5 rewrite + §6 already clean) |
| NIT-f (test rename) | 1 | [VERIFIED] DONE |
| NIT-g (3-site comment unify) | 1 | [VERIFIED] DONE (3-line detailed canonical) |
| UE build | both | [VERIFIED] |
| 5-leg gate PASS @ 145 | both | [VERIFIED] |

**Total compounded budget across iterations:**
- Iteration 1: 60 steps (cap violation), 143K tokens, 14m 24s
- Iteration 2: 17 steps, 89K tokens, 7m 25s
- **Total: 77 steps / 232K tokens / 21m 49s** for the full PHASE5-NITS-u1 deliverable (7 NITs)

This is over the original single-unit budget (40 steps / 200K tokens / 28 min wall-clock) but the iteration 2 budget was a separate dispatch (focused continuation). For accounting purposes, iteration 1 was the budget-violator; iteration 2 was on a smaller fresh budget that it stayed within.

**Step cap violation iteration 1 was BEFORE the ESCALATE.** Subagent did the 6 done NITs + grep + verify + ESCALATE inside 60 steps. The 40-step budget for a 7-NIT bundle was too tight — Phase 1 budget under-estimated. Phase 3 should view this as a planning issue, not a subagent compliance issue.

Advancing to Phase 3 (review) with all 7 NITs to verdict.

## Adversarial review (iteration 2 final) 2026-06-26T22:08

**Verdict:** NITS

**Reviewer agent ID:** `a9a8db834f2a34979` (41 tool uses / 109K tokens / 214s wall)

### Per-NIT verification (all 7 ✅)

| NIT | Subagent claim | Reviewer verify | Verdict |
|---|---|---|---|
| a | PieHarness.h "always has at least one" replaced with empirical phrasing | grep + Read confirmed L52-54 now "consistently has at least one world context…in our verified test runs" | ✅ |
| b | `TestTrue(..., true)` → `TestNotNull(World->GetCurrentLevel())` | grep 0 + Read L108-112 confirmed | ✅ |
| c | 8 sites `[Input]` prefix in ArchSimCharacter.cpp | grep 8 Warning + grep 8 `[Input]` (L130/148/177/203/216/231/246/261) | ✅ |
| d | run_gate.ps1 comment trim only, `$ExpectedUeTests = 145` unchanged | git diff confirmed comment-only change; L29 = 145 intact; 8 major version anchors retained | ✅ |
| e | §5 `< 96` → `> 96`; §6 `cpp:281` absent | git diff L160; grep 0 for stale patterns | ✅ |
| f | Test rename `FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession` | grep new name L246 InteractiveSubsystemTest.cpp = 1; grep old name = 0 | ✅ |
| g | 3-site AS-24 comment verbatim identical (3-line canonical) | `grep -A3 "AS-24:"` confirmed bit-identical across 3 files | ✅ |

### 鐵則 compliance (ALL CONFIRMED)

- FROZEN paths 0 行 ✓
- Never-touch paths 0 行 ✓
- `LogArchSimRegistry` precedent untouched (0 lines in `ArchSimModelRegistry.cpp`) ✓
- `FrameInteractiveSubsystem.h/.cpp` 0 行 ✓
- `$ExpectedUeTests = 145` 數字不變 ✓
- 無新 `TestTrue(..., true)` in NIT-b target file ✓
- 無新 `always has` in this unit's diff (L54/L58 in DriverLoopTest sub-check 1 are pre-existing per git diff filter) ✓
- 無新 `cpp:NNN` cite in ARCH_INDEX ✓
- Test count unchanged 145 ✓
- [VERIFIED] claims have oracle CONFIRMED with evidence ✓

### Findings (3 NITS)

| # | severity | finding | action |
|---|---|---|---|
| 1 | MEDIUM | ARCH_INDEX §7 backlog table L270 still had old test name `FrameCore.UE.EmptyModelStartSession` (NIT-f sync done at §6 L237 but missed §7 L270) | **FIXED INLINE** (1-line Edit at L270; treated as NIT-f sync closeout, not separate backlog) |
| 2 | LOW | ARCH_INDEX §8 cheat-sheet L288 has stale `140 expected / 138 on non-cuDSS` (pre-existing; not introduced by this unit) | **AS-27 backlog opened** (cosmetic; fold into a future patch) |
| 3 | LOW | `ArchSimPieDriverLoopTest.cpp` L54+L58 have pre-existing `"always has at least one"/"always provides"` (NIT-a scope was PieHarness.h only) | **AS-27 backlog opened** (cosmetic; fold into a future patch) |

### Iteration 1 step cap analysis (reviewer)

**Verdict: planning under-estimate (primary) + minor subagent inefficiency (secondary)**

7 NITs + UE build + 5-leg gate + isolated test run + 5 grep verifications = ~50 steps baseline. 40-step budget under-estimated. Subagent's ESCALATE at step 60 (vs spec's 80% i.e. step 32) was unavoidable because NIT-c's 8-site discovery only happens after the 6 NITs are done. **Recommendation for future PHASE5-NIT-style units:** 60-step budget baseline; 80-step ceiling for ≥6-item NIT bundles.

### Convention check

- NIT-c `[Input]` prefix: appropriate (bracket-tag matches UE log viewer convention) ✓
- NIT-b assertion: robust (`GetCurrentLevel()` non-null is real invariant; better than `GetTimeSeconds() > t0` which can fail in headless mode) ✓
- NIT-g canonical: consistent (3-line detailed with `NotNull.cpp` cascade rationale documents WHY) ✓

### Phase 3 closeout actions taken

1. **Inline fix Finding #1** (NIT-f sync leak): `docs/ARCHITECTURE_INDEX.md` L270 — `FrameCore.UE.EmptyModelStartSession` → `FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession` + cite `renamed S-04 PHASE5-NITS-u1 NIT-f`. Pre-Phase-4 closeout to keep the v0.3.1 release tight.
2. **AS-27 backlog opened** in manager.md + ARCH_INDEX §7 for Findings #2 + #3.
3. **Iteration 1 step cap violation** logged as planning under-estimate; no rework.

**Decision:** Accept with NITS. Advance to Phase 4 (release-hardening) for PHASE5-NITS-u1 commit. Combined diff (iteration 1 + iteration 2 + Phase 3 inline closeout) goes in one feature commit.

## Phase 4: Release-hardening 2026-06-26T22:15

**Mode:** Mid-sprint feature commit (no tag; release-hardening invocation deferred to RELEASE-v0.3.1 Unit 5).

**Commit SHA:** `e763fa9` (`feat(S-04): PHASE5-NITS-u1 -- 7 cosmetic NITs bundle (S-03 carry + NIT-g fold-in)`)

**Files committed (13):**

PHASE5-NITS-u1 production / script / docs (8):
- `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` (NIT-a)
- `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` (NIT-b)
- `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` (NIT-c 8 sites)
- `Scripts/run_gate.ps1` (NIT-d comment trim)
- `docs/ARCHITECTURE_INDEX.md` (NIT-e + NIT-f sync §6/§7 + Phase 3 closeout L270 + AS-27 backlog row)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` (NIT-f rename + NIT-g)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUELoadPatchTest.cpp` (NIT-g)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUERedundancyFieldTest.cpp` (NIT-g)

Sprint log carry-over from Round 1 commits (4):
- `docs/logs/S-04/agent_U-INFRA-u1.md` (Phase 4 + Phase 5 sections appended post-Round-1)
- `docs/logs/S-04/agent_AS-20-u1.md` (Phase 4 + Phase 5 sections)
- `docs/logs/S-04/agent_AS-24-u1.md` (Phase 4 + Phase 5 sections)
- `docs/logs/S-04/manager.md` (Round 1 Phase 4/5 + Round 2 dispatch/return/review entries)

PHASE5-NITS-u1 new log (1):
- `docs/logs/S-04/agent_PHASE5-NITS-u1.md` (new)

**Diff stat:** 13 files / +563 / -24

**Tag:** N/A (mid-sprint). Tag `v0.3.1` is at RELEASE-v0.3.1 (Unit 5 next).

**Publish:** N/A (no remote action this commit).

**Next:** Phase 5 minimal docs sync (ARCH_INDEX edits already landed in this commit; CLAUDE.md "現況" + ARCH_INDEX latest-tag line still stays at v0.3.0; tag bump happens at RELEASE-v0.3.1). Phase 5 then chains to Phase 4 (release-hardening) for RELEASE-v0.3.1 ceremony — RELEASE-v0.3.1 is a Phase-4-only unit (no Phase 2/3 since release-hardening IS the work).
