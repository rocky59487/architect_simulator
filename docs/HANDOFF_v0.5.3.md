# 交接指南 — `v0.5.3` 後接手 owner

> **From:** `v0.5.3`(S-08 units NITS-u1 + AS-37-u1/u2;bundled tag,含中繼 commit `a016486`)
> **Date:** 2026-07-02
> **Prior handoffs:** [`HANDOFF_v0.5.2.md`](HANDOFF_v0.5.2.md) / [`HANDOFF_v0.5.1.md`](HANDOFF_v0.5.1.md)
> **Release notes:** [`RELEASE_v0.5.3.md`](RELEASE_v0.5.3.md)
> **Sprint log:** [`docs/logs/S-08/`](logs/S-08/)(**sprint 仍開著** — AS-08 兩 unit 未收,目標 v0.6.0)

---

## Z-01 first action on day 1

**S-08 sprint 在 v0.5.3 時仍進行中。** 接手先看 `~/.claude/state/work-phase.txt`;非 idle 就依 /work Compaction Recovery Protocol 續跑(下一 unit = AS-08-u1),**不要**重開 scope。

```powershell
cd E:/project/ArchSim
git log --oneline -4      # 期望 v0.5.3 release commit + a016486 + 2fb0f4e(v0.5.2)+ 4567c40(v0.5.1)
.\Scripts\run_gate.ps1 -RequireOpenSees    # 期望 GATE: PASS(149 + leg 6)
```

## 1. `v0.5.3` = 什麼

一句話:PIE 測試基礎設施收斂 — v0.5.1 三 NITs 關 + AS-37 查證結案(commandlet-only)+ sidestep helper 落地,AS-08-u2 的 SPUD PIE test 直接受惠。
Source delta:test-side 3 檔 + gate script 1 檔 + docs;engine FROZEN 0 行;`ExpectedUeTests` 149 不變。

## 2. 新 API(test-side)

`ArchSimPieHarness::OverrideGameModeForSafePIE(FAutomationTestBase* Test = nullptr)`(`Source/ArchSim/Private/Tests/ArchSimPieHarness.h`)
**規約:所有 commandlet PIE test 的 RunTest() pre-step 必須呼叫(除非測試目標就是 ALS character)。** WHY 與 crash 鏈在 helper 註解 + ARCH_INDEX § 7 AS-37 row。

## 3. 仍 deferred(S-08 scope 內接續)

1. **AS-08-u1 SPUD save/load 生產接線** — first action:讀 `Plugins/SPUD/` reflection 掃描條件 → RF_Transient audit(`docs/logs/S-08/plan_2026-07-02T0019Z.md` § AS-08-u1 有完整 spec)。
2. **AS-08-u2 SPUD PIE smoke** — first action:用 AS-35 LatentCommand 模板 + `OverrideGameModeForSafePIE()`(規約已立)。
3. **AS-38(LOW)** — first action:`ArchSimScenarioWidget.cpp` `check(Root)` → if-guard。
4. **user-driven PIE P10/P11 人工 re-verify(human)** — v0.5.2 起 pending。

## 4. 過程留下的教訓(durable,本 tag 新增)

1. **時戳單條件 stale 偵測在 NTFS 上會偽陽性** — reviewer 預言、當天真實 run 應驗;修法 = time AND length 雙條件。凡「檔案是否被本輪更新」的判斷都該雙訊號。
2. **Helper 抽取要逐分支對照原 inline** — TestNotNull 這類 structured record 的消失不影響 pass/fail 但改變 report 形狀;review 的「行為恆等」要看 report 層不只結果層。
3. **Subagent budget 校準(三點資料)**:凡收尾跑全 gate 的 unit,call 預算下限 ~80(每次 gate ≈ 15-30 calls);cosmetic 40-call 是系統性低估;校準後 AS-37-u2 首次全額度內完成。

## 5. 後續方向

S-08 收 AS-08 兩 unit → Phase 6 Mode A minor bump **v0.6.0**(SPUD 存檔線 = v0.1 起承諾的 persistence chain 收口)。之後:student-trial 前置(AS-04/05 美術 + 人工 P1..P15 + AS-36 修後首次真實 P10/P11)。

---

接手有問題:`HANDOFF_v0.5.2.md` → 本檔 → `docs/logs/S-08/manager.md`。
