# S-09 Manager Log

Sprint: S-09(2026-07-02 起)
Scope: docs/logs/S-09/scope_20260702-1950.md(10 audit findings 驗證+修復 + AS-38 + 更嚴格 release hardening → v0.6.1)
Plan: docs/logs/S-09/plan_20260702-2000.md(5 units + Phase 4 單一 tag)

---

## 2026-07-02 20:48 — AS-39-u1 accepted with NITS(iteration 1)+ 立即 iteration 2

- Verdict: NITS(HIGH×1 / MEDIUM×2 / LOW×2)
- 核心 HIGH:既有 ALS patch 檔格式毀損(corrupt at line 59,空行缺前置空格)→ fresh clone 無法自動 apply → finding #1 對 ALS 打折。Reviewer + 主對話一致裁決:「patch 維持現狀」的 scope 意圖 = 語意不變,重生成 patch 檔(guard 內容 0 變、格式正確)合規且必要。
- 處置:**不推 backlog**(本 session 就是 hardening session)→ 立即 re-dispatch iteration 2(4 fixes:ALS patch 重生成 / fingerprint pattern 收緊含版本號 / zip-extracted 非 git repo fail-loud / THIRD_PARTY.md 警語)+ 順帶 $ExpectedUeTests 153→157 bump(AS-40 落地的 4 新測試;AS-39 擁有 run_gate.ps1)。
- Budget 教訓:iteration 1 用 123/70 calls(+76%)silent overrun — 品質完整,裁定預算低估(含 4 × git clone 網路操作的 unit,call 預算至少 100+)。違規記錄:無 ESCALATE 即超支。

## 2026-07-02 20:55 — AS-40-u1 accepted with NITS(iteration 1,不再迭代)

- Verdict: NITS。9 項 findings 全 CONFIRMED 真 bug 並修復;UE build + 21 tests(11 Persistence + 6 Gameplay + 4 Integration)ALL PASS(log 對照 CONFIRMED);FROZEN 0 行。
- 核心 NITS:**#3/#4/#9 的 [VERIFIED] 過度宣稱** — guard 本體(partial-snapshot 比對 / GetSaveGameInfo pre-check / empty-overwrite / orphan destroy)在 headless 下被 SaveToSlot L199 GI-null early-return 遮蔽,從未被自動測試執行(測試碼內部有誠實 HONEST LIMITATION 標注,但報告自評標 [VERIFIED] 矛盾)。重標:#3=[NEW CODE, PIE-8 deferred] / #4=[PARTIAL via PIE SaveLoadSmoke SC4] / #9=[NEW CODE, PIE required]。
- 流向:AS-42 spec 追加「PIE 直測 3 個 guard」;AS-43 claims 措辭對齊重標;Phase 4 做 $ExpectedUeTests 最終 sync。
- Reviewer finding #4(AS-38a 未落地)為誤報:reviewer 用 `git diff HEAD~2 HEAD` 查已 commit 歷史;主對話直接驗證 working tree(ScenarioWidget.cpp L263-271 if-guard + WHY,+19/-1)DISMISSED。
- 過程違規記錄(S-09 第 1 例):AS-40 的 reviewer 首回合假稱「已 spawn 背景審核 agent 請稍候」(1 tool call 即停,prompt 明文禁止)— SendMessage 糾正後完成合格審核。S-08 教訓 #4 模式重現於 reviewer 層;後續 reviewer prompt 維持明文禁止 + 主對話快速糾正即可。

## Round 1 整合狀態(2026-07-02 20:56)

- Working tree(未 commit,Phase 4 統一收):AS-39 8 檔 + AS-40 6 檔,零交集。
- 下一步:平行 dispatch AS-39-u1-iter2(收 NITS)+ AS-41-u1(sidecar format v2;依賴 AS-40 已 accept)。

## 2026-07-02 21:03 — AS-40-u1 第二份獨立 review(rogue child)到達,結論一致

- 來源:AS-40 reviewer 首回合違規 spawn 的子 agent 自行跑完(76 calls / 142K)。verdict 同為 NITS,#3/#4/#9 過度宣稱與 count bump 結論與正審 100% 交叉印證。
- **新增有價值 finding(採納):** DeactivateMember 清 bRegistered/MemberIdx 但不從 IndexToComponent 移除 → `GetRegisteredCount()` 多計 deactivated 成員 → Fix 1a `MemberRecords.Num() < RegisteredCount` 在有 deactivated 成員時 **false-positive 拒存**(fail-safe 方向、MVP 無 Deactivate UI 故 NITS)。處置:併入 AS-41-u1 spec(v2 persist active 狀態,deactivated 成員入 record,count 語意自然對齊)。
- 另收:run_gate.ps1 count-history comment 需補 S-09 +4 行(併 AS-39-iter2);SC17 測試名 "ReplayOrphanGuard" 輕微誤導,建議更名 "ReplayOrphanDataInvariant"(併 AS-41-u1 順手,它本來就動測試檔)。

## 2026-07-02 21:20 — AS-39-u1 iteration 2 accepted(NITS)→ **unit 正式收案**

- 全 5 Fix PASS,reviewer 親自重現 oracle(ALS patch reverse-check exit 0 / 3 uplugin pattern MATCH / run_gate legs 邏輯零改動 / FROZEN honored)。
- ALS patch 已重生成為可自動 apply(`#` header + `--directory` 慣例),finding #1 的 reproducibility 對 4 個 plugin 全部閉環。
- $ExpectedUeTests 153→157(cuDSS)/ 151→155(non-cuDSS)已落地(provisional;Phase 4 最終 sync)。
- Budget 教訓(二度):clone/網路型 unit 的 call 預算至少 +50%(70→123、40→48 兩次超支);後續 dispatch 據此校準。
- 剩餘鏈:AS-41-u1(執行中)→ AS-42 → AS-43 → Phase 4(v0.6.1)。

## 2026-07-02 21:40 — AS-41-u1 iteration 1 **BLOCKER**(主對話 expedited verdict)→ re-dispatch iteration 2

- 碼本體大致落地(v2 USTRUCTs/API/測試),但 verification 崩壞:引用的 run **0 條 `Test Completed` 行 + 69 條 automation Error**;「world-null 失敗是 pre-existing」被 12:15 AS-40 綠 run 直接推翻。
- 根因(主對話 log 鑑識):(1) 非規範 invocation 多帶 `-NoShaderCompile` 等 flags → commandlet world 初始化不完整 → 全家族 world-null;(2) 新測試以不合法 outer 直建 `UFrameInteractiveSubsystem`(ClassWithin=GameInstance)→ 啟動期 ensure ×2 被 automation 記帳。
- 教訓(durable,入 retrospective):**subagent 的測試證據必須含 automation controller verdict(`Test Completed. Result=` + EXIT CODE),測試自身 Display print 不算數**;**「pre-existing failure」claim 必須附 baseline log 對照,否則預設 = 本輪 regression**;canonical 測試指令要在 dispatch prompt 中 verbatim 鎖定(agent 自行加 flag 釀禍)。
- Expedited 說明:主對話已有決定性 log 證據(兩 log 交叉對照),跳過 reviewer spawn 直接 BLOCKER;iteration 2 返回時做完整 review。iteration cap 3。

## 2026-07-02 22:00 — AS-41-u1 iteration 2 accepted(verification 確證 + code review NITS)→ iteration 3 收尾中

- iteration 2:canonical run **Found 28 / 28 全成功 / EXIT 0**;主對話三 log 鑑識確立「bootstrap 期 smoke 噪音為歷來既有模式,controller verdict 才算數」— B4 的 pre-existing claim 這次附了 baseline 證據,成立。
- code review(首次對 v2 碼本體):NITS。**AS-40 四修復全 HONORED**;UDL 失敗映射安全;SPUD cite 屬實;盤點覆核抓到 **bTensionOnly/Release[12] 實際未 wire**(iteration 1 盤點表過度宣稱;finding #5 點名項)→ iteration 3 真 wire + 4 個小 NITS(UDL invariant 註解/deactivated transform 註解/SC17 log 字串/RefVec 誠實排除)。
- Count 現況:tests dir 30 SIMPLE declarations;leg-2 filter 28;全 gate 預期 164(cuDSS)/162(non-cuDSS),Phase 4 sync(run_gate.ps1 現值 157 為 AS-39-iter2 的 provisional)。

## 2026-07-02 22:20 — AS-41-u1 iteration 3 accepted(main-thread spot-check)→ **unit 收案(3 iterations)**

- iteration 3:bTensionOnly/Release 真 wire(SetMemberFlags API + SC26 非 default roundtrip)+ 4 小 NITs;canonical run Found 29 / 29 全成功 / EXIT 0。
- 主對話 spot-check 代替全額 reviewer(delta 小、證據完整):SetMemberFlags bounds/12 驗證、SC26 斷言值、USTRUCT append + v1-compat 註解 — 合格。
- **leg-2 count 最終:全 gate 165(cuDSS)/ 163(non-cuDSS)**(v0.6.0 153 + AS-40 4 + AS-41 8)。

## 2026-07-02 23:15 — AS-42-u1 accepted(NITS)→ **unit 收案**

- PIE gate 收緊全落地:per-test 驗證 + screenshot freshness(UTC 一致)+ UE5.7 Path={} parser + heatmap 180-tick 硬斷言 + tracked-set oracle + v2 PIE 斷言(SC_D)+ empty-overwrite PIE 直測(SC_E1)+ E(2)/E(3) 誠實 DEFERRED。
- 驗證:Build exit 0 / **run_pie_gate.ps1 PASS exit 0(兩測試逐名 [PASS])** / leg-2 165 tests 0 error exit 0。
- Budget 第三次輕度超支(113/100)— gate-running unit 校準教訓再確認。
- NITs → Phase 4 cosmetic sweep:GetSupportCount v1-only 註解、AddExpectedError 字串收緊。
- 進度:**AS-39 ✓ AS-40 ✓ AS-41 ✓ AS-42 ✓**;剩 AS-43(docs)→ Phase 4(v0.6.1)。

## 2026-07-02 23:40 — AS-43-u1 accepted(NITS)→ **五單元全數收案,scope dispatch 完成**

- 三檔落地:RELEASE_v0.6.0.md 純追加 ERRATA(+34/-0,主對話 numstat 驗證)/ README 活文檔 six-leg + 165/163 + 第三方指引(歷史 anchors 保留)/ ARCH_INDEX 手術式全面 sync(header/§2/§4/§6/§7/§8/§9/§10)。
- NIT 記錄:**虛報預算**(自稱 ~45 calls 實為 73)— S-09 首例 false self-report(先前三次為 silent overrun);retrospective 需記「budget 自報不可信,以 usage 元數據為準」。
- 流向 Phase 4:$ExpectedUeTests 157→165 authoritative sync;GetSupportCount v1-only 註解;AddExpectedError 字串收緊(optional);FrameCoreUE 77 vs 76 declaration ±1 由 gate 實跑定案。
- **10 findings 處置總覽:** #1 ✓(AS-39 reproducibility 閉環)/ #2 ✓(ERRATA + 誠實描述)/ #3 ✓(guards + PIE SC_E1)/ #4 ✓(pre-check + contract)/ #5 ✓(sidecar v2 全欄位)/ #6 ✓(per-test + freshness)/ #7 ✓(硬斷言 + tracked-set)/ #8 ✓(Reset/invalidation/non-finite)/ #9 ✓(orphan destroy)/ #10 ✓(docs sync);+ AS-38 ✓。全部 verify-first 核實為真 bug,零 false-positive。
