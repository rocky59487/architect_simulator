# v0.5.2 — PlaceKSetMember 端點退化 bugfix(AS-36)

**Date:** 2026-07-02
**Sprint:** S-08(unit AS-36-u1)
**Baseline:** v0.5.1(commit `4567c40`)
**Repo:** https://github.com/rocky59487/architect_simulator

## 一句話

`UArchSimScenarioWidget::PlaceKSetMember` spawn 的裸 `AActor` 沒有 RootComponent,SpawnActor 的世界位置被 UE 靜默丟棄 → 所有 K-set 構件端點退化成「以原點為基準的 offsets」→ 兩根 K1 柱共用同一 node pair → LDLT rank-deficient → solve 靜默失敗 → HeatmapActor 永不出現。v0.5.2 修正 spawn path(graft `USceneComponent` root),portal frame 自此在 commandlet PIE 與 user-driven PIE 皆可解。

## Root cause(數值 trace 摘要)

- `ArchSimModelRegistry.cpp:197-199` 端點計算:`GetOwner()->GetActorTransform().TransformPosition(EndIOffsetUE)`。
- 裸 `AActor` 無 RootComponent → `GetActorTransform()` 恆 `Identity` → ColA(origin `(-100,0,100)` cm)與 ColB(origin `(+100,0,100)` cm)的端點都算成 `(0,0,∓100)` cm → `FindOrAddNode` 正確 dedup(它沒錯)→ `Member[0] I=2 J=3` 與 `Member[1] I=2 J=3`。
- 兩柱構成不接支承的 floating substructure(12 free DOF 無約束)→ mechanism → `bSingular` → `OnSolveComplete` singular guard 擋下 heatmap spawn。
- **S-01(v0.1.1)曾在 test 端踩過同一雷**(`docs/SPRINT_NOTES.md` A1-07 段);production `PlaceKSetMember` 是 S-05 寫的,重複了同一坑。本次修在 production。

## 修了什麼

| File | Delta | 內容 |
|---|---|---|
| `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp` | +41 行 | `PlaceKSetMember` spawn 後 graft `USceneComponent` root(`NewObject` → `RegisterComponent` → `SetRootComponent` → `SetActorLocation(LocationWorld)`),含 WHY 註解 cite S-01 教訓 + AS-36 |
| `Source/ArchSim/Private/Tests/ArchSimScenarioFixtureTest.cpp` | +250 行 | SC8:headless 兩構件不同位置 → node pair 相異 + node count == 4;SC9:root graft 後 `SetActorLocation` → `GetActorLocation` 回歸 |

**影響面:** `PlaceK1Column` / `PlaceK2Beam` / `PlaceK4Brace` / `SpawnDefaultPortalFrame` 全部經過 `PlaceKSetMember` — 全部由壞轉好。無行為「改變」,只有「修正」。

**Engine source delta vs v4.0.0 FROZEN baseline:0 行**(FrameCore / LevelCore 皆未動)。無版本 pin 變動(engine FROZEN;`ExpectedUeTests` 149 不變,SC8/SC9 為既有 test 的 sub-check)。

## Verification matrix

| Leg | Status | Reproduce |
|---|---|---|
| 1. standalone F1..F71 | **PASS**(ALL PASS, failures=0) | `Plugins\FrameSolver\Standalone\build.bat` |
| 2. UE automation 149 tests | **PASS**(exit 0) | `Scripts\run_gate.ps1 -RequireOpenSees`(非 cuDSS host 加 `-ExpectedUeTests 147`) |
| 3. OpenSees cross-validation | **PASS** | 同上(`-RequireOpenSees` 強制) |
| 4. linear deep audit 104 checks | **PASS**(failures=0) | 同上 |
| 5. CLI round-trip | **PASS**(ALL PASS) | 同上 |
| 6. PIE auto-smoke | **PASS**(exit 0;screenshot 33177 bytes;SC2b 相異 node pairs;SC4 HeatmapActor found) | `Scripts\run_pie_gate.ps1 -Root . -Engine $env:UE_ENGINE_ROOT -UProject .\ArchSim.uproject` |

Fix 後 leg 6 關鍵 log(`Saved/Logs/ArchSim.log`):
```
SC2b: Member[0] I=0 J=2   ← ColA(修前 I=2 J=3)
SC2b: Member[1] I=1 J=3   ← ColB(修前 I=2 J=3,與 ColA 重複)
SC2b: Member[2] I=2 J=3   ← Beam
SC4 [VERIFIED]: AFrameUtilizationHeatmapActor found in PIE world after solve.
```

環境前置:`$env:UE_ENGINE_ROOT` 指向 UE 5.7 root;conda env `framecore-direct`(leg 1/4);`openseespy` pip(leg 3);leg 6 **不可加 `-nullrhi`**。

## Known issues / Deferred

- **AS-38(LOW,本輪 review 開)**:`PlaceKSetMember` 的 `check(Root)` 在 shipping build 會被 strip,`NewObject` 極端失敗時無保護 → 改 if-guard;SC8 comment 補「node-count==4 才是完全相異的強保證」說明;SC9 強化為直接走 `PlaceKSetMember` production path。
- **user-driven PIE P10/P11 需人工 re-verify**:本 fix 影響所有 K-set placement;S-06 在 `docs/logs/S-05/u3_pie_smoke.md` P10/P11 的「portal frame heatmap 可見」宣稱,經查在 v0.5.2 之前不可能成立(production bug,非 commandlet-only)。commandlet PIE 已驗 heatmap spawn;「看起來對不對」仍需人在 Editor 執行 P10/P11。
- AS-37(ALS commandlet PIE crash)照舊 backlog — S-08 下一 unit 查證。

## Housekeeping(本 commit 一併收,揭露)

- `docs/ARCHITECTURE_INDEX.md` + `docs/logs/S-07/manager.md`:S-07 close(v0.5.1)寫於 release commit 之後的 docs sync,屬上一 sprint 遺留之未 commit 內容,本 commit 收錄(非 v0.5.2 變更的一部分)。
- `docs/logs/S-08/`:本 sprint 的 scope / plan / agent / manager 日誌(至 dispatch-review 階段)。

## Breaking changes

None。

## Tag plan

```powershell
git push origin main
git push origin v0.5.2
gh release create v0.5.2 --title "v0.5.2 — PlaceKSetMember node-pair degeneration fix (AS-36)" --notes-file docs/RELEASE_v0.5.2.md
```
