# v0.5.4 — SPUD save/load 生產接線(AS-08-u1)

**Date:** 2026-07-02
**Sprint:** S-08(unit AS-08-u1,iteration 2)
**Baseline:** v0.5.3(`03b6350`)
**Repo:** https://github.com/rocky59487/architect_simulator

## 一句話

v0.1 起承諾的 persistence chain 第一段收口:新 `UArchSimPersistenceSubsystem`(GameInstanceSubsystem)以 **SPUD global-object sidecar** 路線接通 save/load — placement/support 記錄存 sidecar,load 時清場 registry 後重放(spawn + root graft + 再註冊 + debounce solve),不依賴 SPUD respawn 裸 actor。Headless 可驗部分全數落地;PIE end-to-end 交 AS-08-u2。

## RF_Transient audit 正式結論(S-01 Decision Log 遺留疑慮收案)

- SPUD 的 persistence gate 是 **`ISpudObject` opt-in**(`SpudPropertyUtil.cpp:1342` `IsPersistentObject` = `Implements<USpudObject>() && !ShouldSkip`),**不是 `RF_Transient`**(`SpudState.cpp:1133` StoreActor filter = `RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed`)。
- `UArchSimMemberData` 不實作 `ISpudObject` → 設計上排除於 SPUD scan;sidecar 是正確補償機制。S-01 的 RF_Transient 疑慮:**不成立,但 sidecar 仍必要**(理由見下)。

## Sidecar 路線論據(SPUD source 逐點確認)

1. `EndIOffsetUE/EndJOffsetUE` 非 SaveGame property → SPUD property scan 拿不到幾何。
2. `UArchSimMemberData` 是 `AddInstanceComponent` 動態掛載;SPUD `RestoreObjectProperties`(`SpudState.cpp:755`)掃 class property,不掃 instance component。
3. 固定支承(`RegisterFixedSupport`)只存在 `FFrameModelDef.Nodes[]`,無 actor 代表 — actor 還原路徑救不回。

## 新增 surface

| 項 | 內容 |
|---|---|
| `UArchSimPersistenceSubsystem`(new,+167/+453) | `Initialize()` 以 `AddPersistentGlobalObjectWithName` 註冊穩定 key;`SaveToSlot`(IsIdle 檢查 → `SnapshotCurrentModel` 填 `FArchSimMemberRecord[]`(WorldTransform/EndI/J offsets/Group/Mat/Sec)+ `SupportPositions[]` → SPUD `SaveGame`);`LoadFromSlot` → SPUD `LoadGame` → `OnPostLoadGame` → `ReplayLoadedSidecar`(Registry `Reset()` → supports → 每 record spawn AActor + **USceneComponent root graft(AS-36 教訓)** + SetActorTransform + MemberData 設定 + RegisterMember → `RequestSolve`);slot 慣例 header 文檔化(`ArchSimSlot_N` + SPUD `__QuickSave__`/`__AutoSave__N__`) |
| `UArchSimModelRegistry::Reset()`(+13/+35) | load 前清場;teardown 順序同 `Deinitialize`(EndSession → ClearTimer → 清 fields),清後可續用 |
| `ArchSim.Build.cs` | SPUD 依賴(**Private** — header 不洩 SPUD 型別;review finding #2 修正) |
| 4 個 headless 測試(+486) | `ArchSim.Persistence.SpudEmptyModelSave / SpudRfTransientAudit / SpudSidecarClearSemantics / SpudSidecarRoundtrip`(SC1-SC14;RF_Transient audit 以 `SpudPropertyUtil::IsPersistentObject` 真實呼叫為 oracle;SC6 為 value-copy + reflection flag 驗證,**非 binary roundtrip** — 誠實標記於 test comment,真 roundtrip 是 AS-08-u2 PIE 範圍) |
| `ArchSimScenarioWidget.cpp`(+1/-2) | 重複 `DECLARE_LOG_CATEGORY_EXTERN` 改 include 化(順手修正,無語意變化) |

**語意約定:** `MemberIdx` 於 load 後單調重分配(replay 順序 = save 順序);`CachedUtilization` 還原值僅為首次 solve 前 display fallback,真實 D/C 由 post-load solve 重算。

**Engine source delta vs v4.0.0 FROZEN baseline:0 行。** SPUD plugin 源碼 0 行(僅讀取)。

## Verification matrix

| Leg | Status | Reproduce |
|---|---|---|
| 1. standalone F1..F71 | **PASS** | `Plugins\FrameSolver\Standalone\build.bat` |
| 2. UE automation **153**(149+4) | **PASS** | `Scripts\run_gate.ps1 -RequireOpenSees`(非 cuDSS host 加 `-ExpectedUeTests 151`) |
| 3. OpenSees | **PASS** | 同上 |
| 4. deep audit 104 | **PASS** | 同上 |
| 5. CLI round-trip | **PASS** | 同上 |
| 6. PIE auto-smoke | **PASS** | `Scripts\run_pie_gate.ps1 -Root . -Engine $env:UE_ENGINE_ROOT -UProject .\ArchSim.uproject` |

(Gate 於 tag 前 fresh 全跑;本檔發布時以實跑輸出為準。)

## Known issues / Deferred

- **PIE-only 驗證清單(AS-08-u2,本 sprint 下一 unit):** PIE-1 真實 `.sav` 寫入;PIE-2 load→replay→solve→heatmap 全鏈;PIE-3 SPUD PIE 0.2s `NewGame` delay 時序;PIE-4 PostLoadGame 不 double-fire;PIE-5 transform 一致;PIE-6 support 1mm 對齊;PIE-7 CachedUtilization 刷新;PIE-8 Snapshot 遇已 Destroy 的 active member(silent-skip Warning 路徑);PIE-9 replay RegisterMember 失敗的 orphan actor(future cleanup 追蹤)。全部須用 `ArchSimPieHarness::OverrideGameModeForSafePIE()`。
- headless `NewObject<Subsystem>` 的 `ClassWithin=GameInstance` warning:既有 pattern,測試 header 已說明。
- AS-38(LOW)照舊 backlog;user-driven PIE P10/P11 人工 re-verify 照舊 pending。

## Breaking changes

None(純新增 surface;既有 API 不變)。

## Tag plan

```powershell
git push origin main && git push origin v0.5.4
gh release create v0.5.4 --title "v0.5.4 — SPUD save/load production wiring (AS-08-u1)" --notes-file docs/RELEASE_v0.5.4.md
```
