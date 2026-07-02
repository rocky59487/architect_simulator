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
