# Agent log — AS-08-u2: SPUD save/load PIE smoke + gate 接線(S-08 最終 unit)

## Dispatch 2026-07-02T1015Z (iteration 1)

**Plan reference:** docs/logs/S-08/plan_2026-07-02T0019Z.md § "AS-08-u2"
**Domain skills loaded:** ue5-engineer (primary)
**Budget:** 4h / 300K tokens / 100 tool calls / 40min wall(依三次 wall 超標教訓上調;80% = 32min ESCALATE 線)
**Dispatch mode:** foreground(serial flow)
**硬規約:** 不准 spawn agent;docs 不碰;gate 跑法 = 檔案重導向 + 自行有界輪詢(背景管線病灶教訓)

### Pre-flight(main thread)

- 交棒清單:AS-08-u1 的 PIE-1..PIE-9(RELEASE_v0.5.4 § Known issues 全文)
- 關鍵現場:`run_pie_gate.ps1` 的 `-ExecCmds` **硬碼單一測試名** `ArchSim.PIE.PortalFrameSmoke` → 新 PIE test 需改為 category 跑法(`ArchSim.PIE`),parser 的 authoritative 訊號是 ASCII `EXIT CODE: N`(N=失敗數),多測試相容;screenshot 檢查對 PortalFrame 檔名 pattern,新測試不必截圖
- AS-37 規約:新 PIE test 必用 `ArchSimPieHarness::OverrideGameModeForSafePIE()`
- SPUD PIE 時序:`Initialize` 後 0.2s `NewGame(false)`(AS-08-u1 audit)→ save/load 前要 wait
- Baseline:v0.5.4 @ `679032a`(已 published);ExpectedUeTests 153 不動(PIE COMPLEX 不入 leg 2 count)

## Iteration 1 中斷 ×2 + 主對話接管 2026-07-02T1115Z

- 中斷 ①:agent 停在「等監控」(同 AS-08-u1 iter1 病)→ SendMessage 矯正續跑。
- 中斷 ②:續跑後 agent 單跑新測試時 **ExecCmds 引號蒸發**(UE 收殘缺指令 → 不跑 automation → 永不 Quit → 25 分 EOSSDK idle),agent 誤判完成又疊全 gate → 主對話殺 UE(7208)+ gate(40544)+ TaskStop agent,**接管收尾**(第三次不續派,依先前承諾)。
- **Agent 的產出本身是好的**:測試檔(695 行,13-step LatentCommand 鏈,31 斷言)+ run_pie_gate.ps1 category 跑法(+57/-5,含 result-log selection)+ run_gate.ps1 顯示標頭(+3/-1,授權內)。主對話用已知良好引號路徑重驗 → 全過。

## Takeover 完成記錄(main thread)

**PIE-1..PIE-9 覆蓋表(依測試檔自身誠實標記 + live 驗證):**
| 項 | 狀態 | 證據 |
|---|---|---|
| PIE-1 `.sav` 實檔 | **[VERIFIED]** | Step 8 assert exists + size>0 |
| PIE-2 load→replay→solve 鏈 | **[PARTIAL]** | replay 路徑直驅 `ReplayLoadedSidecar` 驗證(避開 LoadFromSlot 的 OpenLevel 斷 latent 鏈 — 即 dispatch 預告的 ESCALATE trigger,agent 選了 replay-only 策略);OpenLevel 全鏈留人工/未來 |
| PIE-3 SPUD 0.2s NewGame 時序 | **[VERIFIED]** | Step 3 0.5s wait 前置 |
| PIE-4 PostLoadGame double-fire | **[DEFERRED]** | replay-only 模式下不可觸發(理由記於測試) |
| PIE-5 transform 1mm | **[VERIFIED]** | Step 11 |
| PIE-6 support 1mm | **[VERIFIED]** | Step 11 |
| PIE-7 CachedUtilization 刷新 | **[PARTIAL]**(review #1 更正) | Step 11 soft check:失敗走 AddWarning 非硬斷言(0.5s wait 無法硬保證 debounce solve 落地,硬斷言會 flake);觀測到非零時 runtime 印 [VERIFIED] |
| PIE-8 Destroy'd member snapshot | **[DEFERRED]** | 理由記於測試 |
| PIE-9 orphan 觀察 | **[DEFERRED]** | replay 路徑記錄 |

**Verification(main thread 親跑):**
- Leg 6 wrapper 雙測試:exit 0;`SaveLoadSmoke Result={成功}`(10:45 + 11:10 兩輪)
- **完整 6-leg gate:GATE: PASS**(standalone / UE **153** / OpenSees / audit 104 / CLI / PIE ×2;exit 0;`Saved/gate_v060_takeover.log`)
- Save artifact 衛生:`Saved/SaveGames/` 無 smoke slot 殘留(Step 12 刪檔生效)
- run_gate.ps1 diff = 顯示標頭 only;ExpectedUeTests 153 未動;leg 2 PIE 排除未動

**Gotchas(takeover 期新增):**
1. **ExecCmds 引號蒸發**是「UE 啟動後永久 idle、log 只有 init」的簽名 — 診斷法:log 無任何 `LogAutomationCommandLine` 執行行。
2. Leg 5 `cli_roundtrip.py` 在重載 host 可慢至 ~8 分(非卡);「慢 vs 卡」以 process 存活 + 完成事件判,勿早殺(本次它自己跑完)。5 小時懸案的唯一 confirmed 卡死仍是「背景+tail 管線」那次。

## ESCALATE?
None(takeover 完成)

---

## Adversarial review (iteration 1) 2026-07-02T1145Z

**Verdict:** NITS(4 findings:3 NIT + 1 INFO;無 BLOCKER)

**核實:** PIE 覆蓋標記逐項對測試碼(PIE-1/5/6 斷言真實含數值 oracle;PIE-2 PARTIAL 理由成立且 `SpudSubsystem.cpp:977` OpenLevel 引用可信;PIE-3 wait+後續 IsIdle 斷言接受;4/8/9 DEFERRED 理由成立);run_pie_gate 行級審(log-selection 不會 false-PASS、stale-guard 未破、ASCII 紀律、EXIT CODE authoritative)全 OK;production source **0 diff**(PersistenceSubsystem/Registry/Widget/ALS/SPUD/FrameCore);no commit;run_gate.ps1 = 顯示標頭 only。

**Findings 與 integrator 處置(全數已修):**
1. NIT:PIE-7 header 標 [VERIFIED] 但實作是 AddWarning soft check → **header 改 [PARTIAL] + WHY(flake 取捨)**;takeover 覆蓋表同步更正。
2. NIT:cleanup 在 pre-stamp 之前的 MinValue 微妙性 → **補 NOTE comment**(實質無洞:post-run Test-Path 覆蓋)。
3. NIT:screenshot 由 PortalFrameSmoke 獨供 → **補 MAINTENANCE NOTE**(防未來移除時 false-FAIL)。
4. INFO:斷言數宣稱 31 → 實計 **30**(此處更正)。

**Small-fix 後驗證:** rebuild + leg 6 wrapper 單跑(見下)。

**Decision:** Accept。進 Phase 6 close(v0.6.0 Mode A)。
