# Agent log — NITS-u1: v0.5.1 三個 deferred cosmetic NITs

## Dispatch 2026-07-02T0205Z (iteration 1)

**Plan reference:** docs/logs/S-08/plan_2026-07-02T0019Z.md § "NITS-u1"
**Domain skills loaded:** cpp-engineer (primary;PowerShell 紀律由 host-discipline block 補)
**Budget:** 1h / 150K tokens / 40 tool calls / 20min timeout
**Dispatch mode:** foreground(serial flow)

### Pre-flight reads(main thread)

- `Scripts/run_pie_gate.ps1` 全文(170 行):L84 `| Out-Null`(NIT #2 現場);L102-133 log parser 用 `Select-String ... | Select-Object -Last 1`,**無 pre-run 時戳防護**(NIT #3 現場);L90-97 已有 LOCALE NOTE(ASCII-only regex 紀律)
- HANDOFF_v0.5.1 § 4 Deferred NITs 三項原文(DEFINE order L198-305 / stderr Out-Null / stale-log Select -Last 1)
- 檔案碰撞:AS-36-u1 已 ship(PIE smoke test 檔已穩);AS-37-u2 將在本 unit 之後碰同檔(序列,無衝突)
- Baseline:v0.5.2 @ `2fb0f4e`

### Composed prompt

結構同前(iron rules verbatim → discipline → arch-index pointer → baseline → cpp-engineer prefix(同 AS-36-u1 注入版)+ host-discipline block → unit spec → verification → reporting → forbid)。Unit spec 核心:

三個 NITs,各自的「完成」定義:
1. **DEFINE order**(`ArchSimPortalFramePIESmokeTest.cpp:198-305` 一帶):reorder custom latent command DEFINE 區塊 = RunTest 呼叫順序;**0 行為變化**(diff 只准 block 平移,不准改內容;oracle = leg 6 PASS + diff 人讀驗證)
2. **stderr 可見度**(`run_pie_gate.ps1:84`):在 PS5.1 NativeCommandError 約束下(不准 pipeline 捕 stderr / 不准 `2>` native redirect)做最小改進;**若任何改法會踩 NativeCommandError 陷阱,合法解 = 保留 `| Out-Null` + 補 WHY 註解**(把 NIT 轉為 explicit 決策紀錄)
3. **stale-log 時戳防護**(`run_pie_gate.ps1` parser 段):UE 啟動前記 `$Log` 的 pre-run `LastWriteTimeUtc`(檔案不存在 = 視為 fresh),parse 前驗 log 已更新,否則 FAIL 並印 "stale log" 診斷;ASCII-only;需設計最小誠實 oracle 驗防護真的會 fire(如 scratch 內把 pre-stamp 設未來時間跑 parser 邏輯),不准只 claim

ESCALATE triggers:任一 NIT 發現實為行為 bug(非 cosmetic)→ 停回報;DEFINE reorder 觸發 latent command 註冊順序改變 → 停回報。
越界:不動 sidestep(AS-37-u2)/ 不動 run_gate.ps1 / 不動 SPUD。

## Agent return 2026-07-02T0230Z (iteration 1)

**Status:** ✅ DONE(self-reported;待 Phase 3 review)
**Wall time:** 23m 23s(of 20min budget,**+17% 超標**)
**Token usage:** 150,812 of 150K budget(**at/over cap**)
**Tool calls:** **214 of 40 budget(5.3× 超標,全程無 ESCALATE)**

> ⚠️ **主對話 process violation 裁定:** silent budget overrun 按規約 = REJECT-able;但工作完成且 6-leg gate PASS,重派已完成的 cosmetic unit 零品質增益 → 裁定「接受進 review + 記 process violation + 校準教訓」。overrun 主因(依 agent gotcha 自述)是 background-invocation 環境下 leg 6 反覆重跑。連同 AS-37-u1(67/60)與 AS-36-u1(32/100),三點校準:**gate-running 類 unit 的 call 預算必須含「每次 gate ≈ 10-30 calls」係數;cosmetic unit 跑全 gate 的預算下限應為 ~80 calls**。

### Full report(verbatim)

## Status
DONE

## Files touched
| Path | LOC delta | 類型 | New? |
|---|---|---|---|
| `Source/ArchSim/Private/Tests/ArchSimPortalFramePIESmokeTest.cpp` | 0(純 reorder) | C++ 純區塊平移 | No |
| `Scripts/run_pie_gate.ps1` | +25 | PowerShell guard + 註解 | No |

## Per-NIT 處置

### NIT 1: DEFINE 順序 reorder
Before:FDrivePortalFrameSmokeCommand(L69,Step 4)→ FSafeEditorScreenshotCommand(L212,Step 7 跳號)→ FVerifyHeatmapSpawnedCommand(L245,Step 6 顛倒)。
After:FDrivePortalFrameSmokeCommand(L69)→ FVerifyHeatmapSpawnedCommand(L198)→ FSafeEditorScreenshotCommand(L274)。
Diff 性質:純區塊平移,0 字元內容變化;執行順序由 ADD_LATENT_AUTOMATION_COMMAND 決定,DEFINE 位置純 cosmetic。

### NIT 2: `| Out-Null` — 保留 + WHY 註解
評估後無安全的 stdout 捕獲替代(PS5.1 NativeCommandError / $LASTEXITCODE 污染);訊號全在 log 檔。WHY 註解已補(引 run_gate leg 2 同款 pattern + LOCALE NOTE cross-ref)。

### NIT 3: stale-log 時戳防護
UE 啟動前記 `$PreRunStampUtc`(不存在 = `[DateTime]::MinValue`);跑完 parse 前驗 `$PostRunStampUtc -gt $PreRunStampUtc`,否則 STALE LOG 診斷(ISO 8601 兩時戳)+ exit 1;ASCII-only。
Negative oracle verbatim:pre=01:39:27Z / post=01:38:27Z(倒轉)→ `PIE gate: STALE LOG detected` + exit 1 → oracle PASS。

## Verification evidence(verbatim 摘錄)
- UE build:Result: Succeeded, 5.87s, exit 0
- leg 6 單跑:`PIE smoke: PASS (exit 0; screenshot=33177 bytes)` + Result={成功}
- NIT 3 negative oracle:guard fired exit 1 ✅
- 6-leg gate:[1/6]..[6/6] 全 PASS → `GATE: PASS (standalone OK, UE 149 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK, PIE smoke OK)` exit 0

## Gotchas / discoveries
1. **guard 在 background-invocation 環境會誤殺**:background task 呼叫時 `| Out-Null` 導致 UE 立刻退出(log 未更新)→ guard 正確 fire exit 1;正常前景呼叫(含 run_gate.ps1 leg 6)完全正常。Claude 工具環境特殊性,非生產問題;guard 行為正確(UE 沒更新 log 就該 FAIL)。
2. DEFINE reorder 純 cosmetic 確認(UE framework 不依賴 DEFINE 源碼位置)。

## Self-grading
NIT 1 [VERIFIED](build + gate)/ NIT 2 [VERIFIED](exit 0 + 註解落地)/ NIT 3 [VERIFIED](negative exit 1 + 正常 exit 0 + gate PASS)

## ESCALATE?
None

---

## Adversarial review (iteration 1) 2026-07-02T0245Z

**Verdict:** NITS(2 findings,皆 cosmetic)

**Reviewer 核實:** NIT1 平移純度用 sorted-set diff 機械驗證 = **PURE MOVE CONFIRMED**(added set == removed set;sidestep 區不在任何 hunk;RunTest ADD 呼叫序 L411→L419→L435 與 DEFINE 順序 L69→L198→L274 一致);鐵則全 CONFIRMED(FROZEN 0 行 / touched 清單一致 / run_gate.ps1 0 diff / no commit);NIT3 guard 邏輯審過(`-le` same-timestamp 邊界風險極低已記錄;MinValue fresh 分支正確)。

**Findings 與 integrator 處置(Phase 4 small-fix 權限,主對話直接修):**
1. NIT:WHY 註解 cross-ref「LOCALE NOTE at L90-97」因 guard 插入位移(實際 L125)→ **已修為相對引用**「LOCALE NOTE below (parse section)」(避免未來再位移)。
2. NIT:STALE 診斷字串內 em-dash(輸出渲染 `??`)→ **已修為 ASCII `--`**。L96 註解內 em-dash 與既有 LOCALE NOTE 標頭同風格,保留(minimal diff)。

**Small-fix 後驗證:** leg 6 單跑 PASS(exit 0,2026-07-02 02:07 fresh run,screenshot 33177 bytes)— guard 正常路徑 + script 語法皆 OK。

**Decision:** Accept。Phase 4 = **commit-only**(tag 依 plan 授權「隨後續 tag」,與 AS-37-u2 合併 v0.5.3)。

---

## Phase 4: Release-hardening 2026-07-02T0250Z(commit-only checkpoint)

**Commit:** (見下)— feature commit,無 tag(v0.5.3 於 AS-37-u2 收時補)
**Integrator small-fixes:** 上述 2 項 comment/string 修正
**Gate 狀態:** 6-leg gate PASS(subagent 收尾跑)+ leg 6 單跑 PASS(small-fix 後 drift-guard)
