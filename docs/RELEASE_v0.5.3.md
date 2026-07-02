# v0.5.3 — PIE 測試基礎設施收斂(NITs closeout + AS-37 closed)

**Date:** 2026-07-02
**Sprint:** S-08(units NITS-u1 + AS-37-u1 查證 + AS-37-u2;bundled tag)
**Baseline:** v0.5.2(`2fb0f4e`);中繼 commit `a016486`(NITS-u1)
**Repo:** https://github.com/rocky59487/architect_simulator

## 一句話

v0.5.1 遺留的三個 PIE-gate cosmetic NITs 全關 + AS-37(ALS commandlet PIE crash)查證結案:確認為 **commandlet-only**(不影響 packaged production),處置 (a)+(b-1) — 文件化 + 可重用 sidestep helper `ArchSimPieHarness::OverrideGameModeForSafePIE()`,為 AS-08 SPUD PIE 測試鋪路。

## AS-37 查證結論(讀 crash log 實證,零 production 改動)

- **Crash 鏈(精確版,修正舊文檔記載):** commandlet 模式 AssetRegistry 在 pawn-spawn 時序未及索引 `/ALS/` content → `AArchSimCharacter::LoadAlsAssetsLate()` 的 4 個 `LoadObject` 全敗(CDO + spawn 各一輪)→ **首發崩點 `AlsCharacterMovementComponent.cpp:L894/L903`(ensures)** → 終崩 `AlsCharacter.cpp:L526` `NotifyLocomotionModeChanged` null deref → EXCEPTION_ACCESS_VIOLATION。
- **Severity:** commandlet-only。cooked/pak packaged build 不受影響;caveat:Development `-game` 無 pak 場景未排除(記錄於 ARCH_INDEX § 7)。
- 證據:`Saved/Crashes/UECC-Windows-7ECCED384D44B2CCD56A45B7F390734D_0002`(2026-06-28)等既有 crash log;user-driven Editor PIE 不觸發(AssetRegistry 已完成 scan)。

## 修了什麼

| 項 | File | 內容 |
|---|---|---|
| NIT 1 | `ArchSimPortalFramePIESmokeTest.cpp` | latent command DEFINE 區塊 reorder = RunTest 呼叫順序(sorted-set diff 驗證純平移,0 內容變化) |
| NIT 2 | `Scripts/run_pie_gate.ps1` | `\| Out-Null` 由慣例轉 explicit WHY 註解(PS5.1 NativeCommandError 紀律) |
| NIT 3 | `Scripts/run_pie_gate.ps1` | stale-log 防護:pre-run 時戳 + **長度**雙條件(stale = time 未進 AND length 未變);時戳單條件版在真實 run 出現過 same-timestamp 偽陽性(NITS-u1 review 預言、AS-37-u2 期間應驗),v0.5.3 內即修;scratch oracle 4 分支 PASS |
| AS-37 (b-1) | `ArchSimPieHarness.h/.cpp` | 新 helper `OverrideGameModeForSafePIE(FAutomationTestBase*)`:封裝 CreateNewMap + `WorldSettings->DefaultGameMode = AGameModeBase` sidestep;`#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR` 保護;WHY 註解含完整 crash 鏈 + **未來規約:所有 commandlet PIE test(含 AS-08-u2 SPUD smoke)必須呼叫本 helper** |
| AS-37 (b-1) | `ArchSimPortalFramePIESmokeTest.cpp` | inline sidestep(30 行)→ helper 呼叫(4 行);行為恆等(review 逐分支對照;TestNotNull structured record 由 integrator 補回 helper 保持完整 parity) |
| AS-37 (a) | `docs/ARCHITECTURE_INDEX.md` | § 7 AS-37 row → ✅ closed(結論 + caveat + 規約);§ 6 AS-35 row sidestep 描述同步 helper 化 |

**Engine source delta vs v4.0.0 FROZEN baseline:0 行。** 無新 test class(`ExpectedUeTests` 149 不變);無版本 pin 變動。

## Verification matrix

| Leg | Status | Reproduce |
|---|---|---|
| 1. standalone F1..F71 | **PASS** | `Plugins\FrameSolver\Standalone\build.bat` |
| 2. UE automation 149 | **PASS**(exit 0) | `Scripts\run_gate.ps1 -RequireOpenSees`(非 cuDSS host 加 `-ExpectedUeTests 147`) |
| 3. OpenSees | **PASS** | 同上 |
| 4. deep audit 104 | **PASS** | 同上 |
| 5. CLI round-trip | **PASS** | 同上 |
| 6. PIE auto-smoke | **PASS**(exit 0;screenshot 33177 bytes;含修訂版 stale guard live 驗證) | `Scripts\run_pie_gate.ps1 -Root . -Engine $env:UE_ENGINE_ROOT -UProject .\ArchSim.uproject` |

Gate 完整跑於 2026-07-02 03:02 UTC(tag 前 fresh run)。

## Known issues / Deferred

- **AS-38(LOW,S-08 開)**:`PlaceKSetMember` `check(Root)` shipping-safe null guard;SC8 comment 強保證說明;SC9 走 production path。
- **user-driven PIE P10/P11 人工 re-verify**(v0.5.2 起)照舊 pending。
- helper `Test = nullptr` 時 null-world 失敗無聲(現有 caller 皆傳 `this`;記錄於 agent log)。
- AS-08(SPUD 存檔線)為本 sprint 下一 unit,目標 v0.6.0。

## Breaking changes

None。

## Tag plan

```powershell
git push origin main
git push origin v0.5.3
gh release create v0.5.3 --title "v0.5.3 — PIE test infra closeout (NITs + AS-37 sidestep helper)" --notes-file docs/RELEASE_v0.5.3.md
```
