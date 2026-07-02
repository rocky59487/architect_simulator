# 交接指南 — `v0.5.4` 後接手 owner

> **From:** `v0.5.4`(S-08 unit AS-08-u1)
> **Date:** 2026-07-02
> **Prior handoffs:** [`HANDOFF_v0.5.3.md`](HANDOFF_v0.5.3.md) / [`HANDOFF_v0.5.2.md`](HANDOFF_v0.5.2.md)
> **Release notes:** [`RELEASE_v0.5.4.md`](RELEASE_v0.5.4.md)
> **Sprint log:** [`docs/logs/S-08/`](logs/S-08/)(**sprint 仍開著** — 最後一 unit AS-08-u2 後 Phase 6 close → v0.6.0)

---

## Z-01 first action on day 1

**S-08 只剩 AS-08-u2(SPUD PIE smoke)。** 接手看 `~/.claude/state/work-phase.txt` 依 recovery protocol 續跑。

```powershell
cd E:/project/ArchSim
git log --oneline -3          # 期望 v0.5.4 release commit 在頂
.\Scripts\run_gate.ps1 -RequireOpenSees    # 期望 GATE: PASS(153 + leg 6)
```

## 1. `v0.5.4` = 什麼

一句話:SPUD 存檔線生產接線(sidecar 重放路線)+ RF_Transient audit 收案(結論:非 barrier,gate 是 ISpudObject opt-in)。
Delta:新 PersistenceSubsystem(+620)+ Registry.Reset(+48)+ 4 headless tests(+486)+ Build.cs SPUD Private 依賴;engine FROZEN 0 行;ExpectedUeTests 149→**153**(147→151)。

## 2. 新 API

- `UArchSimPersistenceSubsystem::SaveToSlot(SlotName)` / `LoadFromSlot(SlotName)`(BP-callable;slot 慣例見 header)
- `UArchSimModelRegistry::Reset()`(load 前清場;清後可續用)

## 3. 仍 deferred

1. **AS-08-u2 SPUD PIE smoke(最後一 unit)** — first action:用 AS-35 LatentCommand 模板 + `OverrideGameModeForSafePIE()` 實作 PIE-1..PIE-9 清單(RELEASE_v0.5.4 § Known issues 有完整清單);跑完 → Phase 6 close v0.6.0。
2. **AS-38(LOW)** / **user-driven P10/P11 human re-verify** 照舊。
3. Replay orphan cleanup(PIE-9 追蹤項)— 若 AS-08-u2 驗出實害再開 AS-XX。

## 4. 過程留下的教訓(durable,本 tag 新增)

1. **SPUD 的 persistence gate 是 ISpudObject opt-in,不是 RF_Transient** — 讀 source(`SpudPropertyUtil.cpp:1342` / `SpudState.cpp:1133`)推翻了 S-01 以來的假設;「查 source 收案舊疑慮」比沿用傳言便宜。
2. **GameInstanceSubsystem 跨 OpenLevel 存活**是 SPUD global-object sidecar 能在 PostLoadGame 讀到還原資料的關鍵(restore 發生在 OpenLevel 前)。
3. **Session 中斷的收殘局模式**:iteration 2 fresh agent + 「所有權轉移,逐件 KEEP/FIX/REWRITE」規約,能無損接手半成品(本 unit 全件 KEEP)。
4. Subagent 違規 spawn 子 agent 的矯正:SendMessage 現場糾正 + digest 轉發,比砍掉重跑省一半 token。

## 5. 後續方向

AS-08-u2 收 → Phase 6 Mode A **v0.6.0**(persistence chain 完整收口的 minor)。之後:student-trial 前置(AS-04/05 美術 + 人工 P1..P15 + P10/P11 re-verify)。

---

接手有問題:`HANDOFF_v0.5.3.md` → 本檔 → `docs/logs/S-08/manager.md`。
