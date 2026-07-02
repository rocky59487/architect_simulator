# Agent log — AS-42-u1: PIE gate oracle 收緊(findings #6 #7)+ Round 1-2 review 流入項

## Dispatch 2026-07-02 22:25 (iteration 1)

**Plan reference:** docs/logs/S-09/plan_20260702-2000.md § "AS-42-u1"
**Domain skills loaded:** ue5-engineer(PIE/automation 向)
**Budget:** 3h+ / 250K tokens / 100 steps / 75min timeout(PIE run 慢,依 S-08 教訓上調)
**依賴:** AS-40 + AS-41 已 accept(v2 sidecar + guards 落地)

### Spec 流入項(來自本 sprint reviews)

- AS-40 review:PIE 直測 3 個 headless 測不到的 guard(empty-overwrite / partial-snapshot / orphan destroy)
- AS-41:PIE SaveLoad smoke 擴 v2 端到端斷言(library 值 / UDL / tension-only / release 存活)+ SC24 World 路徑
- PIE OpenLevel latent-chain 硬邊界不變(SpudSubsystem.cpp:977)— replay-equivalent 為 canonical 模式

## Agent return 2026-07-02 23:00 (iteration 1)

**Status(自報):** ✅ 全工作項完成;Build Succeeded(6.47s);**PIE gate exit 0**(`[PASS] ArchSim.PIE.PortalFrameSmoke` + `[PASS] ArchSim.PIE.SaveLoadSmoke` + screenshot 15497 bytes freshness)+ **leg-2 headless 165 tests 0 error exit 0**
**Tool calls:** 113 / 100(+13%,第三次超支未 ESCALATE — gate-running unit 的已知校準問題);**Tokens:** 159K / 250K;**Wall:** 32min

**修改摘要:**
- `run_pie_gate.ps1`:#6a `$ExpectedPieTests` 陣列 per-test verdict 強制;#6b `$RunStartTime` UTC + screenshot freshness;#6c parser 升級 UE5.7 `Path={ArchSim.PIE.X}` 新格式(CJK locale 修正)+ 舊格式 fallback
- `ArchSimPortalFramePIESmokeTest.cpp`:#7a heatmap AddWarning → **180-tick bounded-poll 硬斷言**(≈3s @60fps;tick-based 慢機自動變長)
- `ArchSimSaveLoadPIESmokeTest.cpp`:#7b SC5_PRE tracked-set oracle(pre-set path diff);D SC_D1/D2 v2 注入驗證(SetMemberFlags + InjectMemberUDLs → sidecar 欄位全填);E SC_E1 empty-overwrite guard 獨立 `FDriveSaveLoadEmptyGuardCommand`(Step 11.5,避免 Reset 破壞 PIE-2/5/6 鏈)+ `AddExpectedErrorPlain` 吸收 refused-save Error;GetSupportCount→GetNodeFixityCount(v2 用 NodeFixities);F teardown `__PieSmoke__` slot 清理

(review 待查:E(2) partial-snapshot / E(3) orphan 的 PIE 處置在完整報告中的結論;AddExpectedError 範圍;GetNodeFixityCount 出處;freshness UTC 邏輯;leg 6 是否跑了 2 次)

## Adversarial review (iteration 1) 2026-07-02 23:15

**Verdict:** NITS — **收案**(reviewer:42 calls / 110K)

- 10 軸逐項:freshness `[DateTime]::UtcNow` vs `LastWriteTimeUtc` 一致(無時區 bug);stale 情境推演 → exit 1 封堵;ExpectedPieTests 缺席真 FAIL + EXIT CODE belt-and-suspenders;Path={} parser 優先序正確;180-tick poll 終止條件正確(耗盡 AddError 硬失敗);SC5_PRE 用 GetPathName(GC-safe)時機正確;SC_E1 雙 assert + `Contains`+`Occurrences=1` 可接受;**E(2)/E(3) 誠實 DEFERRED**(AddInfo + 理由:3+ latent steps / 無公開 fault-injection API);SC_D 驗 snapshot 側、replay 側誠實 [NEW CODE, PIE required](OpenLevel 邊界);GetNodeFixityCount 非 AS-42 新增(production 約束 HONORED);FROZEN/never-touch HONORED
- reviewer finding #1(working tree 髒)自行解消:HEAD 乾淨,髒檔屬 AS-39/40/41 待 commit 批次 = 本 session 預期策略
- **NITs 流向 Phase 4 cosmetic sweep:** (a) `GetSupportCount()` 在 v2 恆 0 的誤導 — 加 v1-only 註解;(b) AddExpectedErrorPlain 字串可含 slot name 更緊

**Decision:** AS-42-u1 accepted。PIE gate 收緊完成(#6 #7 閉環)。
