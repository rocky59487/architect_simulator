# 交接指南 — `v0.6.0` 後接手 owner(S-08 CLOSED)

> **From:** `v0.6.0`(S-08 close,Mode A minor bump)
> **Date:** 2026-07-02
> **Prior handoffs:** [`HANDOFF_v0.5.4.md`](HANDOFF_v0.5.4.md) 系列(S-08 各 patch)
> **Release notes:** [`RELEASE_v0.6.0.md`](RELEASE_v0.6.0.md)(含 S-08 sprint 總覽表)
> **Sprint log:** [`docs/logs/S-08/`](logs/S-08/)(manager.md 有 SESSION CLOSE retrospective)

---

## Z-01 first action on day 1

**S-08 已關,state 應為 idle。** 新工作 = 開新 `/work`(Phase 0 會起 S-09)。

```powershell
cd E:/project/ArchSim
git log --oneline -3          # 期望 v0.6.0 release commit 在頂
.\Scripts\run_gate.ps1 -RequireOpenSees    # 期望 GATE: PASS(153 + leg 6 PIE ×2)
```

## 1. `v0.6.0` = 什麼

Persistence chain 完整收口(SPUD sidecar 接線 + PIE end-to-end smoke);S-08 四任務(AS-36/AS-37/NITs/AS-08)全關;sprint 內 4 tags(v0.5.2/3/4 + v0.6.0)全 published。細節見 RELEASE_v0.6.0.md。

## 2. S-09 建議 scope(依 S-08 retrospective)

1. **Human 驗證日(最高價值)**:user-driven P1..P15 + P10/P11(AS-36 fix 後首次)+ 手動 save/load 體感 — 這是「student trial ready」的 canonical gate,自動化已到誠實邊界。
2. **AS-38(LOW)**:PlaceKSetMember shipping guard + SC8/SC9 強化(~1h)。
3. **PIE 深水區 spike(可選,experimental)**:LoadFromSlot OpenLevel 全鏈的 latent-chain 跨 map 技術驗證(PIE-2 full / PIE-4)。
4. AS-04/AS-05 美術(human/parallel)→ student-trial pilot 前置。

## 3. 過程留下的教訓(durable,全 sprint 精選 — 完整版在 manager.md SESSION CLOSE)

1. **Subagent budget 校準**:凡收尾跑全 gate 的 unit,call 下限 ~80、wall 至少 40min;cosmetic 40-call 是系統性低估(214/40 事故)。
2. **背景長命令三律**:timeout 對 background 不強制;`| tail` 管線會扣輸出且疑似背壓(5 小時懸案);正解 = 檔案重導向 + 有界輪詢 + 「慢 vs 卡」用 process/log mtime 雙判。
3. **ExecCmds 引號蒸發簽名**:UE 啟動後永久 idle、log 只有 init、無任何 LogAutomationCommandLine 行 → 查 invocation 引號,別懷疑測試本身。
4. **Subagent 兩大違規模式**:偷 spawn 子 agent(×2)、停在「等監控」(×2)— dispatch prompt 要明文禁止;第三次直接主對話接管。
5. **PIE latent 鏈不能跨 OpenLevel**(`SpudSubsystem.cpp:977`)— save/load 全鏈自動化的硬邊界,replay-only 等效驗證是正解。
6. **殺程序前必查 CommandLine**(差點誤殺 user 的閒置 shell)。

## 4. 承接清單(無 in-flight)

無 BLOCKER、無 parked escalation、無未 publish tag。Backlog:AS-38(LOW)/ AS-29(LOW)/ AS-04/05(human)/ AS-06/09(deferred)。

---

接手有問題:`RELEASE_v0.6.0.md` → `docs/logs/S-08/manager.md` SESSION CLOSE → 各 `agent_*.md`。
