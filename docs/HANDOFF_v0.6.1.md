# 交接指南 — `v0.6.1` 後接手 owner(S-09 CLOSED)

> **From:** `v0.6.1`(S-09 close;單一 aggregate patch tag,5 dispatch units)
> **Date:** 2026-07-02
> **Prior handoff:** [`HANDOFF_v0.6.0.md`](HANDOFF_v0.6.0.md)(不動,frozen history)
> **Release notes:** [`RELEASE_v0.6.1.md`](RELEASE_v0.6.1.md)(findings→修復對照表 + verification matrix)
> **Sprint log:** [`docs/logs/S-09/`](logs/S-09/)(manager.md 全程 review 記錄 + 5 份 agent log)

---

## Z-01 first action on day 1

**S-09 已關,state 應為 idle。** 開箱驗證(fresh clone 或既有 checkout 皆適用):

```powershell
cd <repo-root>
.\Scripts\setup_third_party.ps1 -DryRun     # 期望 4 plugin 全 [OK],exit 0
.\Scripts\run_gate.ps1 -RequireOpenSees     # 期望 GATE: PASS(UE 165;non-cuDSS -ExpectedUeTests 163)
```

## 1. `v0.6.1` = 什麼

外部 audit 10 findings(2B/6H/2M)verify-first 全數證實並修復 + AS-38 收案:
- **Persistence 資料安全**:SaveToSlot partial/empty 覆寫 guard、LoadFromSlot slot-existence pre-check、replay orphan 銷毀、Registry lifecycle 修(Reset 清 flags / fixity 改動 session invalidation / non-finite guard)。
- **Sidecar format v2**:完整 model-state(libraries 定義值 / active / bTensionOnly / Release[12] / loads / UDLs / shells / 一般 fixity)+ version 欄 + v1 相容。
- **PIE gate 收緊**:per-test 逐名驗證、screenshot freshness(UTC)、heatmap 硬斷言、tracked-set oracle、v2 注入斷言、empty-overwrite PIE 直測。
- **Clean-checkout reproducibility**:`docs/THIRD_PARTY.md` + 4 個可自動 apply 的 patch + `Scripts/setup_third_party.ps1` + gate precondition;`Content/BP_ArchSimScenarioWidget.uasset` 入庫。
- **Docs 誠實化**:`RELEASE_v0.6.0.md` ERRATA(純追加)+ README/ARCH_INDEX 全面 sync(165/163、six-leg、v2 描述)。

Engine FROZEN 0 行;LevelSim 0 行;第三方 plugin 源 0 行(僅 patch 檔格式重生成,語意不變)。

## 2. S-10 建議 scope

1. **Human 驗證日(最高價值,自 v0.6.0 順延)**:user-driven P1..P15 + P10/P11 + 手動 save/load 體感(現在 sidecar v2 落地,save/load 值得重點壓)— 「student trial ready」canonical gate。
2. **V2-MIG-01**:v1→v2 實檔遷移驗證。First action:在 v0.6.0 checkout 產生一個 `.sav`(PIE 手動 save),copy 到 v0.6.1 checkout 的 `Saved/SaveGames/`,PIE LoadFromSlot 後確認 v1 分支 replay 正確、SidecarFormatVersion 保持 <2。
3. **E-02/E-03(PIE fault-injection spike,optional)**:partial-snapshot / orphan 的 PIE 直測。First action:設計 test-only 注錯 API(如 `#if WITH_AUTOMATION_TESTS` 的 Registry 壞 record 注入),先寫 spec 再動工。
4. **NIT-EE-01**:`ArchSimSaveLoadPIESmokeTest.cpp` AddExpectedErrorPlain 字串含 slot name 收緊(一行)。
5. Backlog 照舊:AS-29(LOW)/ AS-04/05(human 美術)/ AS-06/09(deferred)。

## 3. 新 API / 新格式(給後續開發用)

- `UArchSimPersistenceSubsystem::SaveToSlot(SlotName, bAllowEmptyOverwrite=false)` — 空 sidecar 蓋既有 slot 需顯式 opt-in。
- Sidecar v2 欄位與單位:見 `ArchSimPersistenceSubsystem.h` 檔頭 v2 說明區(含 RefVec 誠實排除記錄)。
- `UArchSimModelRegistry` v2 additive API:`RestoreLibraries` / `ApplyFixityAt` / `InjectNodalLoads` / `InjectMemberUDLs` / `InjectShells` / `InjectShellPressures` / `SetMemberFlags` / `FindOrAddNodePublic`(replay 專用;RestoreLibraries 有 already-populated guard)。
- `Scripts/setup_third_party.ps1`:`-DryRun` / `-SkipShaCheck`(zip-extracted escape hatch)。
- `Scripts/run_pie_gate.ps1`:新增 PIE 測試時要同步 `$ExpectedPieTests` 陣列(頂部),否則 gate FAIL(fail-loud by design)。

## 4. 仍 deferred / Pending(與 RELEASE_v0.6.1.md Deferred 區對齊)

| ID | 內容 | First action on day 1 |
|---|---|---|
| E-02 | partial-snapshot guard PIE fault-injection 直測 | 見 §2.3 spike spec |
| E-03 | orphan-destroy PIE fault-injection 直測 | 同上 |
| V2-MIG-01 | v1→v2 實檔遷移 | 見 §2.2 |
| NIT-EE-01 | ExpectedError 字串收緊 | `ArchSimSaveLoadPIESmokeTest.cpp` 搜 `AddExpectedErrorPlain`,把匹配字串補上 slot name 後跑 leg 6 |
| PIE-4/8/9 | v0.6.0 起既有 defer(OpenLevel 邊界)| 不動,除非 E-02/E-03 spike 連帶解 |
| SCRIPT-PATH-01 | run_pie_gate.ps1 相對路徑參數在巢狀 `powershell -File` 下瞬退無 log | `run_pie_gate.ps1` param 區對 `$Root`/`$UProject` 加 `Resolve-Path`;複現法:`powershell -File Scripts\run_pie_gate.ps1 -Root . -UProject .\ArchSim.uproject`(巢狀)vs 絕對路徑對照 |

## 5. 過程留下的教訓(durable,S-09 全程;完整版 manager.md SESSION CLOSE)

1. **Subagent 測試證據必須含 automation controller verdict**(`Test Completed. Result=` + EXIT CODE)— 測試自身 Display print 不算數(AS-41 iter1 BLOCKER 的核心)。
2. **「pre-existing failure」claim 必須附 baseline log 對照**,否則預設 = 本輪 regression(AS-41 iter1 被推翻、iter2 附證後成立 — 本專案測試有 boot 期 smoke 噪音 + 正式 run 才算數的既有模式)。
3. **Canonical 測試指令要在 dispatch prompt verbatim 鎖定** — agent 自加 `-NoShaderCompile` 導致 commandlet world 初始化不完整、全家族 world-null。
4. **Budget 校準**:含 clone/網路/gate 的 unit,call 預算至少 +50%(S-09 四次超支:123/70、48/40、113/100、73/50);且 **budget 自報不可信,以 usage 元數據為準**(AS-43 虛報 45)。
5. **Reviewer 也會違規**(假 delegate + 等待)— SendMessage 糾正一次即回正軌;reviewer 誤用 `git diff HEAD~N`(看已 commit 歷史而非 working tree)出現 2 次,review prompt 要指明 diff 基準。
6. **第三方 patch 檔要能自動 apply 才算 reproducibility**(格式毀損的 patch = 唯讀參考文件);patch 語意 vs 檔案格式是兩回事,重生成格式不算改 patch。

## 6. 後續方向(無排序)

- v0.6.2:E-02/E-03 spike + V2-MIG-01 + 剩餘 NITs(依 roadmap-v0-6-x memory:v0.6.x 收 backlog,v0.7 才開基礎建設)。
- Human 驗證日隨時可插(不佔 /work sprint)。
- 風險區:sidecar v2 的 PIE replay 側斷言仍 [NEW CODE](OpenLevel 邊界)— human 驗證日重點壓 save/load。

---

接手有問題:`RELEASE_v0.6.1.md` → `docs/logs/S-09/manager.md` → 各 `agent_AS-*.md`。Reproducibility 問題讀 `docs/THIRD_PARTY.md` + `Tools/patches/README.md`。
