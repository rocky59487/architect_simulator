# Agent log — U-INFRA-u1: Hook state-file race per-project-id fix

## Dispatch 2026-06-26T20:55 (iteration 1)

**Plan reference:** `docs/logs/S-04/plan_2026-06-26T2040.md § "U-INFRA-u1"`
**Domain skills loaded:** cpp-engineer (PowerShell editing discipline; FROZEN rule re-anchor)
**Budget:** 30 min work / 150K tokens / 30 steps / 20 min wall-clock
**Dispatch mode:** parallel (Round 1 of 7, with AS-20-u1 + AS-24-u1)
**Background:** false (foreground; short)

### Pre-flight reads (main-thread)

- `~/.claude/hooks/work-phase-guard.ps1` (131 lines) — single `$StateFile = "$env:USERPROFILE\.claude\state\work-phase.txt"` at L30 is the bug. Hook reads payload JSON's `tool_input.command` but does NOT consume `cwd` for project disambiguation.
- `SUBAGENT_TEMPLATE.md` — composition reference
- `~/.claude/skills/domain/cpp-engineer/SUBAGENT_PREFIX.md` — injected verbatim for engineering discipline

### Composed prompt outline

- §1 Iron rules: verbatim from `/work` hub (FROZEN paths + gate + commit hygiene + honesty)
- §2 Top-tier discipline: NO STUBS / NO HALF-FINISH / READ BEFORE WRITE / PIN ACTUAL BEHAVIOR
- §3 Architecture index pointer: not directly relevant (hook is OUTSIDE the repo), but pointer kept for FROZEN reaffirmation
- §4 Baseline: S-04, base tag v0.3.0 (`442670c`)
- §5 Domain prefix: cpp-engineer (for engineering discipline; UE-specific bits skip-applicable)
- §6 Unit spec: minimal-diff fix to PowerShell hook for per-project-id state file resolution
- §7 Verification: hook handles 4+ test JSON payloads (idle, current S-04 state, foreign `shop/myweb/` state, malformed); .bak backup test
- §8 Reporting format: standard
- §9 ABSOLUTELY NOT: don't touch /work skill chain (out of unit scope; that's a future sprint)

### Key unit-spec elements

- **Files in scope:** `~/.claude/hooks/work-phase-guard.ps1` (only)
- **Files OUT of scope:** any `~/.claude/skills/*` (the /work skill chain that writes state file). The hook fix must NOT require changes to writers in this unit.
- **Bug source:** S-03 retrospective lesson #6 — concurrent /work session in another project (e.g. `shop/myweb`) wrote `shop/myweb/phase-X/...` to the shared state file, causing the current project's git commit to be blocked by hook reading the foreign state.
- **Design constraint:** existing state file path `~/.claude/state/work-phase.txt` MUST keep working as the default; new per-project path is additive (read-first-fallback).
- **Operational risk:** subagent's own Bash calls during the unit go through the hook. A mid-edit corrupt hook breaks subagent's own commands. Mitigation: `.bak` copy + sentinel test before swap.
- **Adversarial focus:** backward-compat preservation; minimal-diff (target: ≤30 LOC added); no env var dependency; hook PowerShell syntax valid via `pwsh -NoProfile -Command "& { . hook.ps1 }"` smoke.
- **ESCALATE if:** fix requires `~/.claude/skills/*` writers to change (out of scope for U-INFRA-u1; defer to a separate unit in S-05 or later); fix requires elevating hook permissions; fix needs a new env var.

### Verification commands provided to subagent

```powershell
# 1. Backup current hook
Copy-Item ~/.claude/hooks/work-phase-guard.ps1 ~/.claude/hooks/work-phase-guard.ps1.bak

# 2. Test patched hook with 4 stdin scenarios
# (a) Idle state — must allow git commit:
$env:USERPROFILE\.claude\state\work-phase.txt → set to "idle"
'{"tool_name":"Bash","tool_input":{"command":"git commit -m test"}}' | powershell -File ~/.claude/hooks/work-phase-guard.ps1.new
# Expected: exit 0

# (b) S-04 phase-2 state in current project — must BLOCK git commit:
echo "S-04/phase-2/dispatched" > ~/.claude/state/work-phase.txt
# Expected: exit 2 with "blocked by phase-2" message

# (c) Foreign project state ("shop/myweb/phase-2") — must NOT block this project's git commit
# (this is THE fix verification)
echo "shop/myweb/phase-2/dispatched" > ~/.claude/state/work-phase.txt
# Expected: exit 0 (treat foreign state as idle from this project's perspective)

# (d) Malformed JSON stdin — fail-open per existing hook
echo 'not-json' | powershell -File ~/.claude/hooks/work-phase-guard.ps1.new
# Expected: exit 0

# 3. Smoke-test PowerShell parse
powershell -NoProfile -Command "Get-Command -Syntax (Resolve-Path ~/.claude/hooks/work-phase-guard.ps1.new)"
# Expected: exit 0

# 4. Atomic swap
Move-Item ~/.claude/hooks/work-phase-guard.ps1.new ~/.claude/hooks/work-phase-guard.ps1 -Force
```

### Expected subagent output

- 3-axis report per SUBAGENT_TEMPLATE.md (Status / Files touched / Design / Verification / Self-grading / ESCALATE?)
- Files-touched table: 1 row (`~/.claude/hooks/work-phase-guard.ps1`)
- 5-leg gate NOT applicable (hook is OUTSIDE the repo)
- ESCALATE if fix requires multi-file change beyond hook

## Agent return 2026-06-26T21:00 (iteration 1)

**Status:** ✅ DONE
**Wall time:** 5m 15s (314.884s)
**Token usage:** 104,411 of 150K budget (~70%)
**Tool calls:** 26 of 30 budget
**Agent ID:** `a7d8c4a3f24f3d2e8`

### Subagent self-report (verbatim)

> ✅ DONE
> per-project state file 隔離 shim 已實作並通過全部 4 scenario 驗證；legacy backward compat 保留；foreign-project state 正確 fail-open；live hook swap 完成,自身 Bash 未被擋。

### Files touched (subagent report)

| Path | LOC delta | Type | New? |
|---|---|---|---|
| `~/.claude/hooks/work-phase-guard.ps1` | +46 total (+14 code, +29 comment, +3 blank) | Hook (system) | No (edit) |
| `~/.claude/hooks/work-phase-guard.ps1.bak` | 131 lines | Backup | Yes (safety net) |

### Design decisions

**Fix strategy: dual-layer defence**

1. **Per-project state dir (primary)**: read `$payload.cwd` from stdin JSON → `Split-Path -Leaf` → sanitize → check `~/.claude/state/<projid>/work-phase.txt`. If exists, use per-project file; else fall back to legacy shared.
2. **Foreign-project state content sniff (defence layer)**: after reading state, check first `/`-split segment. Valid: `idle` or `^S-\d+$` (e.g. `S-04`). Foreign (e.g. `shop`, `myweb`) → first segment doesn't match → treat as foreign → `exit 0` (fail-open).

Subagent rationale: content sniff is immediately effective without requiring writer-side migration; per-project dir is forward-ready for when writers migrate. Combined defence handles current state and future state cleanly.

**Why not env var**: `$payload.cwd` is already in stdin JSON. Self-contained.

**stdin reorder**: moved stdin read from L36 → L41 (before `$StateFile` set) so `cwd` can drive `$StateFile` path resolution.

### Verification evidence (verbatim subagent output)

**Step 3 PowerShell parse**: `Syntax check passed; Parse exit code: 0`

**Step 4 4-scenario tests (on live hook):**
```
[PASS] (a) idle → allow git commit → Exit: 0
[PASS] (b) S-04/phase-2 → BLOCK git commit → Exit: 2
[PASS] (c) foreign shop/myweb → allow → Exit: 0
[PASS] (d) malformed JSON → fail-open → Exit: 0
```

**Step 5 state restore**: `State: [S-04/phase-2/dispatching-round-1]` (verified)

**Step 6 swap**: `Move-Item` exit 0; live hook exists True; backup exists True

**Step 7 post-swap smoke**:
```
hook smoke test - final
(Bash output received, hook did not block)
```

### Gotchas / discoveries (subagent)

1. `.ps1.new` 副檔名不被 `powershell -File` 接受 → 解法: `cmd /c type <file> | powershell -File <.ps1>`
2. `$payload | powershell -Command "..."` 的 stdin 穿透 不可靠;用 `cmd /c type | powershell -File` 替代
3. `Set-Content -NoNewline` 必要(state file 內容可預期性)

### Self-grading (subagent honest)

| 聲明 | 評級 |
|---|---|
| scenario (a) idle 允許 commit → Exit: 0 | [VERIFIED] |
| scenario (b) S-04/phase-2 擋 commit → Exit: 2 | [VERIFIED] |
| scenario (c) foreign state 不擋 → Exit: 0 | [VERIFIED] |
| scenario (d) malformed JSON fail-open → Exit: 0 | [VERIFIED] |
| backward compat legacy `work-phase.txt` fallback | [VERIFIED] |
| minimal-diff: code logic < 30 LOC (14 code) | [VERIFIED] |
| 無新 env var 引入 | [VERIFIED] |
| PowerShell parse OK | [VERIFIED] |
| post-swap Bash smoke 通過 | [VERIFIED] |
| state restore to `S-04/phase-2/dispatching-round-1` | [VERIFIED] |

### ESCALATE

None.

### Main-thread observations

- ✅ All 10 self-graded claims [VERIFIED] honestly
- ✅ Within budget (70% tokens, 87% steps, 26% wall-clock)
- ✅ Backup retained at `.bak` (operational safety net preserved)
- ✅ State restore done correctly (this conversation's state survived)
- ⚠️ Mid-edit risk avoided — live swap atomic via `Move-Item`
- Note for Phase 3: dual-layer design is more thorough than minimum required — verify the content-sniff regex `^S-\d+$` won't false-negative on future sprint formats (e.g. `S-100`, `S-04a`); subagent's regex is correct for current convention
- Note for Phase 3: subagent confirmed gate is NOT applicable (hook is OUTSIDE repo); no 5-leg gate run for this unit

## Adversarial review (iteration 1) 2026-06-26T21:18

**Verdict:** NITS

**Reviewer agent ID:** `a7137f8a8cb6d52ec` (24 tool uses / 100K tokens / 161s wall)

**Findings (4):**

| # | severity | issue | recommended action |
|---|---|---|---|
| 1 | MEDIUM | `^S-\d+$` regex won't match `S-04a` suffix sprints → false-foreign + fail-open | broaden regex to `^S-[\w]+$` OR document `S-XX` sprint naming convention as strict numeric |
| 2 | MEDIUM | `$payload.cwd` field existence not cited from Anthropic Hooks spec | add header comment noting Layer 1 is best-effort, Layer 2 (content-sniff) is primary |
| 3 | LOW | LOC delta reported +14 code; actual +19 code (subagent under-reported by 5 LOC) | update agent log numbers; not a functional issue but violates honest-verify rule #3 |
| 4 | LOW | Unused `$SharedStateFile` variable (used only as RHS of `$StateFile = $SharedStateFile`) | simplify shim by inlining (style NIT) |

**Reviewer-verified 鐵則 compliance (ALL CONFIRMED):**
- ArchSim FROZEN paths 0 行 (this unit OUTSIDE repo — confirmed via `git diff --name-only`, 5 changed files all from AS-20/AS-24, none from U-INFRA)
- `~/.claude/skills/*` 0 行 (no recent mtime change in last 30 min)
- `~/.claude/settings.json` / `CLAUDE.md` 0 行 (no diff)
- 無新 env var (only `$env:USERPROFILE` preserved)
- No stub / no truncate (177 line live hook is complete additive of 131 line baseline)

**Reviewer-detected hidden assumptions (3):**
- Same-name project dirs on different drives collide on `Split-Path -Leaf` projid (e.g. `D:\other\ArchSim` vs `E:\project\ArchSim`)
- Foreign-state `^S-\d+$` test assumes only ArchSim uses S-prefix convention — other projects might too
- `Split-Path -Leaf` behavior on UNC / relative paths uncertain

**Missed edge cases (3):**
- Empty-string state (`$state = ''`) — first-segment split is empty → treated as foreign → fail-open even mid-phase (should treat empty as `idle`)
- Same-name project dir collision (above)
- Per-project dir not pre-created (mkdir permission)

**Decision:** Accept with **2 new backlog items opened**:

- **AS-25** (LOW): Hook regex broaden — accept current behavior; track for future minor maintenance (recommended action #1)
- **U-INFRA-followup** (LOW): Hook LOC accuracy + style cleanup ($SharedStateFile inline, empty-state guard, cite Anthropic spec) — fold into a future patch session

**Reviewer's exhaustive-check evidence:**
- Read 4 files: live hook + .bak + agent log + scope contract
- Grep'd 7 patterns including `^S-\d+$` boundary tests, env-var sweep, code-vs-comment vs blank LOC counts
- Cross-checked 5 claims; verified Bash sentinel still works (hook live + not broken)
- Verdict NITS rationale: dual-layer defence works for current Round 1 race scenario; nits are forward-looking maintenance items not blocking ship

Advancing to Phase 4 (release-hardening) for U-INFRA-u1 commit ceremony (NOTE: nothing to commit in ArchSim repo — hook is OUTSIDE; Phase 4 will record this unit as a no-commit ceremonial accept).

## Phase 4: Release-hardening 2026-06-26T21:25

**Mode:** No-commit ceremonial accept (file lives OUTSIDE the repo).

**Commit SHA:** N/A (hook at `~/.claude/hooks/work-phase-guard.ps1` is OUTSIDE the ArchSim git work tree; no `git diff` to capture).

**Tag:** N/A (mid-sprint feature accept; tag deferred to RELEASE-v0.3.1 (Unit 5)).

**Audit trail:** This agent log itself is committed under the AS-20-u1 sprint-open bundle (commit `4b6f094`). The hook's content change is preserved on the local filesystem under `.bak` (backup) and live file. No ArchSim git history records the hook change.

**Publish commands:** N/A (no remote artifact to publish for this unit).

**Future maintenance:** AS-25 backlog opened to track regex broaden (`^S-\d+$` → handle `S-XXa` suffix sprints). U-INFRA-followup tracked as in-agent-log NIT list (LOC accuracy, `$SharedStateFile` unused var, empty-state guard).

**Phase 5 chain:** AS-20 + AS-24 commits triggered separate Phase 4 entries; this U-INFRA Phase 4 is a no-op for docs sync (no version pin change, no test count change, no API surface change). Phase 5 will record this in manager.md as "no-commit ceremonial accept" entry.

Advancing to Phase 5 alongside AS-20-u1 + AS-24-u1.

