# Sprint S-08 — Manager log

> Scope contract: `scope_2026-07-02T0019Z.md` | Plan: `plan_2026-07-02T0019Z.md`
> Target: v0.5.1 → v0.6.0(AS-36 + AS-37 + NITs + AS-08)
> Append-only。

---

## 2026-07-02T0100Z — AS-36-u1 accepted with NITS

- **Verdict:** NITS(reviewer 6 findings:1 HIGH doc-precision、2 MEDIUM、3 LOW;無 BLOCKER)
- **Nits opened as:** AS-38(LOW:PlaceKSetMember Root shipping-safe null guard + SC8 comment 強保證說明 + SC9 走 production path 強化)
- **Reason for accept:** 全 deliverable 有 log/檔案 oracle 佐證;nits 不擋 ship,可下個 unit/session 收
- **Finding #3 誤歸因裁定:** `docs/ARCHITECTURE_INDEX.md` + `docs/logs/S-07/manager.md` 兩髒檔為 S-07 close 遺留(內容 = v0.5.1 release 記錄,寫於 commit `4567c40` 之後),非 AS-36-u1 subagent 所改;轉 Phase 4 隨本輪 release 收 commit
- **Budget 實耗:** 160,627/250K tokens;32/100 calls;15m23s/30min — 全在 budget 內(HANDOFF v0.5.1 教訓 #6 的 100-call 校準本輪未觸頂)
- **關鍵成果:** root cause = 裸 AActor 無 RootComponent → SpawnActor Location 被吞(S-01 v0.1.1 同雷在 production 重演);fix = USceneComponent root graft;SC8/SC9 headless 回歸;commandlet PIE SC2b 相異 node pair + HeatmapActor spawn 證實;6-leg gate PASS(149 UE tests)
- **誠實揭露 carry:** user-driven PIE 一直是壞的(production bug 非 commandlet-only);S-06 P10/P11 宣稱需人工 re-verify → 留待 human backlog

---

## 2026-07-02T0125Z — AS-36-u1 shipped as v0.5.2

- **Commit:** `2fb0f4e`
- **Tag:** v0.5.2(local annotated;publish pending user run)
- **Files committed:** 10(widget fix + fixture test + S-07 遺留 docs ×2(揭露)+ S-08 logs ×4 + RELEASE/HANDOFF_v0.5.2)
- **Phase 3 verdict:** NITS(6 findings;finding #3「未申報檔案」經主對話查證為 S-07 遺留,誤歸因裁定)
- **NITS opened as new backlog:** AS-38(LOW)
- **Release notes:** docs/RELEASE_v0.5.2.md;**Handoff:** docs/HANDOFF_v0.5.2.md
- **Publish status:** awaiting user

### Adversarial review summary

Reviewer 逐 claim 對 log 檔與 git diff:149 tests / SC8-SC9 / SC2b / SC4 / screenshot 33177 bytes 全數 oracle CONFIRMED;FROZEN 0 行 + 越界禁令(NITS-u1/AS-08/AS-37 scope)遵守 CONFIRMED;6 findings 全 NITS 級。

### Notable decisions this cycle

- Release-hardening per-unit 模式:七 agent 審計由 /work Phase 3 取代;post-gate delta docs-only 免重跑 gate(揭露於 commit message)。
- Sanitize sweep 抓到 agent log 內 2 行 username 洩漏(`C:\Users\<user>\...` → `~/.claude/...`),commit 前修正。
- Phase 5 發現 SKILL_CONFIG `PROJECT_CLAUDE_MD=E:\project\ArchSim\CLAUDE.md` 指向不存在檔案(config drift);依專案實際慣例改 sync memory 檔(`frame-engine-next-plan.md` 補 game-body 錨點段,修正「v0.x 狀態只在 MEMORY.md index 行」的 drift)。

### State at end of cycle

- Remaining tasks in scope: AS-37(u1 查證 → user (a)/(b) gate → u2 條件)、NITS-u1、AS-08(u1 + u2)
- Next dispatch target: **AS-37-u1**
- Sprint logs updated: ✅ this entry + ✅ agent_AS-36-u1.md(含 Phase 4 section)

---

## 2026-07-02T0155Z — AS-37-u1 accepted with NITS(查證 unit,無 code 產出)

- **Verdict:** NITS(4 精確度 findings:PluginManager 行號 2057→2007 / leg6 時戳 UTC / 第二 crash 為 WRITE violation / severity 補 Development `-game` caveat;更正直接記於 agent log,無新 backlog)
- **查證結論:** crash 鏈 = commandlet AssetRegistry 未及索引 `/ALS/` content → `LoadObject` 四連敗(CDO + spawn 各一輪)→ `AlsCharacterMovementComponent.cpp:894/903` ensures → `AlsCharacter.cpp:526` null deref。**Severity: commandlet-only**(cooked/pak packaged 不受影響;Dev `-game` 無 pak 留 caveat)。文檔原記載「crash 在 NotifyLocomotionModeChanged」修正為「終崩點,首發 L894/L903」。
- **Budget:** 150K/200K tokens;9m49s/25min;**67/60 calls(+12% 超標無 ESCALATE — process NIT,產出品質未受影響;連續第二個 unit 顯示 call 預算偏緊,傾向 investigation 類 unit 預算 →80)**
- **Next:** user decision gate (a)/(b)
- **User decision(2026-07-02T0200Z):選 (a)+(b-1)** — 文件化 commandlet-only limitation + test-harness 統一 sidestep helper(AS-08-u2 直接受惠)。AS-37-u2 觸發為縮小版(~1.5-2h:docs + helper + PortalFrame test 改用 helper);(b-2)/(b-3)/(b-4) 不採。Dispatch 順序維持 plan:NITS-u1 → AS-37-u2 → AS-08-u1 → AS-08-u2

---

## 2026-07-02T0250Z — NITS-u1 accepted with NITS,commit-only(tag 併入 v0.5.3)

- **Verdict:** NITS(2 cosmetic:LOCALE NOTE cross-ref 位移 / STALE 字串 em-dash — integrator Phase 4 small-fix 直接修,無新 backlog)
- **交付:** NIT1 DEFINE reorder(sorted-set diff = PURE MOVE CONFIRMED)/ NIT2 Out-Null 保留 + WHY 決策紀錄 / NIT3 stale-log 時戳防護(negative oracle fired exit 1;正常路徑 exit 0)
- **Gate:** 6-leg PASS(subagent)+ leg 6 單跑 PASS(small-fix 後 drift-guard,02:07 fresh)
- **⚠️ Process violation 記錄:** subagent tool calls **214/40(5.3×)**、wall 23.4/20min、tokens at cap,全程無 ESCALATE。裁定:工作品質達標(reviewer 機械驗證通過)→ 接受;違規記錄於此。**校準教訓(三點資料):32/100(AS-36)/ 67/60(AS-37-u1)/ 214/40(NITS-u1)— 凡 unit 收尾要跑全 gate,call 預算下限 ~80;「每次 gate ≈ 10-30 calls」係數必須計入;Phase 1 planning 的 cosmetic 40-call 預算是系統性低估**
- **Tag 裁定:** 依 plan「隨後續 tag」授權,NITS-u1 commit-only;v0.5.3 於 AS-37-u2 收時一併 tag(兩 unit 同屬 PIE-infra 小項)
- **Next:** AS-37-u2 dispatch

---

## 2026-07-02T0400Z — AS-37-u2 accepted with NITS → shipped as v0.5.3(與 NITS-u1 合併 tag)

- **Verdict:** NITS(3 findings + 2 missed edges,無 BLOCKER;行為近恆等 — 唯 TestNotNull structured record 消失)
- **Integrator small-fixes(Phase 4,全數 oracle-backed):**
  1. helper 補 `Test->TestNotNull(...)` 恢復 inline 完整 parity + 修 misleading comment(AS-37-u2 review #1/#2)
  2. ARCH_INDEX L252 AS-35 row sidestep 描述 → helper 指標(review missed edge)
  3. **run_pie_gate.ps1 stale guard 加 length 條件**(same-timestamp 偽陽性當天應驗 NITS-u1 reviewer 預言;stale = time 未進 AND length 未變);scratch oracle 4/4 分支 PASS
- **Gate:** 6-leg GATE: PASS(2026-07-02 03:02 UTC fresh,含修訂版 guard live 驗證)
- **Budget:** AS-37-u2 subagent 37/90 calls、22.3/25 min、108K/200K — **校準後首個全額度內 unit**
- **AS-37 正式 closed:** (a)+(b-1) 落地;commandlet-only + Dev `-game` caveat 記錄;未來 PIE test 規約(必用 `OverrideGameModeForSafePIE`)入 ARCH_INDEX
- **Tag:** v0.5.3(NITS-u1 `a016486` + AS-37-u2 + integrator fixes)
- **Publish status:** awaiting user
- **Next:** AS-08-u1(SPUD save/load 生產接線)

---

## 2026-07-02T0355Z — PUBLISHED v0.5.2 + v0.5.3;user standing publish authorization;AS-08-u1 session-death 中斷

- **User 授權(durable,已入 memory `publish-authorization`):**「紀錄之後可以直接發布不用給我發布」→ /work Phase 4/6 自此直接 push + `gh release create`;force-push / tag overwrite / release delete 仍需逐次明示授權。
- **Published:** main `4567c40..03b6350` + tags v0.5.2 / v0.5.3 →
  https://github.com/rocky59487/architect_simulator/releases/tag/v0.5.2
  https://github.com/rocky59487/architect_simulator/releases/tag/v0.5.3
- **AS-08-u1 中斷記錄:** 前一個 Claude Code process 在 agent 執行中退出,無 completion record。中斷前異常:subagent 違規 spawn 3 個平行子 agent(已矯正續跑 + 轉發 digest);死亡時 working tree 留有 partial work(Registry.h/.cpp / Build.cs / run_gate.ps1 / ScenarioWidget.cpp / 新 untracked 檔待盤點)。處置:盤點後 iteration 2 重派(fresh agent 收殘局續作)。

---

## 2026-07-02T0930Z — 背景執行病灶處置 + watchdog 規則(durable)

- **事故:** v0.5.4 tag 前的 6-leg gate 以 `run_in_background` 執行,**hang 在 leg 2 之後 5 小時**(leg 2 UE 12:23 正常 EXIT 0;gate script 卡在 leg 3 前後;輸出檔全空因 tail 管線緩衝;`timeout` 參數對 background task 不強制)。與 NITS-u1 記錄的「background-invocation 下 UE/管線行為異常」同族。
- **處置:** TaskStop 殺 wrapper;殘留 powershell PID 以 CommandLine 驗明 = user 自己的閒置 shell,**不殺**(教訓:殺孤兒前必查 CommandLine);重跑再度被 harness 自動轉背景 → 加 **Monitor watchdog**(盯 `GATE: PASS|FAIL` verdict 行,12 分鐘 timeout 喚醒介入)。
- **Durable 規則(本 session 起):** ① 關鍵路徑長命令(gate/build)若落背景,必配 verdict-line watchdog,不裸等 notification;② background task 的 timeout 參數不可信,外部 watchdog 才是硬保障;③ 卡死判定三訊號:輸出檔 size + `ArchSim.log` mtime + process list;④ 殺程序前先 `Get-CimInstance Win32_Process` 查 CommandLine 防誤殺。

---

## 2026-07-02T0958Z — AS-08-u1 accepted with NITS(iteration 2)→ shipped as v0.5.4

- **Verdict:** NITS(4 findings:SC6 名過其實 value-copy(補誠實標記)/ Build.cs SPUD Public→Private / test RF_Transient 語意小差(可接受)/ comment INFO 補行 — 全數 integrator Phase 4 small-fix 收)
- **兩個 BLOCKER 排查點 CONFIRMED CORRECT:** GlobalObject store/restore 時序鏈(SpudSubsystem.cpp:579-587 / 963-971;restore 在 OpenLevel 前,GameInstance subsystem 跨 map 存活)+ Snapshot 六欄位資料源與重建鏈完整(AS-36 root-graft 教訓已套用)
- **RF_Transient audit 收案:** SPUD gate = ISpudObject opt-in(SpudPropertyUtil.cpp:1342),非 RF_Transient(SpudState.cpp:1133 filter 不含);S-01 疑慮不成立但 sidecar 仍必要(EndI/J offsets 非 SaveGame + instance component 不在掃描面 + supports 無 actor)
- **Budget:** iter2 60/100 calls、97K/300K、37.4/30min(wall +25% — 第 3 件超 wall,大 unit wall 預算同樣偏緊,教訓入 retrospective)
- **Gate:** 6-leg GATE: PASS(UE **153**;17:27-17:57 冷快取慢跑,兩次「卡死」判定中第二次實為慢 — watchdog 誤報教訓:verdict-file 輪詢要配 process/log 進度訊號雙判)
- **Tag:** v0.5.4;**publish:** 直接發布(standing auth)
- **Next:** AS-08-u2(SPUD PIE smoke,最後一 unit)→ Phase 6 close v0.6.0
