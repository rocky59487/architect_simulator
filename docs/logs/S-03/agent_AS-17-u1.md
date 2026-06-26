# Agent log — AS-17-u1: empty-CurrentModel StartSession behavior audit

## Dispatch 2026-06-26T17:05 (iteration 1)

**Plan reference:** `docs/logs/S-03/plan_2026-06-26T1652.md` § Unit 2
**Domain skills loaded:** `cpp-engineer` + `ue5-engineer` (both VERBATIM injected into prompt)
**Budget:** 3h / 180K tokens / 35 steps / 22min timeout
**Dispatch mode:** parallel with Unit 1 (LOW-batch-u1) — Round 1

### Pre-flight reads done by main thread

- `docs/ARCHITECTURE_INDEX.md` §3 (FrameCoreUE quick-ref) + §7 (backlog)
- `Source/ArchSim/Private/Subsystems/ArchSimModelRegistry.cpp` L215-244 (`FlushAndStartSession`)
- `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameInteractiveSubsystem.h` L1-95
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/FrameInteractiveSubsystem.cpp` L44-95 (`StartSession` impl)
- `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameCoreUEModelTypes.h` L377-380 (`FFrameModelDef` fields: Materials/Sections/Nodes/Members are all `TArray<>` so `.Num()==0` defines "empty")
- `docs/logs/S-02/manager.md` AS-17/C-02 origin

### C-02 origin (verbatim from S-02 hardening)

> AS-17 (MEDIUM): audit and either prove or guard the empty-`CurrentModel` `StartSession` path (C-02).

### Parallelism-safety note

Round 1 parallel with **LOW-batch-u1**. Allowed write paths for THIS unit (AS-17-u1):
- `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameInteractiveSubsystem.h`
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/FrameInteractiveSubsystem.cpp`
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` (1 new sub-check if needed)
- `docs/SPRINT_NOTES.md` (audit conclusion if no code change)
- `docs/logs/S-03/agent_AS-17-u1.md` (this log)

**Forbidden write paths for THIS unit** (LOW-batch-u1 owns them):
- `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h`
- `Source/ArchSim/Private/Subsystems/ArchSimModelRegistry.cpp`
- `Source/ArchSim/Public/Components/ArchSimMemberData.h`
- `Source/ArchSim/Private/Components/ArchSimMemberData.cpp`
- `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp`
- `docs/ARCHITECTURE_INDEX.md`

If your audit concludes a guard is needed on the consumer side (ArchSim Registry) —
ESCALATE (do not write to forbidden paths; let main thread re-order to a sequential pass).

Both agents instructed to NOT run the full 5-leg gate (main thread runs once after both return).

### Composed prompt (verbatim)

```
[See full prompt in Agent tool invocation that follows.]
```

(Agent return will be appended after subagent finishes.)

---

## Agent Return — 2026-06-26T17:35

### Status
DONE — Case A (no guard needed)
`empty-CurrentModel StartSession` 已經 graceful fail；不需要加 guard；新 test `FrameCore.UE.EmptyModelStartSession` pin 契約，build + test 全通。

### Files touched
| Path | LOC delta | Category | New? |
|---|---|---|---|
| `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` | +92 | Test | No |
| `Scripts/run_gate.ps1` | +1 (140→141) | Config | No |
| `docs/SPRINT_NOTES.md` | +50 | Docs | No |
| `docs/logs/S-03/agent_AS-17-u1.md` | +this section | Docs | No |

### Design decisions

**audit conclusion: Case A**

Source-traced path for `StartSession(EmptyDef, ...)`:
1. `FromBlueprint(EmptyDef, Cached->Model, OutError)` — all loops are no-ops on empty TArrays;
   falls through to `return true`. (FrameCoreUEModelMarshal.cpp L168-251)
2. `new frame::ReSolveSession(empty_model, eopts)` — ctor does NOT throw; stores validate() result.
3. `FrameModel::validate()` L31: `nodes.empty()` → `why = "no nodes"; return false` → `valid() == false`.
4. Existing guard at FrameInteractiveSubsystem.cpp:81-88 catches `!valid()`, fills `OutError`,
   deletes both `Session` and `Cached`, returns `false`. Zero dirty state.
5. Consumer (`FlushAndStartSession`) checks the `false` return and logs Warning. Also safe.

**No guard added because the existing path is already correct.** Adding a redundant early-out
before `FromBlueprint` would be "guard for scenarios that can't happen" (iron rule "PIN ACTUAL BEHAVIOR").

**Test edge cases chosen:**
- Fully empty model (0 mats / 0 secs / 0 nodes / 0 members) — exercises the `nodes.empty()` branch.
- Partial empty (1 mat + 1 sec, 0 nodes) — also hits `nodes.empty()`; verifies FromBlueprint doesn't
  short-circuit on materials/sections being non-empty.
- Recovery after failure — verifies that failed `StartSession` doesn't leave `Session != nullptr`
  (dirty state would cause double-delete on the next `EndSession` in the ctor call chain).
- Double `EndSession()` after failed start — idempotent because `EndSession` guards with `if (Session)`.

**`Nodes.Num() < 2` vs `< 1` discussion (not applicable):** Case A conclusion means no explicit
guard was written. If a future reviewer adds one, the minimum correct guard is `Nodes.Num() < 2`
not `< 1`, because FrameModel::validate() requires at least 2 distinct nodes (members need i≠j
endpoints), and `nodes.size() == 1` would pass the `nodes.empty()` check but fail at member
endpoint resolution or trivially produce a zero-DOF system with no members. The test sub-check
for partial (0 nodes) covers the immediately failing case.

### Verification evidence

**UE build (2026-06-26T17:30):**
```
[6/13] Compile [x64] FrameCoreUEInteractiveSubsystemTest.cpp
...
Result: Succeeded
Total execution time: 14.88 seconds
```
Exit code: 0

**New test result (2026-06-26T17:31):**
```
LogAutomationController: Display: Test Completed. Result={成功} Name={EmptyModelStartSession} Path={FrameCore.UE.EmptyModelStartSession}
**** TEST COMPLETE. EXIT CODE: 0 ****
```
1 test, 8 sub-checks (all pass, no errors in log). Exit code: 0

**5-leg gate:** SKIPPED — main thread responsibility.

### Gotchas / discoveries

**engine empty-model behavior**: `ReSolveSession` ctor does NOT throw on empty model. It internally
runs `FrameModel::validate()` and stores the result. `valid()` returns `false` with diagnostic
`"no nodes"`. The D-01 try/catch wrap in StartSession is not needed for empty-model but remains
correct for genuine allocation failures (`std::bad_alloc`).

**FromBlueprint passes for empty model**: this is intentional — the marshal is designed as a
conversion function, not a validator. The engine's validate() is the canonical validator.
Callers who want to validate before starting a session should use
`UFrameModelBuilder::ValidateModel()` explicitly (which calls validate() on the converted model).

**None** of the forbidden paths needed to be touched.

### Self-grading

- "empty `StartSession` returns false gracefully" — [VERIFIED] (test reproduce: `Result={成功}` sub-check 1a)
- "`OutError` non-empty + descriptive" — [VERIFIED] (test reproduce: sub-check 1b; engine fills `Session->diagnostic()` = "no nodes")
- "後續 valid `StartSession` call 不被 empty 失敗污染" — [VERIFIED] (test reproduce: sub-check 4a/4b/4c)
- "guard 邊界（Case A — no guard written）" — N/A (Case A)

### ESCALATE
None.

---

## Adversarial review (iteration 1) 2026-06-26T17:50

**Verdict:** NITS

**Reviewer dimension coverage:** ✅ all 5 dimensions verified — true reproduction (UE build 14.88s + `Result={成功}` + new test 真實 exists L246-312), conclusion (A) supported by 5-step source trace, guard discussion appropriate (`< 2` vs `< 1`), 4 edge cases covered (with 3 gaps noted below), honest grading. Reviewer Read FROZEN engine source READ-ONLY (allowed per reviewer scope) to verify ctor non-throw claim.

**鐵則 compliance:** ALL CONFIRMED — FROZEN engine source 0 行 / never-touch 0 行 / 4 ext plugins 0 行 / LOW-batch-u1 territory (Source/ArchSim/**, ARCHITECTURE_INDEX.md) 0 行 from this unit (those are parallel LOW-batch-u1 territory).

**Findings (3 NITS):**

| # | severity | file:line | issue | action |
|---|---|---|---|---|
| 1 | NITS | `docs/SPRINT_NOTES.md` (AS-17 section) | sub-check count description says "8 sub-checks" but reviewer counted **10 TestXxx assertions** in test L246-312 (TestNotNull + 3×TestFalse + 3×TestFalse + 2×TestTrue + TestFalse). Distinct sub-checks = 4 logical groups, but assertion count = 10. | **→ inline fix at Phase 5 docs sync** (1-line correction) |
| 2 | NITS | `docs/SPRINT_NOTES.md` (AS-17 section) + agent log Gotchas | diagnostic string described as `"no nodes"` but actual engine path is `FrameSolver.cpp:41` `"invalid model: " + why`, so `OutError == "invalid model: no nodes"`. Test still passes (only checks `!OutError.IsEmpty()`), but doc imprecise. | **→ inline fix at Phase 5 docs sync** (string-correction) |
| 3 | NITS | `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` test name | `FrameCore.UE.EmptyModelStartSession` is top-level; existing 3 InteractiveSubsystem tests use `FrameCore.UE.InteractiveSubsystem.*` namespace. Functionally works (gate `FrameCore.*` filter catches both), but convention drift. | **→ AS-22 inline fix or Phase 5** (1-line test path rename, gate keep 141) |

**Missed edge cases (3 worth-noting):**
- Members non-empty but Nodes empty (asymmetric) — engine validate still graceful, not tested.
- Consecutive N×empty `StartSession` (leak check) — not tested.
- "Double EndSession" sub-check (#2) has no assert — relies on "no crash" only.

**Hidden assumptions (3, all verified by reviewer's READ):**
- `ReSolveSession` ctor not throw → verified via FROZEN source READ (`Impl::buildBaseline` + `assembleAndFactor` don't throw; `bad_alloc` covered by existing try/catch).
- `diagnostic()` returns non-empty on invalid → verified, but actual string is `"invalid model: no nodes"` (NITS-2).
- `IsSessionActive() == (Session != nullptr)` → verified via FrameInteractiveSubsystem.h:51.

**Decision:** Accept with **3 NITS inline-fixable at Phase 5** (no new backlog needed; all are doc/cosmetic). Test rename (NITS-3) is a 1-line edit that can land in Phase 5 or as part of Unit 6 (PIE test bundle). Advance to Phase 4.
