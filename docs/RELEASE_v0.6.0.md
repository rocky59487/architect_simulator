# v0.6.0 — Persistence chain 收口(S-08 close:AS-36 + AS-37 + NITs + AS-08)

**Date:** 2026-07-02
**Sprint:** S-08 close(Mode A minor bump;本 sprint 第 4 個 tag)
**Baseline:** v0.5.4(`679032a`);sprint 基線 v0.5.1(`4567c40`)
**Repo:** https://github.com/rocky59487/architect_simulator

## 一句話

v0.1 起承諾的 persistence chain 完整收口:SPUD sidecar 存檔線(v0.5.4)加上本 tag 的 **PIE end-to-end smoke**(`ArchSim.PIE.SaveLoadSmoke`:save → `.sav` 實檔 → replay 重建 → solve → 1mm 幾何斷言),leg 6 升級為全 `ArchSim.PIE` category(2 測試)。S-08 四項任務(AS-36 / AS-37 / v0.5.1 NITs / AS-08)全數關閉。

## 本 tag 內容(vs v0.5.4)

| File | Delta | 內容 |
|---|---|---|
| `Source/ArchSim/Private/Tests/ArchSimSaveLoadPIESmokeTest.cpp` | new,~700 行 | `ArchSim.PIE.SaveLoadSmoke`:IMPLEMENT_COMPLEX + 13-step LatentCommand 鏈,30 個硬斷言;pre-step 依規約呼叫 `OverrideGameModeForSafePIE()`;teardown 刪 `__PieSmoke__` slot |
| `Scripts/run_pie_gate.ps1` | +57/-5(+review 註解) | leg 6 改跑整個 `ArchSim.PIE` category;新 result-log selection(pre-run 清 live log 防 rotation 誤讀);EXIT CODE 仍為 authoritative;screenshot 由 PortalFrameSmoke 供給(MAINTENANCE NOTE 已註記) |
| `Scripts/run_gate.ps1` | +3/-1 | leg 6 顯示標頭更新(純 cosmetic) |

**PIE-1..PIE-9 覆蓋(誠實分級,標記與實作經 review 對齊):**
- **[VERIFIED]** PIE-1 `.sav` 實檔(exists+size>0)/ PIE-3 SPUD NewGame 時序 / PIE-5 member transform 1mm / PIE-6 support node 1mm
- **[PARTIAL]** PIE-2 replay 鏈(直驅 `ReplayLoadedSidecar` 等效路徑 — `LoadFromSlot` 的 `OpenLevel` 會斷 PIE latent 鏈(`SpudSubsystem.cpp:977`),全鏈留人工/未來)/ PIE-7 CachedUtilization(soft check:AddWarning 非硬斷言,避免 debounce timing flake)
- **[DEFERRED]** PIE-4 PostLoadGame double-fire(replay-only 模式不可觸發)/ PIE-8 Destroy'd member snapshot / PIE-9 orphan 觀察(各附理由於測試檔頭)

**Engine source delta vs v4.0.0 FROZEN baseline:0 行。** Production(ArchSim)source delta vs v0.5.4:0 行(純 test + gate script)。`ExpectedUeTests` 153 不變(PIE COMPLEX 不入 leg 2)。

## S-08 sprint 總覽(v0.5.1 → v0.6.0,4 tags 全 published)

| Tag | 內容 |
|---|---|
| v0.5.2 | AS-36:PlaceKSetMember 端點退化 fix(裸 AActor 無 RootComponent → 位置被吞 → 雙柱同 node pair → singular);SC8/SC9 回歸 |
| v0.5.3 | v0.5.1 三 NITs + AS-37 結案(commandlet-only;`OverrideGameModeForSafePIE()` helper + 規約);stale-log guard(time+length) |
| v0.5.4 | AS-08-u1:SPUD sidecar 存檔接線 + RF_Transient audit 收案(gate = ISpudObject opt-in);`Registry::Reset()`;4 headless tests(153) |
| **v0.6.0** | AS-08-u2:PIE save/load smoke + leg 6 category 化;**S-08 scope 全關** |

## Verification matrix(v0.6.0 tag 前)

| Leg | Status | Reproduce |
|---|---|---|
| 1. standalone F1..F71 | **PASS** | `Plugins\FrameSolver\Standalone\build.bat` |
| 2. UE automation 153 | **PASS** | `Scripts\run_gate.ps1 -RequireOpenSees`(非 cuDSS `-ExpectedUeTests 151`) |
| 3. OpenSees | **PASS** | 同上 |
| 4. deep audit 104 | **PASS** | 同上 |
| 5. CLI round-trip | **PASS**(重載 host 下可慢至 ~8 分,非卡) | 同上 |
| 6. PIE ×2(PortalFrame + SaveLoad) | **PASS**(exit 0) | `Scripts\run_pie_gate.ps1 -Root . -Engine $env:UE_ENGINE_ROOT -UProject .\ArchSim.uproject` |

完整 gate 於 tag 前單次全綠(`GATE: PASS`);review small-fixes(註解級)後 rebuild + leg 6 重驗 PASS。

## Known issues / Deferred(接 S-09)

- **AS-38(LOW)**:PlaceKSetMember `check(Root)` shipping guard + SC8/SC9 強化。
- **PIE 深水區(本 tag 誠實邊界)**:LoadFromSlot 全鏈(OpenLevel)自動化、PIE-4/8/9 — 需 PIE latent 鏈跨 map reload 的技術 spike 或人工驗證。
- **user-driven 人工驗證(human)**:P1..P15 + P10/P11(v0.5.2 fix 後首次真實驗證)仍是「ready for student trial」的 canonical gate。
- AS-04 / AS-05(美術)/ AS-29(LOW)/ AS-06 / AS-09 照舊。

## Breaking changes

None。

## 發布

```powershell
git push origin main && git push origin v0.6.0
gh release create v0.6.0 --title "v0.6.0 — Persistence chain complete (S-08 close)" --notes-file docs/RELEASE_v0.6.0.md
```
