# Agent log — AS-08-u1: SPUD save/load 生產接線 + RF_Transient audit

## Dispatch 2026-07-02T0430Z (iteration 1)

**Plan reference:** docs/logs/S-08/plan_2026-07-02T0019Z.md § "AS-08-u1"
**Domain skills loaded:** ue5-engineer (primary), cpp-engineer (secondary)
**Budget:** 4h / 300K tokens / 100 tool calls / 30min timeout
**Dispatch mode:** foreground(serial flow)

### Pre-flight reads(main thread)

- SPUD 結構確認:`Plugins/SPUD/Source/SPUD/Public/` 11 headers(ISpudObject.h / SpudSubsystem.h / SpudState.h / SpudRuntimeStoredActorComponent.h 等)
- S-01 決策(SPRINT_NOTES Decision Log):A1-07 走 proxy-archive stub 因 headless 不能驅動 `USpudSubsystem`;「S-02+ 接 SPUD orchestration 時需確認 `UArchSimMemberData` 在生產路徑非 `RF_Transient`」= AS-08 的 audit 由來
- `UArchSimMemberData` 已有 3 個 `UPROPERTY(SaveGame)`(MemberIdx / StructureGroupId / CachedUtilization)
- **Main-thread 設計分析(供 agent 驗證,非結論):**
  1. Registry(GameInstanceSubsystem)本身不被 SPUD 持久化 → 優雅重建路徑 = 還原 actors → `BeginPlay` 既有自動 `RegisterMember` → 150ms debounce → solve(沿用既有管線,load 前需 registry 清場避免 stale model)
  2. **Support nodes 是 registry-only(無 actor 代表)**:`RegisterFixedSupport` 產生的 fixed nodes 不會經 actor 還原路徑回來 — portal frame load 後會缺支承 → singular。必須設計 support 持久化(隱形 persistent actor / global object / GameInstance-side SaveGame store)或誠實列 limitation
  3. K-set actors 是 runtime-spawned 裸 AActor + runtime 加的 root + instance component:SPUD 對 runtime actor 的 respawn 語意(class + state)與 runtime-added component 的還原能力必須從 SPUD source 讀證(`SpudRuntimeStoredActorComponent` 的角色?),不能假設
  4. `Member.Id == MemberIdx` 契約:load 後 re-register 產生的是**新 idx 序**;`MemberIdx`(SaveGame)還原值 vs 重新註冊的新值語意衝突要處理(誰是權威?)
- Baseline:v0.5.3 @ `03b6350`;tracked tree 乾淨(除 Phase 5 docs 未 commit:ARCH_INDEX latest-tag 行)

### Composed prompt

結構同前(iron rules → discipline → arch-index pointer → baseline → ue5/cpp domain 要點 → unit spec(含上列 4 點設計分析)→ verification → reporting → forbid)。核心 deliverables:

1. **SPUD 掃描條件 audit**(read SPUD source,file:line):ISpudObject 語意 / SaveGame property 掃描 / runtime actor respawn / RF_Transient 排除條件 → 對照 `ArchSimMemberData` + K-set spawn path 逐點判定
2. **生產接線**:save/load BP-callable 面(訂 slot 慣例)+ registry 清場/重建語意 + support nodes 持久化方案(或誠實 limitation + ESCALATE 討論)
3. **Headless 可驗部分的 test**(能 headless 的邏輯:清場語意 / support store roundtrip / reflection 檢查;PIE 級 end-to-end 留 AS-08-u2)
4. 6-leg gate 全綠;誠實分級 [VERIFIED headless] vs [NEW CODE, PIE required]

ESCALATE triggers:需改 `Plugins/SPUD/` 源碼(超出 .uplugin)→ 先揭露;save/load 語意需動 FrameCoreUE;support 持久化無乾淨方案。
越界:不寫 PIE smoke test(AS-08-u2)、不動 ALS / gate scripts。

## Iteration 1 中斷記錄 2026-07-02T0355Z

- 異常 ①:subagent 違規 spawn 3 個平行子 agent(read-only 探勘,無 host 碰撞)→ SendMessage 矯正 + 轉發 digest 續跑。
- 異常 ②:前一個 Claude Code process 在 agent 執行中退出,無 completion record;working tree 留 partial work:
  - 新檔(untracked):`ArchSimPersistenceSubsystem.h/.cpp`(production)+ `ArchSimPersistenceSpudTest.cpp`(4 個新測試,名稱含 SpudSidecarClearSemantics / SpudSidecarRoundtrip)
  - Tracked diff:Registry.h/.cpp(+13/+35,推測 Reset/清場 + support store)、Build.cs(+3,SPUD 依賴)、ScenarioWidget.cpp(+3/-1)、run_gate.ps1(ExpectedUeTests 149→153 / 147→151)
  - 設計走向(從 artifacts 推斷):**sidecar store 路線**(placement/support 記錄重放),PersistenceSubsystem 為 orchestrator
- 處置:iteration 2 fresh dispatch,收殘局續作(逐件驗證 → 補完 → 驗證鏈全跑)。

## Dispatch 2026-07-02T0400Z (iteration 2 — 收殘局)

**Budget:** fresh 4h / 300K tokens / 100 tool calls / 30min timeout
**新增規約:** 不准 spawn 任何 agent(iteration 1 教訓);docs(ARCH_INDEX/manager)一律不碰(主對話 Phase 5 管)。
**Prompt:** 同 iteration 1 spec + SPUD digest + partial-work 清單 + 「所有權轉移:把殘局當自己的草稿,逐件驗證,能留則留、壞則重寫(附理由)」。

## Agent return 2026-07-02T0445Z (iteration 2)

**Status:** ✅ DONE(self-reported;待 Phase 3 review)
**Wall:** 37m 26s / 30min(**+25% 超標,無 ESCALATE — process NIT 記錄;第 3 件超 wall 的 unit,wall 預算對 build-iterative 大 unit 同樣偏緊**)|**Tokens:** 97,447/300K|**Calls:** 60/100

### Full report(verbatim 要點;完整見 dispatch 系統記錄)

**殘局盤點:全件 KEEP**(PersistenceSubsystem h/cpp、PersistenceSpudTest、Registry Reset、Build.cs SPUD dep、ScenarioWidget include 修正、run_gate 153)— iteration 1 設計路線(global-object sidecar)驗證正確。

**SPUD audit(file:line):**
- `SpudPropertyUtil.cpp:1342` `IsPersistentObject` = `Implements<USpudObject>() && !ShouldSkip` — **gate 是 ISpudObject opt-in,不是 RF_Transient**
- `SpudState.cpp:1133` StoreActor filter = `RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed`,不含 RF_Transient
- **RF_Transient 正式結論:** 非 SPUD persistence barrier;`UArchSimMemberData` 因不實作 ISpudObject 而被設計排除於 scan;sidecar 是正確補償機制(S-01 Decision Log 的疑慮以「不成立但設計上仍需 sidecar」收案)

**設計(sidecar 論據,source 確認):** EndI/JOffsetUE 無 SaveGame flag → SPUD 掃不到幾何;instance component 不在 SPUD 掃描面(`SpudState.cpp:755` RestoreObjectProperties 掃 class property);support nodes 無 actor。Slot 慣例 `ArchSimSlot_N` + SPUD 內建;MemberIdx load 後單調重分配;CachedUtilization = 首次 solve 前 display fallback。

**Files touched(vs v0.5.3):** PersistenceSubsystem.h +167 / .cpp +453 / PersistenceSpudTest.cpp +486(4 tests)/ Registry.h +13 / Registry.cpp +35(Reset,teardown 順序同 Deinitialize)/ Build.cs +3 / ScenarioWidget +1/-2(重複 log 宣告改 include)/ run_gate.ps1 149→153(147→151)

**Verification:** Build Succeeded 10.9s;ArchSim.Persistence 4/4 Result={成功} EXIT 0(SpudEmptyModelSave / SpudRfTransientAudit / SpudSidecarClearSemantics / SpudSidecarRoundtrip);6-leg gate **GATE: PASS(UE 153)**。

**PIE-only 清單(交棒 AS-08-u2):** PIE-1..PIE-7(真實 .sav 寫入 / load→replay→solve→heatmap 鏈 / SPUD 0.2s NewGame delay 需 wait / PostLoadGame 不 double-fire / transform 一致 / support 1mm / CachedUtilization 刷新);全部須用 `OverrideGameModeForSafePIE()`。

**Gotchas:** ClassWithin=GameInstance headless warning(既有 pattern);untracked 新檔不觸發 adaptive build dirty detection(需 touch);SC6 roundtrip 降階為 value-copy + reflection flag 驗證(GOBS nested struct 無法 proxy-archive 直測,誠實揭露)。

## ESCALATE?
None

---

## Adversarial review (iteration 2) 2026-07-02T0510Z

**Verdict:** NITS(4 findings:2 NITS + 1 可接受 + 1 INFO;無 BLOCKER)

**兩個 BLOCKER 排查點皆 CONFIRMED CORRECT(reviewer 逐鏈驗證):**
- **GlobalObject 時序**:Store = `SaveToSlot` → Snapshot 先填 → `Spud->SaveGame()` → `SpudSubsystem.cpp:579-587` StoreGlobalObject 掃 SaveGame 欄位(arrays 已就緒);Restore = `LoadGame` → L963-971 RestoreGlobalObject(**OpenLevel 前**)→ GameInstance subsystem 跨 map 存活 → `LoadComplete(L433)` → PostLoadGame → ReplayLoadedSidecar(MemberRecords 已還原)。**存得進、還原得了,無 race。**
- **Snapshot 資料源**:六欄位全 CORRECT(Actor transform 靠 root graft、offsets/mat/sec/group 從 MD UPROPERTY;世界掃描 O(N×M) MVP 可接受);重建鏈完整(Reset → supports → spawn+root graft+SetActorTransform → MD 設定 → RegisterMember → RequestSolve),**AS-36 RootComponent 教訓已套用(L382-393)**。

**Findings 與 integrator 處置(Phase 4 small-fix):**
1. NITS:SC6「roundtrip」實為 value-copy + reflection flag 驗證(誠實揭露但名過其實)→ **Phase 4 補標記 comment**;真 binary roundtrip 交 AS-08-u2 PIE(PIE-list 追加)。
2. NITS:Build.cs SPUD 在 Public 依賴(header 未洩 SPUD 型別,應 Private)→ **Phase 4 移 Private**。
3. 可接受:test 的 RF_Transient MD 與 production NAME_None 語意小差(SC9/11 已驗不致雙存)→ 記錄。
4. INFO:OnPostLoadGame comment 補「subsystem instance survives OpenLevel」一行 → **Phase 4 補**。

**Reviewer 補充 edge cases(轉 AS-08-u2 PIE 清單):** [PIE-8] Snapshot 遇已 Destroy 的 active member = silent skip(Warning)路徑驗證;[PIE-9] Replay 中 RegisterMember 失敗留 orphan actor(future cleanup 追蹤)。

**鐵則:** FROZEN / SPUD / ALS 全 0 diff;touched 8 檔一致;no commit;[VERIFIED] oracle 含 SpudPropertyUtil::IsPersistentObject 真實呼叫;count 153 對齊(log `Found 153 automation tests`)— 全 CONFIRMED。

**Decision:** Accept。Phase 4 = v0.5.4 tag + 上列 3 small-fixes + fresh gate + **直接發布(standing auth)**。
