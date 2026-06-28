# Sprint S-07 — Manager log

> **Sprint opened:** 2026-06-28 (immediately after S-06 close on v0.5.0)
> **Sprint goal:** ship v0.5.1 with AS-35 PIE auto-smoke via UE Automation Test framework + LatentCommands (C++) integrated as gate's 6th leg
> **Scope contract:** [`scope_2026-06-28T0938Z.md`](scope_2026-06-28T0938Z.md)
> **Execution plan:** [`plan_2026-06-28T0938Z.md`](plan_2026-06-28T0938Z.md)

---

## 2026-06-28T1025Z — AS-35-u1 accepted with NITS

**Phase:** 3 (adversarial review) → 4 (release-hardening) pending
**Unit:** AS-35-u1 — PIE Auto-Smoke C++ test class + Build.cs verify + isolated PASS
**Iteration:** 1 (first-pass review acceptance)
**Subagent:** general-purpose, ue5-engineer + cpp-engineer SUBAGENT_PREFIX injected (agent ID `a51ff1f07dd6ea74b`)
**Reviewer:** general-purpose, read-only (agent ID `a5296cca26fb01503`)
**Wall time:** 24m (subagent) + 3m (reviewer) = ~27 min; well under 1.5h Phase 2/3 budget for u1
**Token usage:** subagent 127K of 200K cap (63%); reviewer 111K (no output cap concern)
**Tool calls:** subagent **147 of 40 budget** (overrun — retrospective item for Phase 6); reviewer 24

### Outcome

- ✅ NEW file `Source/ArchSim/Private/Tests/ArchSimPortalFramePIESmokeTest.cpp` (443 LOC after main-thread NIT-1 inline fix)
- ✅ `Source/ArchSim/ArchSim.Build.cs` 0-line edit (no `AutomationController` dep needed)
- ✅ FROZEN paths 0-line touch (`git diff --stat Plugins/FrameSolver/Source/FrameCore/ Plugins/LevelSim/` empty)
- ✅ Test PASSes in isolation: `Saved/Logs/ArchSim.log` `Result={成功}` for `ArchSim.PIE.PortalFrameSmoke` at exit 0
- ✅ Screenshot artifact `Saved/Screenshots/WindowsEditor/v0_5_x_pie_smoke00000.png` (15,497 bytes, 1526×532)
- ✅ Iron rules #1, #2, #3, #4, #5, #6 ALL CONFIRMED by reviewer

### NITs (4 total)

| # | severity | resolution |
|---|---|---|
| 1 | MEDIUM (misleading WHY comment on COMPLEX vs SIMPLE) | **FIXED INLINE by main thread before Phase 4** — see agent_AS-35-u1.md § "Main-thread inline fix" |
| 2 | MEDIUM (Bug C root cause diagnosis precision) | **Opened as AS-36 backlog**; spawn_task `task_8cf96d94` superseded |
| 3 | LOW (DEFINE order vs execution order — readability) | **Deferred, no AS-XX**; sub-30 LOC reordering, not worth tracking formally |
| 4 | LOW (`ProductionFilter` typo in dispatch narrative — code uses correct `ProductFilter`) | **Phase 5 docs sync will edit dispatch log narrative**; no code change needed |

### Reasons for accept

- Verdict NITS (not BLOCKER) — no fabricated `[VERIFIED]`, no FROZEN violation, no silent stub, no anti-goal violation
- Reviewer evidence exhaustive (10 files read, 6 grep patterns, 8 cross-checked claims)
- All 8 plan-§2 adversarial_focus dimensions covered
- Discovered side-bug (Bug C `PlaceKSetMember` two-column-same-node-pair) correctly out-of-scope and tracked

### New backlog items opened this cycle

- **AS-36** — Fix `PlaceKSetMember` two-K1-column-same-node-pair bug. Symptom in headless commandlet PIE: `SC2b` log shows `Member[0] I=2 J=3` and `Member[1] I=2 J=3` (two columns sharing endpoint node pair) → LDLT factorization rank-deficient → solve silently fails → HeatmapActor never spawns. Root cause likely either (a) FindOrAddNode 1 mm dedup mis-snaps columns whose I-end coincides with B-support node and J-end stacks vertically into a position another column reaches via offset rotation, OR (b) `EndIOffsetUE` / `EndJOffsetUE` calculation in `PlaceKSetMember` for stacked columns produces colinear-offset endpoints. Subagent's spawn_task `task_8cf96d94` (description "actor-transform identity") may mis-locate the cause. **Owner: next session subagent under separate `/work` invocation.** Repro: run `ArchSim.PIE.PortalFrameSmoke` in commandlet; check `Saved/Logs/ArchSim.log` for SC2b `Member[0] I=2 J=3 / Member[1] I=2 J=3`. Verify in user-driven PIE separately to confirm whether bug is commandlet-only (likely it's not, since solve would also fail in user-driven PIE — possible the user-driven path generates non-identical transforms via Slate/click path).
- **AS-37** — Document or fix ALS commandlet PIE crash. Symptom: `AArchSimCharacter` spawn in commandlet PIE crashes because ALS `LoadObject<T>()` for plugin content (`SKM_Als` / `CS_Als_Default` / etc.) fails at the timing PIE pawn instantiates → `MovementSettings` null → `NotifyLocomotionModeChanged()` `EXCEPTION_ACCESS_VIOLATION`. User-driven PIE doesn't hit this because Editor pre-mounts plugin content. AS-35-u1 test sidesteps via test-local `WorldSettings->DefaultGameMode = AGameModeBase::StaticClass()` override; production unchanged. **Decision needed:** (a) document as known commandlet-only limitation, or (b) investigate ALS LoadObject timing further (extension of S-06 U-ALS work).

### Retrospective notes for Phase 6 (carried forward)

1. **Tool-call cap overrun (147 vs 40):** Per-unit step cap of 40 is too tight for UE-build-cycle work. Each Build.bat + UnrealEditor-Cmd test invocation + log inspection cluster eats ~5-10 calls in a successful iteration; with one rebuild loop it doubles. Consider raising default for `ue5-engineer`-tagged units to 80-100, OR explicitly noting in plan that build-iterative units exceed 40 typical.
2. **Bug-discovery during unit work:** AS-35-u1 discovered a real production bug (Bug C) and a real platform-edge bug (ALS commandlet crash) that were out-of-scope. The session subagent honored scope and spawn_task'd / documented. Phase 3 reviewer correctly elevated the discoveries to backlog AS-XX. This pattern works well; preserve it.
3. **Spawn_task chip vs AS-XX backlog:** `task_8cf96d94` was a session-only UI chip; AS-36 is the durable backlog. Phase 3 protocol should explicitly note: discoveries from dispatch units should land as AS-XX backlog (not just chips) when they're MEDIUM+ severity. Chips remain useful for follow-up suggestions the user might or might not want to action.

---

---

## 2026-06-28T1105Z — AS-35-u2 accepted with NITS

**Phase:** 3 (adversarial review) → 4 (release-hardening) pending
**Unit:** AS-35-u2 — PowerShell gate wiring + 6-leg integration (`Scripts/run_pie_gate.ps1` NEW + `Scripts/run_gate.ps1` M)
**Iteration:** 1 (first-pass review acceptance)
**Subagent:** general-purpose, ue5-engineer inline + PowerShell expertise (agent ID `aa9219ccfe1df9d8b`)
**Reviewer:** general-purpose, read-only (agent ID `ad31a6bbadf50ef1c`)
**Wall time:** 28m (subagent) + 3m (reviewer) = ~31 min; over scope's 1-1.5h u2 budget by ~5 min but within session budget
**Token usage:** subagent 114K of 100K cap (~14% over); reviewer 110K (no output cap concern)
**Tool calls:** subagent **40 of 25 cap** (~60% over — anticipated per dispatch raise-if-needed); reviewer 26

### Outcome

- ✅ NEW file `Scripts/run_pie_gate.ps1` (170 LOC) — PowerShell wrapper for PIE test commandlet + log parsing + screenshot artifact verification
- ✅ EDIT `Scripts/run_gate.ps1` (+55 / -19 lines) — leg 2 filter changed to category enumeration (Option A) + new leg 6 block added + verdict line updated + label bumps [N/5] → [N/6]
- ✅ Decision documented: **Option A** (leg 2 filter category enumeration) over Option B (test-side skip) or Option C (`?` exclusion) — justified by avoiding edit to u1's already-reviewed test
- ✅ FROZEN paths 0-line touch
- ✅ Full 6-leg gate ALL PASS verbatim (`GATE: PASS exit 0`)
- ✅ u1 test file untouched (still `??` in `git status`)
- ✅ Iron rules #1, #2, #3, #4, #5, #6 ALL CONFIRMED by reviewer

### NITs (5 total, all documentation/cosmetic)

| # | severity | resolution |
|---|---|---|
| 1 | NIT | **Phase 5 docs sync** — ARCH_INDEX § 6/§ 8/§ 9 "5-leg gate" → "6-leg gate" (3 lines) |
| 2 | NIT | **Phase 5 docs sync** — ARCH_INDEX § 6 namespace convention list add `PIE` category |
| 3 | NIT | **Phase 5 docs sync** — ARCH_INDEX § 6 test inventory add `ArchSim.PIE.PortalFrameSmoke` row + v0.5.1 "Recent additions" bullet |
| 4 | NIT (no action) | scope §1 originally said `$ExpectedUeTests 149→150`; Plan §3 revised to "keep 149, PIE in leg 6". Subagent correctly applied; not a bug |
| 5 | NIT (deferred) | `\| Out-Null` swallows stderr in leg 6 — matches existing leg 2 convention; debug preference only; no AS-XX |

### Reasons for accept

- Verdict NITS — no fabricated `[VERIFIED]`, no FROZEN violation, no silent stub, no anti-goal violation
- Reviewer evidence exhaustive (10 files read, 8 grep patterns, 9 cross-checked claims)
- All 7 plan-§2 adversarial_focus dimensions covered (6 fully YES + 1 PARTIAL = cheat-sheet update handled by Phase 5)
- 14 existing ArchSim tests verified mapped to Option A filter categories with zero silent-drop
- Tool-call + token overruns are process critique, not result critique; result is genuinely PASS

### Missed edge cases (informational, no AS-XX warranted)

- Stale `Saved/Logs/ArchSim.log` false-positive risk (UE log rotation behaviour undocumented — `Select-Object -Last 1` could mis-read leg-2 EXIT CODE as leg-6's in a leg-6-fail-to-start scenario). Spot-check confirmed working empirically; flag here for future debugging.
- Screenshot 00000..00007 = 8 files emitted per test run (test apparently invokes screenshot 8× per tick). Functional but documents real behaviour vs intended "1 screenshot per run".
- `$PieTestResultFound` latent logic — if `Test Completed.` line absent but EXIT CODE 0 somehow, would falsely PASS. Edge case currently unreachable.

### Cumulative session state (after u1 + u2 both accepted)

| Unit | Status | Files |
|---|---|---|
| AS-35-u1 | ACCEPTED (NITS, iter 1) | `Source/ArchSim/Private/Tests/ArchSimPortalFramePIESmokeTest.cpp` NEW |
| AS-35-u2 | ACCEPTED (NITS, iter 1) | `Scripts/run_pie_gate.ps1` NEW + `Scripts/run_gate.ps1` M |
| **Scope** | All 1 task (AS-35) units complete | Phase 4 release-hardening Mode B single-patch ready to ship as v0.5.1 |
| **Backlog opened this session** | AS-36 (`PlaceKSetMember` two-K1-column-same-node-pair) + AS-37 (ALS commandlet PIE crash audit) |

### Next phase

Phase 4 release-hardening — Mode B single-patch (v0.5.0 → v0.5.1), commit covers BOTH u1+u2 deltas in one annotated tag.

---

(Next entry: Phase 4 release-hardening + Phase 5 docs sync + Phase 6 SESSION CLOSE.)
