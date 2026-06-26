# 交接指南 — `v0.3.1` 後接手 owner

> `v0.3.1` 在 2026-06-27 發布,tag `v0.3.1` 接在 `v0.3.0` 之後。
> **S-04 patch path close** — S-03 carryover (AS-20 + AS-24) + 7 cosmetic NITs + hook fix。
> 3 個 v0.3.1 feature commits + 1 ceremonial U-INFRA accept + 1 release-hardening commit。
> Engine FROZEN 全程 0 行。
> 主交接文鏈:`HANDOFF.md` → `HANDOFF_v0.1.md` → … → `HANDOFF_v0.3.0.md` → 本檔。

---

## 1. `v0.3.1` = 什麼

一句話:**closes 2 S-03 deferred backlog (AS-20 LogTemp→LogArchSim umbrella sweep + AS-24 FrameCoreUE NewObject GetTransientPackage outer 3-site) + bundles 7 cosmetic Phase-5 NITs accumulated from S-03 reviews + 1 cross-session hook race fix (lives outside repo)。FrameCore engine + LevelSim + 4 external plugin source 整 sprint 0 行。**

### 動到的檔(本 release vs `v0.3.0`)

| Path | LOC delta | Type | Sprint unit |
|---|---|---|---|
| `Source/ArchSim/Private/Components/ArchSimMemberData.cpp` | +1 include / 0 net code | Production | AS-20 |
| `Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp` | +1 include / 0 net code | Test | AS-20 |
| `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` | +1 / -1 (docstring) | Test helper | PHASE5-NIT-a |
| `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` | +4 / -4 | Test | PHASE5-NIT-b |
| `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` | +8 / -8 (8 sites uniform `[Input]` prefix) | Production | PHASE5-NIT-c |
| `Scripts/run_gate.ps1` | -51 / +11 (comment trim only; logic unchanged) | Config | PHASE5-NIT-d |
| `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` | +3 / -1 (NIT-f rename + NIT-g comment unify + AS-24) | Test | AS-24 + NIT-f + NIT-g |
| `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUELoadPatchTest.cpp` | +3 / -1 (AS-24 + NIT-g) | Test | AS-24 + NIT-g |
| `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUERedundancyFieldTest.cpp` | +3 / -1 (AS-24 + NIT-g) | Test | AS-24 + NIT-g |
| `docs/ARCHITECTURE_INDEX.md` | ~+50 net (§5 + §6 + §7 backlog table + Latest tag line) | Docs | NIT-e + Phase 5 + Phase 3 closeout |
| `docs/RELEASE_v0.3.1.md` | new | Docs | release |
| `docs/HANDOFF_v0.3.1.md` | new (本檔) | Docs | release |
| `docs/logs/S-04/scope_*.md` + `plan_*.md` + `manager.md` + `agent_*.md` (8 logs) | new | Sprint logs | /work driver |
| `~/.claude/hooks/work-phase-guard.ps1` | +46 LOC (+14 code + 29 comment + 3 blank) | **OUTSIDE repo** | U-INFRA-u1 |

Cumulative since `v0.3.0` (`442670c`): **11 files in-repo + 1 hook OUTSIDE repo / +900-ish in repo / +46 OUTSIDE / 0 lines under FROZEN paths**。

### Sprint S-04 backlog 處理

| ID | Status before S-04 | After S-04 |
|---|---|---|
| AS-20 (LogTemp → LogArchSim) | 🟡 LOW | ✅ closed (4b6f094) |
| AS-24 (FrameCoreUE NewObject outer) | 🟡 LOW | ✅ closed (2883d40; honest disclosure that default outer was already GetTransientPackage — fix is intent-documentation) |
| AS-25 (Hook regex broaden for S-XXa) | — | 🟡 newly open (LOW; OUTSIDE repo) |
| AS-26 (UArchSimModelRegistry ClassWithin + ArchSimPieHarness mirror) | — | 🟡 newly open (MEDIUM; HANDOFF_v0.3.0.md §4 follow-up that AS-24-u1 scope excluded) |
| AS-27 (ARCH_INDEX §8 + DriverLoopTest sub-check 1 cosmetic) | — | 🟡 newly open (LOW; pre-existing carryovers) |
| Z-01 (v0.4.0 spike — UE5.8 + Scenario MVP) | 🔵 deferred (S-03 retrospective) | 🔵 still deferred (S-04 scope tasks #6-#8 remaining; may complete this session) |
| AS-04 (Plugins panel visual) | 🟡 open (human) | 🟡 still open |
| AS-05 (K1-T2 / K4 art assets) | 🟡 open (parallel) | 🟡 still open |
| AS-06 (SPUD StructUtils deprecation) | 🔵 deferred (pre-5.8) | 🔵 still deferred (couples to Z-01) |
| AS-08 (SPUD RF_Transient audit) | 🟡 open | 🟡 still open |
| AS-09 (non-cuDSS re-verify) | 🔵 deferred (opportunistic) | 🔵 still deferred |

### 什麼未動

- FrameCore engine source (`v4.0.0` FROZEN) — 整 sprint **0 行**
- LevelSim engine (`v1` FROZEN) — 整 sprint **0 行**
- 4 個外部 plugin source (ALS / Prefabricator / SPUD / SUQS) — 整 sprint 0 行(無 read-for-precedent need 在 S-04)
- `ArchSim.uproject` / `.gitignore` / build artifacts — 鐵則 #5
- `Plugins/FrameSolver/Source/FrameCoreUE/Public/` 全部 production UCLASS surface(含 `FrameInteractiveSubsystem.h`)— **AS-24-u1 只動 test sites,production unchanged**
- `LogArchSimRegistry` per-class precedent at `ArchSimModelRegistry.cpp:13`(anti-goal)
- v0.3.0 已 shipped tests 全保留(SaveLoad / MaxRankCeiling / RebaselineCeiling / TickDriver / CharacterInput / PieHarnessSmoke / PieRebaseline / PieDriverLoop / PieInputRuntime / EmptyModelStartSession),v0.3.1 加 0 個新 test;NIT-f rename 既有 test path 而非 add/remove

---

## 2. 怎麼跑

```powershell
# Pre-req
$env:UE_ENGINE_ROOT      # 指 UE 5.7 install root
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"
# openseespy 在 system Python (pip install openseespy) — NOT 在 framecore-direct env

# UE editor incremental build
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# 一鍵 5-leg gate (cuDSS host expects 145)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS, UE 145 tests run, exit 0

# Non-cuDSS host fallback (143)
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 143

# NIT-f rename verify (single test)
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession; Quit" `
    -unattended -nullrhi -log
# Expect: 1 Result={成功} + EXIT CODE: 0 + old name "No tests matched"
```

### 使用者必做(v0.3.1 carry forward 自 v0.2.0/v0.3.0 的 BP author 設定)

⚠️ **C++ 端 only ships — UAsset 由 project author 在 UE Editor 建立**:

1. 開 UE Editor → `ArchSim.uproject`
2. 按 `docs/INPUT_MAPPING.md` 在 `Content/Input/` 建 6 個 UAsset
3. 開 BP class 繼承 `AArchSimCharacter`,assign 6 個 UAsset
4. PIE → 測 WASD / Mouse / Space / Shift / Ctrl + ALS 蹲衝刺 + v0.3.0 加的 `NotifyControllerChanged` lifecycle 在 re-possess 時應該也乾淨

### v0.3.1 額外可確認(可選)

5. PIE → 觀察 `[Input]` prefix 出現在 `Output Log` 中 Enhanced Input 相關 warning(若觸發任何 IA 為 null 的場景,新 prefix 讓 filter 更直觀)
6. 跑 `FrameCore.UE.InteractiveSubsystem.*` 子樹 isolated 應有 4 個 sub-tests(StartEndLifetime + PatchSemantics + PerfBaseline + EmptyModelStartSession 命名都 align 到子樹下)

---

## 3. 新 token / 新 flag / 新 API

### Production-side (`AArchSimCharacter`)

**沒有新 API。** NIT-c 只動 `UE_LOG` 訊息文字(加 `[Input]` 前綴);call sites 不變。

### Test path rename

舊:
- `FrameCore.UE.EmptyModelStartSession`

新(命名空間 parity):
- `FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession`

若任何外部 script 跑舊路徑(本 repo 內 0 個),需 sync。`Scripts/run_gate.ps1` comment 中已自動 sync 到新名(NIT-d trim 順帶處理)。

### 新 Test helper

無。

### Gate 變動

- `Scripts/run_gate.ps1` L29 `$ExpectedUeTests` **不變** (145 cuDSS / 143 non-cuDSS)。NIT-d 只 trim comment block 中的 intermediates;script logic 完全不動。

### Hook 變動 (OUTSIDE repo)

`~/.claude/hooks/work-phase-guard.ps1` 加 dual-layer defence (per-project state dir + foreign-state content sniff)。本 repo 跑 `/work` 不受影響;**其他 project 跑 `/work` 寫 state 不會再誤擋本 project 的 commit**(S-03 lesson #6 closeout)。

---

## 4. 仍 deferred 的 items + Sprint S-05 建議排序

### Z-01: v0.4.0 spike — UE5.8 upgrade + Scenario editor MVP (eval-gated)

**Status**:S-04 scope contract Tasks #6-#8 包含 SPIKE-UE5.8-eval + SPIKE-Scenario-u1/u2/u3 + 條件 RELEASE-v0.4.0。User picked "兩個都試 (UE5.8 → Scenario)" with "no cap" budget。本 v0.3.1 release 是 patch path close;spike portion 可能在本 session 繼續,可能 defer 到 S-05。

**First action on day 1**(若 spike 進 S-05):re-invoke `/work` in a new session;Phase 0 reads `docs/logs/S-04/scope_2026-06-26T2030.md` Tasks #6-#8 unshipped status;decide-gate before any code:UE5.8 install available?Research/ue58_attempt/ sandbox 還在嗎?Plugin compatibility (ALS/SPUD/Prefabricator/SUQS) re-verify status?

### AS-25: Hook regex broaden for `S-XXa` suffix sprints (LOW; OUTSIDE repo)

**First action**:Edit `~/.claude/hooks/work-phase-guard.ps1` content-sniff regex from `^S-\d+$` to `^S-[\w]+$` (or `^S-\d+[a-z]?$` for stricter). Verify with the 4-scenario stdin test that the U-INFRA-u1 subagent established. No ArchSim repo commit.

### AS-26: `UArchSimModelRegistry` ClassWithin verify + ArchSimPieHarness NewObject outer mirror (MEDIUM)

**First action**:Read `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` — confirm it's `UCLASS` with implicit `Within=UGameInstance` (since base `UGameInstanceSubsystem` has `Within=UGameInstance`). If yes, the same ClassWithin warning that AS-24 documented for `UFrameInteractiveSubsystem` applies. Mirror the AS-24 fix at `Source/ArchSim/Private/Tests/ArchSimPieHarness.cpp:81`:
```cpp
// AS-26: GetTransientPackage() outer for ClassWithin(UGameInstance) consistency
return NewObject<UArchSimModelRegistry>(GetTransientPackage());
```
Add the AS-24-style WHY comment. Verify single-test isolated run no longer trips NotNull.cpp ensure. 5-leg gate count unchanged at 145.

### AS-27: ARCH_INDEX §8 + DriverLoopTest sub-check 1 cosmetic carry-overs (LOW)

**First action (a)**:Edit `docs/ARCHITECTURE_INDEX.md` §8 gate cheat-sheet (L288 area) — replace `# 5-leg gate (default 140 expected; pass 138 on non-cuDSS host)` with `# 5-leg gate (default 145 expected; pass 143 on non-cuDSS host)` to match current state.

**First action (b)**:Edit `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` L54 + L58 sub-check 1 comments — replace `"always has at least one"` / `"always provides"` with empirical phrasing matching the NIT-a closeout at `ArchSimPieHarness.h:52`. One commit can bundle both items.

### Phase 5 docs-sync inline candidates (cosmetic, future S-05 docs commit)

- (None new this release — PHASE5-NITS-u1 swept the v0.3.0 carry. S-05 review will surface a fresh list.)

### Carry-over from earlier sprints (no S-05 commitment)

- **AS-04** Plugins panel visual confirmation (~30 min human). **First action:** 開 UE Editor → Edit → Plugins → filter "ALS"/"Prefabricator"/"SPUD"/"SUQS",每 plugin status="Enabled",截圖到 `docs/screenshots/gate0_plugins_panel.png`。
- **AS-05** K1-T2 / K4 art assets (parallel human-side). **First action:** 與 art owner 確認 source pipeline,匯入 `Content/`,確認 `.gitignore` 已蓋 `.uasset` binary。
- **AS-06** SPUD UE5.5 StructUtils deprecation. **First action:** 暫不動作 — couples to Z-01;UE5.8 spike 啟動再評估。
- **AS-08** SPUD `RF_Transient` audit. **First action:** `grep -rn "RF_Transient\|UPROPERTY.*Transient" Plugins/SPUD/Source/` 確認 save-game 路徑不洩漏 transient component data。
- **AS-09** non-cuDSS host re-verify. **First action:** 在無 RTX GPU host 跑 `.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 143` 確認 non-cuDSS path 143 tests 全綠。

---

## 5. 過程留下的教訓 (durable, S-04 specific)

僅本 Sprint S-04 學到的;與全域教訓無重疊:

1. **Pre-flight grep collapses or expands budget BEFORE dispatch.** AS-20 dropped from 1-1.5h to 30 min (umbrella already existed);AS-24 expanded from 1 site to 3 (sister sites surfaced). Both cases happened at Phase 2 pre-flight, BEFORE the subagent was dispatched — saving wasted ESCALATE round-trips. **Lesson:** every Phase 2 dispatch should do 5-10 min of pre-flight grep on the unit's target patterns. Higher-ROI than the same grep done inside the subagent's budget.

2. **Honest disclosure of "fix is equivalent to default" preserves both ship and truth-discipline.** AS-24-u1 subagent found that `NewObject<T>()`'s template default IS `GetTransientPackage()` — making the explicit-pass fix technically a no-op. Subagent self-graded `[VERIFIED, expected]` with framing "fix value = intent clarity + comment". Phase 3 reviewer independently verified the UE-source claim. Accepted on intent-documentation grounds, with AS-26 backlog opened for the HANDOFF carryover (ArchSimPieHarness mirror). **Lesson:** when a subagent's fix turns out to be a no-op equivalent of the default behaviour, the right move is honest disclosure + intent-documentation framing — not silent shipping or rejecting the fix. The release notes carry the disclosure transparently.

3. **Reviewer-found "scope leak" → inline fix as Phase 3 closeout, not new backlog.** PHASE5-NITS-u1 reviewer caught ARCH_INDEX §7 L270 still had the old test name despite NIT-f sync claim. Main thread inline-fixed in 1 line as Phase 3 closeout before Phase 4 commit. AS-27 backlog opened only for the truly pre-existing items the reviewer flagged. **Lesson:** reviewer-found scope leaks of the unit's own scope → fix inline. Reviewer-found genuine pre-existing items → backlog. The distinction is "is this what the unit was supposed to do?" — yes → fix; no → backlog.

4. **ESCALATE triggers can be over-conservative; user scope-confirm is fast and right.** PHASE5-NITS-u1 iteration 1 hit the spec's `>2 warn sites` ESCALATE trigger for NIT-c when subagent found 8. The 8-site fix turned out to be mechanically trivial + low-risk. User confirmed via AskUserQuestion in seconds; fresh focused dispatch (within budget) finished the work in 7m25s. **Lesson:** ESCALATE triggers should err on the side of "ask if scope expands materially" — not on the side of trying to predict every safe expansion in advance. The user's scope-confirm is a cheap round-trip.

5. **Mid-sprint feature commits + final tag ceremony is the right cadence for patch releases.** S-04 patch path = 3 mid-sprint feature commits (`4b6f094` AS-20 / `2883d40` AS-24 / `e763fa9` PHASE5-NITS-u1) + 1 release-hardening commit + v0.3.1 tag. Per-unit commit makes `git log --follow` clean on a single file; tag captures cumulative delta. **Lesson:** matches v0.3.0 / v0.2.0 / v0.1.x cadence; don't combine into one mega-commit; don't tag every mid-sprint commit.

6. **Step-cap mechanical violation is usually a planning issue, not a subagent issue.** PHASE5-NITS-u1 iteration 1 blew the 40-step budget (60 used). Phase 3 reviewer analysed: 7 NITs × ~6-8 steps each + verification + ESCALATE check = ~50 steps baseline; 40 was under-estimated at Phase 1. **Lesson:** PHASE5-NIT-style bundle units with ≥6 items should budget 60-80 steps. Don't blame the subagent for blown caps when the cap was set by planning that didn't model the actual work-per-NIT cost.

7. **Parallel-dispatch + shared 5-leg gate file is OK if no race; per-unit commits keep `git blame` clean.** S-04 Round 1 dispatched 3 units in parallel (U-INFRA outside repo + AS-20 in Source/ArchSim/ + AS-24 in Plugins/FrameSolver/FrameCoreUE/Tests/). No file overlap; each subagent ran its own 5-leg gate on the combined working tree (gates were sequential within each subagent's flow, only the gate-output log file was shared). PASS readings were valid because the diffs didn't conflict. Per-unit explicit `git add` at Phase 4 kept commits attributable. **Lesson:** parallel dispatch + sequential gates within each subagent + per-unit explicit staging works for non-overlapping units. The reviewer's Finding #1 about "AS-24 diff present in AS-20's gate run" was a documentation accuracy nit, not a quality issue.

---

## 6. 後續方向

### Sprint S-05 建議排序

backlog after v0.3.1:

1. **Z-01** v0.4.0 spike continuation (UE5.8 upgrade + Scenario editor MVP) — if not completed in current S-04 session, this is the headline for S-05.
2. **AS-26** UArchSimModelRegistry ClassWithin verify + ArchSimPieHarness mirror — MEDIUM; short unit; can bundle with other LOWs in a v0.3.x patch line.
3. **AS-25 + AS-27** LOW cleanup — short hook regex broaden + 2 cosmetic ARCH_INDEX/DriverLoopTest fixes. Bundle one patch.
4. **AS-04 + AS-05** 美術 + 視覺確認 (human-side parallel).
5. **AS-08** SPUD orchestration 當決定接 SPUD save-game 時.
6. **AS-09** non-cuDSS host re-verify 機會性.
7. **AS-06** SPUD StructUtils deprecation 僅 couples 到 Z-01.

### 何時 bump 下個 minor

- v0.3.x patch 線 適合 AS-25/AS-26/AS-27 + 任何 cosmetic NIT 收尾
- 當 Scenario MVP shippable 進 PIE end-to-end → bump v0.4.0
- UE5.8 升級 itself 不算 user-visible feature → 屬 v0.4.0 spike 的 helper(carryforward from RELEASE_v0.3.0 §6)

### 風險區

- **UE5.8 升級** — carry forward from RELEASE_v0.3.0 §6 風險區 unchanged. 若觸發 SPUD `StructUtils` 真的破壞,要決定 ALS-Refactored v4.18+ verify or fork。
- **AS-26 modifies ArchSimPieHarness.cpp** which is exercised by v0.3.0-shipped `ArchSim.Integration.Pie*` tests — if the ClassWithin fix changes test behaviour, 5-leg gate count or any sub-check may shift; verify carefully.
- **Hook fix (U-INFRA-u1) is outside repo** — if a different machine runs `/work` on this repo, it will use whatever hook is on that machine. The race fix only takes effect on machines that have the patched hook. Not a release risk but a cross-machine drift risk for `/work` users.

---

接手有問題:
- `docs/HANDOFF.md` → `docs/HANDOFF_v0.1.md` → `HANDOFF_v0.1.1.md` → … → `HANDOFF_v0.2.0.md` → `HANDOFF_v0.3.0.md` → 本檔
- Sprint S-04 完整 manager log: `docs/logs/S-04/manager.md` (append-only)
- v0.3.1 release notes: `docs/RELEASE_v0.3.1.md`
- Architecture index: `docs/ARCHITECTURE_INDEX.md`
- Sprint notes (cross-sprint summary): `docs/SPRINT_NOTES.md`
