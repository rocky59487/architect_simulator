# v0.6.1 — S-09 audit-hardening patch(10 findings 全修 + sidecar v2 + PIE gate 收緊 + clean-checkout reproducibility)

**Date:** 2026-07-02
**Sprint:** S-09(單一 patch tag;5 dispatch units aggregate)
**Baseline:** v0.6.0
**Repo:** https://github.com/rocky59487/architect_simulator

## 一句話

外部 audit 回報的 10 個 findings(2 Blocker + 6 High + 2 Medium)經 verify-first 逐條核實**全數為真**並修復,加上 AS-38 backlog 收案:save/load 資料破壞風險關閉、sidecar 升級 **format v2 完整 model-state persistence**、PIE gate oracle 全面收緊、4 個第三方 plugin 的 clean-checkout reproducibility 閉環、docs claim 誠實化(含 v0.6.0 release notes ERRATA)。

## Findings → 修復對照

| Finding | 嚴重度 | 修復(unit) |
|---|---|---|
| #1 clean-checkout 不可重現(4 plugin untracked + 無 manifest) | Blocker | AS-39:`docs/THIRD_PARTY.md`(pinned SHA manifest)+ `Tools/patches/` 4 patch 全可自動 apply(ALS patch 重生成修格式,語意 0 變)+ `Scripts/setup_third_party.ps1`(冪等,`-DryRun`/`-SkipShaCheck`)+ `run_gate.ps1` precondition fail-loud;`Content/BP_ArchSimScenarioWidget.uasset` 改 tracked |
| #2 v0.6.0 persistence claim 過度 | Blocker | AS-43:`RELEASE_v0.6.0.md` 追加 [ERRATA](原文零修改)+ README/ARCH_INDEX 描述對齊現實 |
| #3 SaveToSlot 可覆寫 empty/partial sidecar | Blocker/High | AS-40:partial-snapshot 偵測(不符 → false,不呼叫 SaveGame)+ empty-overwrite guard(`bAllowEmptyOverwrite=false` 預設參數,BP additive)|
| #4 save/load API contract 誤導 | High | AS-40:LoadFromSlot 加 `GetSaveGameInfo` slot-existence pre-check(missing → false);header 註解改誠實 async 語意 |
| #5 sidecar 非完整 physics persistence | High | AS-41:**format v2** — materials/sections library 定義值、member active/`bTensionOnly`/`Release[12]`、nodalLoads/memberUDLs/shells(+pressure)、一般 per-DOF fixity、`SidecarFormatVersion` 欄 + v1 archive 相容(SPUD skip-and-default,`SpudState.cpp:1077`) |
| #6 PIE gate oracle 太鬆 | High | AS-42:`run_pie_gate.ps1` per-test 逐名驗證(`$ExpectedPieTests` 陣列,缺一即 FAIL)+ screenshot freshness(UTC mtime > run start,stale 即 FAIL)+ UE5.7 `Path={}` parser(CJK locale 修正) |
| #7 PIE 斷言可 false-pass | High | AS-42:PortalFrame heatmap AddWarning → **180-tick bounded-poll 硬斷言**;SaveLoad 改 tracked-set oracle(replay 實 spawn 集合,關死舊 actor false-pass) |
| #8 registry lifecycle 狀態污染 | High | AS-40:`Reset()` 清 live component `bRegistered`/`MemberIdx`;`RegisterFixedSupport` 後 session invalidation(EndSession → 下次 solve 全量重建 K);`RegisterMember` non-finite endpoint guard |
| #9 replay 失敗留 orphan actor | Medium | AS-40:RegisterMember 失敗 → `DestroyActor` + 不計數;log 分列 succeeded/failed |
| #10 docs topology drift | Medium | AS-43:README six-leg/165/163、ARCH_INDEX §2/§4/§6/§7/§8/§9/§10 全面 sync、latest links 修正 |
| AS-38(backlog) | LOW | AS-40:PlaceKSetMember `check(Root)` → shipping-safe if-guard + log + destroy;SC8 註解強化;SC9 改走 production `PlaceK1Column` 路徑 |

## 測試面 delta

- Leg-2 UE automation:**153 → 165**(cuDSS)/ 151 → 163(non-cuDSS)
  - +4(AS-40):`ArchSim.Persistence.ResetClearsComponentFlags / RegisterMemberNonFinite / ReplayOrphanGuard / SaveLoadGuards`
  - +8(AS-41):`V2LibraryStructRoundtrip / V2RestoreLibraries / V2InjectLoads / V2FixityApi / V2FormatVersion / V2DeactivatedSaveGuard / V2V1CompatDefaults / N00TensionReleaseWire`
  - 更名:`ReplayOrphanGuard` → `ReplayOrphanDataInvariant`(SC17;automation ID 變更,test 面非 production)
- Leg-6 PIE:2 測試不變,斷言大幅加硬(上表 #6/#7)+ v2 端到端注入斷言(SC_D)+ empty-overwrite guard PIE 直測(SC_E1)

## Production source delta

`Source/ArchSim/` 4 檔(Persistence h/cpp、Registry h/cpp)+ ScenarioWidget.cpp(AS-38 guard);**FrameCore engine FROZEN 0 行**(v4.0.0 stable seal 完整保留);LevelSim 0 行;第三方 plugin 源 0 行(patch 維持既有語意,僅 ALS patch 檔案格式重生成)。

## Verification matrix(v0.6.1 tag 前,單次一鍵全 gate)

| Leg | Status | Reproduce |
|---|---|---|
| precondition(4 plugin + patch fingerprints) | PASS | `Scripts\setup_third_party.ps1 -DryRun` |
| 1. standalone F1..F71 | PASS | `Plugins\FrameSolver\Standalone\build.bat` |
| 2. UE automation **165** | PASS | `Scripts\run_gate.ps1 -RequireOpenSees`(non-cuDSS `-ExpectedUeTests 163`) |
| 3. OpenSees | PASS | 同上 |
| 4. deep audit 104 | PASS | 同上 |
| 5. CLI round-trip | PASS | 同上 |
| 6. PIE ×2(per-test 驗證 + screenshot freshness) | PASS | `Scripts\run_pie_gate.ps1 -Root . -Engine $env:UE_ENGINE_ROOT -UProject .\ArchSim.uproject` |

(fresh clone 前置:`Scripts\setup_third_party.ps1` 安裝 4 個 pinned plugin + patches;見 `docs/THIRD_PARTY.md`。)

## Honest limitations / Known issues

- **LoadFromSlot 全鏈(OpenLevel)自動化不可達**(`SpudSubsystem.cpp:977` PIE latent-chain 斷)— PIE 測試用 replay-equivalent 等效路徑;全鏈驗證維持人工。v0.6.0 以來不變。
- **v2 replay 側 CurrentModel 的 PIE 斷言 [NEW CODE, PIE required]**:snapshot 側(sidecar 欄位填充)PIE [VERIFIED];replay 後 CurrentModel 逐欄位斷言受 OpenLevel 邊界限制,幾何由既有 replay-equivalent 1mm 斷言覆蓋。
- **partial-snapshot guard 與 orphan-destroy 的 PIE fault-injection 直測 DEFERRED**(E-02/E-03:無公開注錯 API;headless 單元已 pin 邏輯,PIE 觸發路徑 [NEW CODE])。empty-overwrite guard 已 PIE 直測 [VERIFIED](SC_E1)。
- **v1 → v2 實檔遷移未實測**(repo 無 v1 `.sav` artifact;v1-compat 由 SPUD skip-and-default 行為 + 模擬態測試覆蓋)。
- **RefVec 不持久化**(replay 由 `PickRefVecForAxis` 幾何重算;手動設定的 non-default RefVec 不存活 — header 有誠實排除記錄)。
- PIE-4/8/9(v0.6.0 起 DEFERRED)照舊。
- `AddExpectedErrorPlain` 以 `Contains` + `Occurrences=1` 匹配 refused-save log — 可再收緊為含 slot name(accepted NIT,綠測前夕不動)。
- `GetSupportCount()` 為 v1-only accessor(v2 sidecar 恆 0)— header 已註明,新 caller 用 `GetNodeFixityCount()`。

## Breaking changes

None。`SaveToSlot` 新參數帶預設值(BP additive);sidecar v2 對 v1 archive 向前相容;USTRUCT 欄位 append-only。測試面:SC17 automation ID 更名(`ReplayOrphanGuard` → `ReplayOrphanDataInvariant`)。

## Deferred(接 S-10;audit ID 對應 HANDOFF_v0.6.1.md first actions)

- **SCRIPT-PATH-01**:`run_pie_gate.ps1` 以 `powershell -File` 巢狀呼叫且傳相對 `-Root . -UProject .\...` 時觀察到 editor 瞬退且無 log(絕對路徑與 run_gate.ps1 整合路徑均正常;tag 前以 raw commandlet 絕對路徑重驗 PASS)。建議 script 入口對 `$Root`/`$UProject` 做 `Resolve-Path` 正規化。
- **E-02 / E-03**:partial-snapshot / orphan 的 PIE fault-injection 直測(需注錯 API 設計)。
- **V2-MIG-01**:v1 → v2 實檔遷移驗證(生成或取得 v0.6.0 `.sav` 後載入)。
- **NIT-EE-01**:AddExpectedErrorPlain 字串含 slot name 收緊。
- Human 驗證日(P1..P15 + P10/P11)仍是「student trial ready」canonical gate(v0.6.0 HANDOFF 建議,順延)。
- AS-29(LOW)/ AS-04/05(human)/ AS-06/09 照舊。

## 發布

```powershell
git push origin main && git push origin v0.6.1
gh release create v0.6.1 --title "v0.6.1 — S-09 audit hardening (10 findings fixed; sidecar v2; PIE gate tightened; reproducible checkout)" --notes-file docs/RELEASE_v0.6.1.md
```
