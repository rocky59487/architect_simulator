# 交接指南 — `v0.5.2` 後接手 owner

> **From:** `v0.5.2`(S-08 unit AS-36-u1;patch on v0.5.1)
> **Date:** 2026-07-02
> **Prior handoffs:** [`HANDOFF_v0.5.1.md`](HANDOFF_v0.5.1.md)(S-07)/ [`HANDOFF_v0.5.0.md`](HANDOFF_v0.5.0.md)(S-06)
> **Release notes:** [`RELEASE_v0.5.2.md`](RELEASE_v0.5.2.md)
> **Sprint log:** [`docs/logs/S-08/`](logs/S-08/)(sprint 進行中 — scope 尚有 AS-37 / NITs / AS-08 未收)

---

## Z-01 first action on day 1

**注意:S-08 sprint 在 v0.5.2 時仍開著。** 若接手時 `~/.claude/state/work-phase.txt` 非 idle,依 /work Compaction Recovery Protocol 從 state 續跑,**不要**重開 scope。

```powershell
cd E:/project/ArchSim
git log --oneline -3          # 期望 v0.5.2 release commit 在頂
.\Scripts\run_gate.ps1 -RequireOpenSees   # 期望 GATE: PASS(149 UE tests + leg 6 PIE)
```

若 gate FAIL → 先查 leg 6(AS-36 fix 剛動過 fixture test 與 widget);fallback `Scripts\run_pie_gate.ps1` 單跑看 SC2b。

## 1. `v0.5.2` = 什麼

- 一句話:portal frame 端點退化 bug(裸 AActor 無 root → 位置被吞 → 雙柱同 node pair → singular)修正;commandlet PIE 首次真正看到 HeatmapActor spawn。
- Source delta:2 檔(widget +41 / fixture test +250);engine FROZEN 0 行;版本 pin 0 動。
- 一併收:S-07 遺留 docs sync(ARCH_INDEX + S-07 manager)+ S-08 sprint 日誌。

## 2. 怎麼跑

見 `RELEASE_v0.5.2.md` verification matrix(6-leg 全 PASS + reproduce 指令)。

## 3. 新 token / flag / API

無。純 bugfix + test。

## 4. 仍 deferred(S-08 scope 內接續)

1. **AS-37 ALS commandlet PIE crash 查證** — first action:S-08 plan 的 AS-37-u1 dispatch(read-only 重現 + 定位 `LoadObject` 時序,證據在 `Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp:L138-146` 一帶)→ 回報後 user 選 (a) 文件化 / (b) 深挖。
2. **v0.5.1 三 NITs(NITS-u1)** — first action:reorder `ArchSimPortalFramePIESmokeTest.cpp` DEFINE 區塊 + `run_pie_gate.ps1` stderr/時戳(ASCII-only regex)。
3. **AS-08 SPUD save/load 線(u1 wiring + u2 PIE smoke)** — first action:讀 `Plugins/SPUD/` reflection 掃描條件 → RF_Transient audit `ArchSimMemberData` spawn paths。
4. **AS-38(LOW,本輪新開)** — first action:`ArchSimScenarioWidget.cpp` `check(Root)` → if-guard + log + destroy;SC8 comment;SC9 走 production path。
5. **user-driven PIE P10/P11 人工 re-verify(human)** — first action:開 Editor 按 `docs/logs/S-05/u3_pie_smoke.md` P10/P11,確認 heatmap 顏色視覺正確(v0.5.2 起理論上第一次可能成功)。

## 5. 過程留下的教訓(durable)

1. **裸 `AActor` + `SpawnActor(Location)` = 位置靜默丟失** — S-01 在 test 端踩過(v0.1.1),S-05 在 production 重踩。凡 spawn 純 `AActor::StaticClass()` 且後續依賴 `GetActorTransform()`,必 graft `USceneComponent` root。已寫進 widget 註解;建議未來 review checklist 對每個新 SpawnActor call site 查 root。
2. **上一 sprint 的 close-docs 會以髒檔遺留**(manager.md 描述 commit 的文字不可能在該 commit 內)— 下一輪 release 收編並在 notes 揭露即可;reviewer 對「未申報檔案」的 finding 要先查歸屬再定罪。
3. **Reviewer 誤歸因是真實風險** — Phase 3 finding #3 把 S-07 遺留當成 subagent 越界;主對話用 diff 內容(描述 `4567c40` 的文字)證偽。裁定紀錄在 `docs/logs/S-08/manager.md`。

## 6. 後續方向

- S-08 收完 AS-37 / NITs / AS-08 → Phase 6 Mode A minor bump **v0.6.0**。
- v0.6.0 後:student-trial 前置(AS-04/05 美術 + 人工 P1..P15)。

---

接手有問題:`HANDOFF_v0.5.1.md` → 本檔 → `docs/logs/S-08/manager.md`。
