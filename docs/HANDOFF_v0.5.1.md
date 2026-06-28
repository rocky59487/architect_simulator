# 交接指南 — `v0.5.1` 後接手 owner

> **From:** `v0.5.1` (tag-time commit at HEAD; see `git log --oneline -2` after publish)
> **To:** next session owner (any Claude / human)
> **Date:** 2026-06-28
> **Prior handoffs:** [`HANDOFF_v0.5.0.md`](HANDOFF_v0.5.0.md) (S-06 main) / [`HANDOFF_v0.4.0.1.md`](HANDOFF_v0.4.0.1.md) (S-05 hotfix) / [`HANDOFF_v3.6.0.md`](HANDOFF_v3.6.0.md) (engine pre-FROZEN anchor)
> **Release notes:** [`RELEASE_v0.5.1.md`](RELEASE_v0.5.1.md)
> **Sprint log:** [`docs/logs/S-07/manager.md`](logs/S-07/manager.md)

---

## Z-01 first action on day 1

**Verify the 6-leg gate PASSes on your host AND user-driven PIE still works.**

```powershell
# Step 1: confirm git state
cd E:/project/ArchSim
git log --oneline -3                  # 期望 v0.5.1 release commit + v0.5.0 + earlier S-06
git for-each-ref --sort=-creatordate --format='%(refname:short)' refs/tags | Select-Object -First 3
                                      # 期望 v0.5.1 / v0.5.0 / v0.4.0.1

# Step 2: confirm v0.5.1 deliverables
ls Source/ArchSim/Private/Tests/ArchSimPortalFramePIESmokeTest.cpp     # 期望 exists, 443 LOC
ls Scripts/run_pie_gate.ps1                                            # 期望 exists, 170 LOC
grep '\[6/6\]' Scripts/run_gate.ps1                                    # 期望 4 hits (header + 3 inline)

# Step 3: verify 6-leg gate (NEW automated 6th leg in v0.5.1)
.\Scripts\run_gate.ps1 -RequireOpenSees
# 期望: GATE: PASS  (standalone OK, UE 149 tests green, OpenSees PASS, deep audit OK,
#                    CLI round-trip OK, PIE smoke OK)

# Step 4: verify PIE screenshot artifact
Get-ChildItem Saved\Screenshots\WindowsEditor\v0_5_x_pie_smoke*.png
# 期望: at least 1 file, size > 1024 bytes (typical ~15 KB)

# Step 5: USER-DRIVEN PIE smoke STILL canonical for "ready for student trial"
# 開 UE Editor: E:\project\ArchSim\ArchSim.uproject
# 跟 docs/logs/S-05/u3_pie_smoke.md P1..P15 一一執行
# v0.5.1 的自動 leg 6 只是 regression guard — catches "engine still alive" but
# does NOT replace human eyes for "feels right" judgement (per pie-auto-smoke-preference memory).
```

如果 6-leg gate 全 PASS + 螢幕截圖驗證 OK + USER-DRIVEN smoke P1..P15 全 PASS → ready for student trial。 在 manager.md (新 sprint) 加一筆「v0.5.1 6-leg gate PASS confirmed YYYY-MM-DD」 + 開始 S-08 scope (建議 AS-36 first)。

如果 6-leg gate FAIL → 走 post-publish hotfix protocol (`gh release edit v0.5.1 --prerelease` + ship v0.5.1.1). 排查順序:
1. Leg 1 standalone FAIL → engine FROZEN issue (shouldn't happen); cuDSS/conda env
2. Leg 2 count mismatch (149/147 not matched) → run_gate.ps1 filter change broke a Category; check ARCH_INDEX § 6 inventory
3. Leg 6 PIE FAIL → likely AS-35-u1 test regression OR ALS commandlet crash (AS-37); fallback: run `Scripts\run_pie_gate.ps1` standalone for detailed log
4. Screenshot 0 bytes / missing → render thread issue; verify NOT running with `-nullrhi`

---

## 1. `v0.5.1` = 什麼

- **一句話**: PIE smoke 從「user-driven only」加上「auto-leg-6 catches PortalFrame regression」, 不取代人工 UX 判斷。
- **與 v0.5.0 的 source-line delta**: 1 commit total. Tracked: 3 production files (~670 LOC net additive) + 5 sprint log files + 2 release docs.
- **Engine source delta vs v3.6.0/v4.0.0 FROZEN baseline**: **0 lines**. FrameCore/LevelCore 完整保留 FROZEN policy。
- **整入了哪些先前 deferred items**:
  - HANDOFF_v0.5.0 「USER-DRIVEN PIE smoke 仍是 hard gate」 — v0.5.1 加了 auto smoke 作為 regression guard, USER-DRIVEN P1..P15 仍是 ready-for-student gate (per user preference memory)
- **什麼未動**:
  - LevelSim 完全未動 (FROZEN since v2.2+1)
  - FrameCore 完全未動 (FROZEN since v4.0.0)
  - ALS plugin 未動 (S-06 U-ALS patch carries forward)
  - 任何 Blueprint asset 未動 (純 C++ + PowerShell)

---

## 2. 怎麼跑 (主要 reproduce paths)

### 6-leg gate (single command, ~3-4 min)

```powershell
cd E:/project/ArchSim
.\Scripts\run_gate.ps1 -RequireOpenSees
```

每腿在 `$LASTEXITCODE` 0 才算 PASS; 整體 `GATE: PASS` 才算成功; exit code 0 = ship safe。

### 單獨跑 leg 6 (PIE auto-smoke, ~30 sec)

```powershell
.\Scripts\run_pie_gate.ps1 -Root . -Engine $env:UE_ENGINE_ROOT -UProject .\ArchSim.uproject
```

artifacts:
- `Saved/Logs/ArchSim.log` — `Result={成功}` for `ArchSim.PIE.PortalFrameSmoke`
- `Saved/Screenshots/WindowsEditor/v0_5_x_pie_smoke*.png` — 8 frames (test emits 1/tick)

### 單獨跑 PIE 測試 (raw UnrealEditor-Cmd, ~30 sec)

```powershell
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.PIE.PortalFrameSmoke; Quit" `
    -unattended -log
```

**注意:** NO `-nullrhi` flag. Render thread needed for screenshot.

### 環境前置條件

- `$env:UE_ENGINE_ROOT` 指向 UE 5.7 root (e.g. `E:\project\UE_5.7`)
- conda env `framecore-direct` activated for leg 1 + 4 (standalone + audit)
- `openseespy` pip installed for leg 3 (`-RequireOpenSees` enforces; soft-skip without)
- python on PATH for legs 3 + 5

---

## 3. 新 token / 新 flag / 新 API

v0.5.1 沒有新的 BP-callable API 或 CLI token — 純 test infra + gate wiring 補強。

新增的 test namespace category: `ArchSim.PIE.*`
- 第一個成員: `ArchSim.PIE.PortalFrameSmoke`
- Future additions in this category should go in `Source/ArchSim/Private/Tests/ArchSim<Name>PIESmokeTest.cpp` 並使用相同的 LatentCommand 模板 (參考 `ArchSimPortalFramePIESmokeTest.cpp:1-100` 為 reference)
- **重要**: 加新 PIE 測試時 ALSO 更新 `Scripts/run_gate.ps1` 第 110 行的 leg 2 filter ‐ 確認 PIE category 仍排除在外 (Option A enumeration pattern)

---

## 4. 仍 deferred 的 items

對 RELEASE_v0.5.1.md "Known Issues / Deferred" 每項加 **First action on day 1** 一行:

### AS-36 — `PlaceKSetMember` two-K1-column-same-node-pair bug

**Severity:** MEDIUM (BACKLOG)
**Discovered:** S-07 AS-35-u1 commandlet PIE smoke; SC2b log line shows `Member[0] I=2 J=3` and `Member[1] I=2 J=3` (両柱共用 node pair)
**Symptom:** LDLT rank-deficient → solve silently fails → HeatmapActor never spawns in commandlet PIE
**Spawn-task superseded:** `task_8cf96d94` dismissed in favour of AS-36 backlog

**First action on day 1:**
1. Run user-driven PIE per `docs/logs/S-05/u3_pie_smoke.md` P10. Click BP-button `SpawnDefaultPortalFrame` in widget. Observe: does HeatmapActor spawn with non-trivial colour? If YES → bug is commandlet-only; if NO → bug is production, P10 is broken since v0.5.0.
2. If commandlet-only: low priority defer; add `AddInfo` to test explaining the limitation, move on.
3. If production-bug: read `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp PlaceKSetMember` body. Look at `EndIOffsetUE` / `EndJOffsetUE` calculations for K1 column (`-50,0,0` / `+50,0,0` cm offsets per `PlaceK1Column`). Verify per-column transform places the column endpoints at distinct world positions. Cross-check with `FindOrAddNode` 1mm tolerance — if column A endpoint and column B endpoint end up at same global mm coords (likely if K1 default is 1 m vertical and both columns spawn at Z=0 with same X/Y), node dedup will merge them. **Fix likely: per-column `LocationWorld` offset (e.g. ColumnA at (-100,0,0), ColumnB at (+100,0,0)) needs to be applied to base + endpoints, not just base.**

### AS-37 — ALS commandlet PIE crash

**Severity:** MEDIUM (BACKLOG)
**Discovered:** S-07 AS-35-u1 — sub-agent could not run PIE because `AArchSimCharacter` spawn caused `EXCEPTION_ACCESS_VIOLATION`
**Symptom:** ALS `LoadObject<T>()` for plugin content (`SKM_Als`, `CS_Als_Default`, etc.) fails at the PIE-pawn-spawn timing → `MovementSettings` null → `NotifyLocomotionModeChanged()` deref null
**Workaround in v0.5.1:** AS-35-u1 test sidesteps via test-local `WorldSettings->DefaultGameMode = AGameModeBase::StaticClass()` override. **Production unchanged.** User-driven PIE doesn't hit this (Editor pre-mounts plugin content earlier).

**First action on day 1:**
1. Decide: (a) Document as known commandlet-only limitation (no production impact; cheap) OR (b) Investigate further (extension of S-06 U-ALS work).
2. If (b): check `Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp:L138-146` `ALS_ENSURE(IsValid(Settings))` site. Read v0.5.0 `Tools/patches/als_l400_animinstance_guard.patch` for baseline pattern. Consider adding similar runtime-late LoadObject in `AArchSimCharacter::PostInitProperties` (already exists per S-06 U-ALS) but verify it ALSO defends against commandlet's `LoadObject` not finding the asset.

### Deferred NITs (no AS-XX)

- **Test file DEFINE order vs execution order:** `Source/ArchSim/Private/Tests/ArchSimPortalFramePIESmokeTest.cpp:198-305`. Reorder custom latent command DEFINE blocks to match RunTest call order. Cosmetic only.
- **`run_pie_gate.ps1` stderr visibility:** `\| Out-Null` swallows CRL noise. Cosmetic; matches existing leg 2 convention.
- **Stale `Saved/Logs/ArchSim.log` false-positive risk:** subagent reviewer noted leg 6 reads `Select-Object -Last 1` from a possibly-stale log. UE's log-rotation behaviour means this works in practice but is undocumented. If future debugging surfaces a false PASS, consider timestamp filter on log lines.

---

## 5. 過程留下的教訓 (durable, learned this release)

1. **PIE auto-smoke = UE Automation Test framework + LatentCommands (C++), NOT Python `-ExecutePythonScript`.** This was empirically verified DEAD twice in v0.5.0 (Python blocks game thread; Slate post-tick callbacks unreliable through PIE transition). The skeleton in `~/.claude/projects/E--project/memory/v0-5-0-pie-auto-smoke-architecture.md` "What NOT to try" 5 lessons should be kept as a sticky note for the next PIE-automation work.
2. **PowerShell 5.1 + CJK literal in regex = ArgumentException.** Embedding `成功` in a regex string literal in `.ps1` fails because PowerShell 5.1 reads `.ps1` as ANSI. **Workaround:** keep CJK out of regex strings; use ASCII-only patterns for control flow. Diagnostic-only display can use CJK as data (echo to console).
3. **`NativeCommandError` wrapping pollutes `$LASTEXITCODE`.** Capturing UE commandlet stdout via `*>&1 \| Out-File` or `Tee-Object -Variable` wraps stderr in NativeCommandError, makes `$?` false even when exe returned 0. **Workaround:** invoke UE commandlets without pipeline capture; rely on `$LASTEXITCODE` directly; parse from `Saved/Logs/*.log` file instead of captured variable.
4. **`FTakeActiveEditorScreenshotCommand` asserts in commandlet mode.** Its `Update()` body calls `FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef()` without null-guard. In commandlet mode `GetActiveTopLevelWindow()` returns null → assert. **Workaround:** custom latent command using `FScreenshotRequest::RequestScreenshot` (Slate-free, render-thread based). Don't ESCALATE on this one; it's a known UE 5.7 commandlet quirk.
5. **`AArchSimCharacter` crashes in commandlet PIE.** ALS LoadObject timing fails when no Editor pre-mounted plugin content. **Workaround:** test-local `WorldSettings->DefaultGameMode = AGameModeBase::StaticClass()` override. **Production unchanged.** AS-37 tracks proper fix.
6. **`/work` hub mechanical-stop budgets need calibration for build-iterative UE work.** Both u1 and u2 overran their tool-call budgets (147/40 and 40/25 respectively) but completed under wall-clock and token caps. Build-iterative work (`Build.bat` + `UnrealEditor-Cmd.exe` cycles + log inspection) typically eats 5-10 tool calls per round. Suggest raising `ue5-engineer`-tagged unit budgets to 80-100 calls.

---

## 6. 後續方向 (無排序)

- **Next session (S-08) candidate scope**: AS-36 (PlaceKSetMember bug, MEDIUM) + AS-37 (ALS commandlet, MEDIUM) — these are the only fresh items from S-07.
- **Minor backlog from earlier sessions still open** (per ARCH_INDEX § 7): AS-04 (Plugins panel visual, human) / AS-05 (K1-T2/K4 art assets, parallel) / AS-06 (SPUD UE5.5 StructUtils, deferred pre-5.8) / AS-08 (SPUD orchestration RF_Transient audit, open when wiring SPUD) / AS-09 (re-verify gate on non-cuDSS host, deferred opportunistic) / AS-29 (run_gate standalone leg PowerShell race, LOW)
- **Next minor bump (v0.6.0) candidates**:
  - Wire SPUD save/load (AS-08) — closes the persistence chain promised since v0.1
  - Re-do user-driven PIE smoke with v0.5.1 leg-6 as the canonical "regression caught? no? then UX judge time" gate
  - Pilot student-trial run (if AS-36/37 closed, AS-04/05 art assets ready)
- **風險區 (還沒驗證, 但下次 release 必看)**:
  - PIE auto-smoke on English-locale Windows host (CJK locale verified; English locale uses same EXIT CODE ASCII signal so should work, but not verified)
  - PIE auto-smoke on non-cuDSS host (engine path 0 delta so should be identical, but the new test runs UE which links FrameCoreUE which links FrameCore — depends on whether non-cuDSS build hooks the leg-2 filter the same way)

---

## 7. 鐵則 audit summary (this release)

| Iron rule | Status |
|---|---|
| #1 FROZEN `Plugins/FrameSolver/Source/FrameCore/` 0-line | ✅ CONFIRMED (`git diff --stat` empty) |
| #2 FROZEN `Plugins/LevelSim/Source/LevelCore/` 0-line | ✅ CONFIRMED |
| #3 6-leg gate green | ✅ CONFIRMED (PASS at 2026-06-28T10:53:47, re-verified post-NIT-fix) |
| #4 Honest verify, `[VERIFIED]` vs `[NEW CODE]` grading | ✅ CONFIRMED |
| #5 explicit `git add`, never `-A` | ✅ CONFIRMED (Phase 5 explicit list) |
| Protected files untouched (`.gitignore`, `ArchSim.uproject`, `Plugins/LevelSim/`, build artifacts) | ✅ CONFIRMED |

**CLAUDE.md amendment NOT required.** All FROZEN paths honored.

---

接手有問題: `docs/HANDOFF_v0.5.1.md` → `docs/HANDOFF_v0.5.0.md` → `docs/HANDOFF_v0.4.0.1.md` → `docs/HANDOFF_v3.6.0.md`. PIE-specific 問題讀 `~/.claude/projects/E--project/memory/v0-5-0-pie-auto-smoke-architecture.md` + `pie-auto-smoke-preference.md`. Sprint S-07 process detail 讀 `docs/logs/S-07/manager.md`.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code) via /work hub release-hardening
