# Sprint Notes — Architect Simulator

> **用途**:每個 Sprint 的實做筆記、Spike 結論、未在 IMPLEMENTATION_PLAN.md 預測到的發現、跨 Sprint 的 decision log。
> **更新頻率**:每個 Sprint 結束時補上;Spike 結論在當 Sprint 內即時記。
> **與 IMPLEMENTATION_PLAN.md 的關係**:本檔記「實做中發現的真相」,計畫書記「實做前的設計」。兩者衝突以本檔為準。

---

## Sprint S-00(W1-2,Gate 0)— 環境準備 + 三個技術 Spike

**狀態**:🟡 進行中(agent 側 8 個 task 完成,等使用者視覺確認 Plugins 面板)
**起始日期**:2026-06-25
**負責**:agent + 使用者(視覺確認)

### Spike 結論摘要(2026-06-25)

| # | Spike | 結論 | 對計畫書的影響 |
|---|---|---|---|
| 1 | ALS-Refactored v4.17 + UE5.7 build | 🟡 待 agent 填 | (待填) |
| 2 | FDemandSummary USTRUCT 欄位清單 | ✅ **SafetyFactor / MaxDC 都存在** | IMPLEMENTATION_PLAN 多處 `MaxMemberDC` 已全改為 `MaxDC` |
| 3 | UE5.7 PCG 節點名稱 | ⚠️ **2 個重要發現**:(a) **無「Perlin Noise」獨立節點**,改用 `Spatial Noise` 節點 `PCGSpatialNoiseMode::Perlin2D`(`PCGSpatialNoise.h:69`);(b) **無「Attribute By Slope」節點**(完整搜過 Engine/Plugins/PCG / Experimental/PCGBiomeCore / PCGBiomeSample) | 主檔 Part G1 + IMPLEMENTATION_PLAN G1-T1 + A3 已修正;G1 加 Slope Filter 替代選項表(MVP 走 Landscape Spline 老師手動標) |

### 額外觀察(UE Build log L10-11)

⚠️ **SPUD 依賴 `StructUtils` plugin 在 UE5.5 已 deprecated 將被移除**(UE Build log L10-11 警告)。MVP 不擋,但 UE5.8+ 升級會破。已加入主檔 Part L3 風險清單。

### Spike 1:ALS-Refactored v4.17 + UE5.7 build 驗證

**結論**:✅ **PASS**。ALS v4.17 乾淨 build 64 秒,4 個 plugin 全部 link 成功,5-leg gate 沒退化。鐵則 #1 / #5 邊界完整(FrameCore engine source 0 行動 / `Plugins/LevelSim/` 0 行動 / `ArchSim.uproject` 0 行動)。

**Plugin clone**:

| Plugin | Source | Tag/Branch | 目錄 |
|---|---|---|---|
| ALS-Refactored | `Sixze/ALS-Refactored` | tag `4.17`(無 `v` 前綴;HEAD `ba232486`) | `Plugins/ALS/` |
| Prefabricator | `unknownworlds/prefabricator-ue5` | `--depth 1` main | `Plugins/Prefabricator/` |
| SPUD | `sinbad/SPUD` | `--depth 1` main | `Plugins/SPUD/` |
| SUQS | `sinbad/SUQS` | `--depth 1` main | `Plugins/SUQS/` |

**.uplugin EngineVersion 確認**:

| Plugin | 原始值 | 結果 |
|---|---|---|
| ALS | `"5.7.0"` | ✅ 原本就正確,未修改 |
| Prefabricator | **無 EngineVersion 欄位** | 新增 `"EngineVersion": "5.7.0"`(無空格 colon,沿襲 Prefabricator 原檔風格) |
| SPUD | **無 EngineVersion 欄位** | 新增 `"EngineVersion" : "5.7.0"`(保留 sinbad colon-space 風格) |
| SUQS | **無 EngineVersion 欄位** | 新增 `"EngineVersion" : "5.7.0"`(保留 sinbad colon-space 風格) |

**`ArchSim.uproject` 未修改**(鐵則 #5)。4 個 plugin 透過 `.uplugin EnabledByDefault=true`(ALS / SPUD / SUQS)或 `Installed=true`(Prefabricator)自動載入,不需 explicit 列在 `.uproject.Plugins[]`。

**`Config/DefaultEngine.ini`** 新增 section(ALS README 強制要求):

```ini
[/Script/Engine.InputSettings]
DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput
DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent
```

**UE Build** — `%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project="<repo-root>\ArchSim.uproject" -waitmutex`(`UE_ENGINE_ROOT` 與 `<repo-root>` 由貢獻者本機設定;預設見 README 環境章節):

- **Result: Succeeded, 63.97 秒**
- 198 actions 全綠,4 個 plugin 各自 module .dll link 成功(ALS / ALSCamera / ALSExtras / ALSEditor / PrefabricatorRuntime / PrefabricatorEditor / PrefabricatorEditorPostInit / ConstructionSystemRuntime / ConstructionSystemEditor / SPUD / SPUDEditor / SPUDTest / SUQS / SUQSTest)
- `DefaultEngine.ini` 改動自動 trigger makefile invalidation(`Invalidating makefile for ArchSimEditor (DefaultEngine.ini modified)`,預期行為)
- **Warning 1(非阻擋)**:`Visual Studio 2026 compiler version 14.51.36247 is not a preferred version. Please use the latest preferred version 14.44.35207` — 過往 FrameCore build 都用同 toolchain,無實際問題
- **Warning 2(已記 Decision Log)**:`Plugin 'SPUD' depends on plugin 'StructUtils' which was deprecated in 5.5 and will soon be removed`
- 其他 C4996 warnings(`UTexture::GetAssetRegistryTags` / `FParticleEmitterInstance::GatherMaterialRelevance` 等)是 UE5 內部 source 的 next-release deprecation,Adaptive Build 把這些 Engine header 拉進 plugin TU 才出現,非新加 plugin 引起

**5-leg gate** — `Scripts/run_gate.ps1 -RequireOpenSees`:

```
[1/5] standalone FrameCore gate (build.bat)...
       standalone: ALL PASS  (failures=0) (exit 0)
[2/5] UE headless automation...
       UE automation: 135 tests run, exit code 0 (process exit 0; expected >= 135)
[3/5] OpenSees offline cross-validation...
       OpenSees compare: PASS (exit 0)
[4/5] linear-analysis deep audit...
       linear deep audit: PASS failures=0 checks=104 (exit 0)
[5/5] CLI round-trip (frame_cli J1 bridge)...
       CLI round-trip: ALL PASS  (failures=0) (exit 0)
======================================================
 GATE: PASS  (standalone OK, UE 135 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK)
```

**沒退化**:135 UE tests = v4.0.0 baseline 一致;新加 4 個 plugin 沒讓 UE automation runner 漏抓任何測試,沒有 plugin load 失敗訊號(UE automation 啟動 = UE Editor headless 完整 plugin pipeline 跑通)。

### Spike 2:FDemandSummary USTRUCT 欄位查閱

**確認:`FFrameDemandSummary` 完整結構**(`Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameCoreUEResultTypes.h:178-196`):

```cpp
USTRUCT(BlueprintType)
struct FFrameDemandSummary {
    float MaxDC;                  // 不是 MaxMemberDC
    float SafetyFactor;           // ✅ 真實存在
    int32 GoverningMemberIdx;     // -1 sentinel
    int32 GoverningMemberId;
    EFrameFailMode Mode;
    bool bValid;
    float MaxShellDC;
    int32 GoverningShellIdx;      // -1 sentinel
    int32 GoverningShellId;
    bool bShellValid;
};
```

**影響**:D4 安全係數的 `UArchSimSafetyFactorHelper` 可以直接讀 `Utilization.SafetyFactor`,不必自算 `SF = 1.0/MaxDC`(雖然自算也行)。計畫書修正完成。

### Spike 3:UE5.7 PCG 節點名稱

**確認(Surface Sampler 正常)**:`UPCGSurfaceSamplerSettings::GetDefaultNodeTitle()` 回 `"Surface Sampler"`(`PCGSurfaceSampler.h:117`),節點名跟主檔/計畫書一致,**無需修改**。

**發現 1**:UE5.7 PCG 無獨立「Perlin Noise」節點

- 真實節點:`Spatial Noise`(`PCGSpatialNoise.h:69` `GetDefaultNodeTitle()`;`L68 GetDefaultNodeName()` 同名)
- 主 enum `PCGSpatialNoiseMode`(`PCGSpatialNoise.h:10-23`,**全部 2D 變體,UE5.7 無 3D Spatial Noise**):
  - `Perlin2D` — classic Perlin(節點預設值,`L85`)
  - `Caustic2D` — 水下焦散風格,swirly
  - `Voronoi2D` — Voronoi cell + edge distance
  - `FractionalBrownian2D` — fBM 分形布朗運動
  - `EdgeMask2D` — 邊緣 blend 用 mask
- 子 enum `PCGSpatialNoiseMask2DMode`(`L25-34`,**僅在 `Mode = EdgeMask2D` 時生效**):`Perlin` / `Caustic` / `FractionalBrownian`
- **建築模擬器要的 classic Perlin** → 設 `Mode = PCGSpatialNoiseMode::Perlin2D`(節點預設值就是這個,通常 BP/PCG Graph 不需 override)
- ⚠️ **不要混用兩個 enum**:`Perlin`(不帶 2D 後綴)是 `PCGSpatialNoiseMask2DMode` 內的值,在 `Perlin2D` mode 不生效

**發現 2**:UE5.7 PCG 無「Attribute By Slope」節點

- 完整搜過:`Engine/Plugins/PCG` / `PCGInterops` / `Experimental/PCGBiomeCore` / `Experimental/PCGBiomeSample`
- 沒有任何 Slope / SlopeFilter / SlopeMask / NormalDot 字眼

**替代方案**(主檔 Part G1 已列出 4 選項):
- (a) Custom PCG Node 寫 `UPCGSlopeFilterSettings`(~8h)
- (b) GeometryScript 算 face normal · Z 軸 → 篩選(~5h)
- (c) **Landscape Spline 預先標**(~3h,**MVP 建議**)
- (d) 取消 slope filter(0h,但學生體驗差)

**決策**:**MVP 走 (c) Landscape Spline 老師手動標**;Phase 2 升 (a) Custom PCG Node。

### Gate 0 關閉條件

- [ ] UE Editor 開啟 ArchSim.uproject,Output Log 無 "Plugin X failed to load"
- [ ] 四個 plugin(ALS-Refactored / Prefabricator / SPUD / SUQS)出現在 Edit → Plugins 面板狀態 Enabled — **需使用者人工視覺確認**
- [x] 5-leg gate 全綠(standalone F1..F71 / UE 135 tests / OpenSees / audit 104 / CLI roundtrip)— 2026-06-25 agent 跑完,GATE: PASS
- [x] 三個 Spike 結論寫入本檔
- [x] SPUD UE5.5 deprecated warning 寫入主檔 Part L3

---

## Sprint S-01(W3-4,Gate 1)— A 層完整骨架

**狀態**:🟡 部分完成(A1-01..A1-06 落於 v0.1; A1-07 落於 v0.1.1)
**起始日期**:2026-06-25(同 S-00 收尾日)
**負責**:agent

### Landed in v0.1(commit `389ebfb`)

A1-01 .. A1-06:`UArchSimMemberData` ActorComponent + `UArchSimModelRegistry`
GameInstanceSubsystem。`Member.Id == MemberIdx` 契約落在 `ArchSimModelRegistry.cpp:178`
(`RegisterMember`)+ `:389`(`DeactivateMember`);常數 `kNodeMergeTolMm=1.0` /
`kVerticalAxisDot=0.999` / `kCmToMm=10.0` / `MaxRank=96`(debounce 上限)。

### Landed in v0.1.1(本 release)

**A1-07 SaveLoadRoundTrip**:`Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp`
新建。21+ sub-assertion 覆蓋:5 個 fake actor in-order 註冊 / 10 個 unique node /
member id 序保留 / `RegisteredCount <= MaxRank=96` / 三個 `UPROPERTY(SaveGame)` 欄位
(`MemberIdx` / `StructureGroupId` / `CachedUtilization`)經 `-1` scribble 後可由
buffer 還原 / `Member.Id == MemberIdx` 契約 roundtrip 前後皆守。

| 子議題 | 處置 |
|---|---|
| SPUD orchestration | **stub**:headless `-nullrhi -unattended` 不適合驅動 `USpudSubsystem`(需活的 World+Level+GameInstance + 磁碟 artefact)。改用 UE 官方 `FObjectAndNameAsStringProxyArchive` + `ArIsSaveGame=true`(SPUD 內部即用此 reflection 序列化路徑)。UPROPERTY(SaveGame) 真覆蓋 |
| `UArchSimModelRegistry` 無 Outer 警告 | cosmetic;與既有 `FrameCoreUEInteractiveSubsystemTest` 同 fallback pattern |
| 首跑 `nodeCount expected 10, got 2` | 真 bug:base `AActor` 無 `RootComponent`,`SpawnActor(...,Location,...)` 的 Location 被吞 → 5 actor transform 全 identity → 端點全 merge。修法:手動 `NewObject<USceneComponent>` + `SetRootComponent` + `SetActorLocation`(test 留 inline comment 給後人)|
| `§` (U+00A7) MSVC escape 超範圍 | 改 ASCII `"section 4"`;test 內 UE_LOG 訊息全 ASCII |

**Test 執行結果**(`Saved/Logs/ArchSim.log` 2026-06-25T07:37:49):
```
Test Completed. Result={成功} Name={SaveLoadRoundTrip}
                Path={ArchSim.Persistence.SaveLoadRoundTrip}
**** TEST COMPLETE. EXIT CODE: 0 ****
```

**Coverage gap(deferred 進 v0.2)**:`Scripts/run_gate.ps1` line 70 的
`ExecCmds = 'Automation RunTests FrameCore; Quit'` 只跑 `FrameCore.*` namespace。
`ArchSim.Persistence.SaveLoadRoundTrip` 在 `ArchSim.*` namespace,**目前不被 5-leg
gate 收**。需在 S-02 加 `ArchSim.*` filter(`HANDOFF_v0.1.1.md §4 item #1`)。

### 仍 deferred(進 S-02)

- A1-06 full integration test
- A2-01 ALS pawn 接入
- Gate 0 視覺確認(SPRINT_NOTES L141-142 兩 checkbox 仍未打勾)
- K1-T2 / K4 美術前置
- ArchSim 命名空間進 `run_gate.ps1`(本 release 新增 deferred)

---

## Decision Log(跨 Sprint)

| 日期 | 決策 | 理由 | 影響 |
|---|---|---|---|
| 2026-06-25 | G1 Slope Filter 走 Landscape Spline(MVP)+ Custom PCG Node(Phase 2) | UE5.7 PCG 無 Attribute By Slope,Landscape Spline 是最簡單 MVP 解 | G1-T1 工時不變;Phase 2 新增 ~8h Custom Node |
| 2026-06-25 | SPUD UE5.7 暫接受 StructUtils deprecated 警告 | MVP 不擋,SPUD 升級風險留 UE5.8+ 時驗證 | 鎖定 UE5.7 至 MVP 完成 |
| 2026-06-25 | A1-07 SPUD orchestration 走 SaveGame proxy archive stub(不驅動 `USpudSubsystem`) | headless automation 不適合驅動完整 SPUD;proxy archive 是 SPUD 內部用的同一條 reflection 路徑,UPROPERTY(SaveGame) 真覆蓋 | S-02+ 接 SPUD orchestration 時需確認 `UArchSimMemberData` 在生產路徑非 `RF_Transient`(否則 SPUD 反射掃描跳過)|

---

## Sprint S-02 (2026-06-26, v0.1.3 → v0.2.0) — game-body demo-ready foundation

**狀態**:✅ closed 2026-06-26
**起始日期**:2026-06-26 (same-day open and close, driven via `/work` 7-phase chain)
**負責**:agent (8 dispatch units) + integrator (release-hardening pass)

**Detailed per-unit history is in [`docs/logs/S-02/`](logs/S-02/)** —
this section is the high-level pointer only, per the SPRINT_NOTES.md policy
note at the top of this file.

### One-paragraph summary

Sprint S-02 shipped the user-visible playable foundation: `UArchSimGameInstance`
+ `FTickableGameObject` driver (AS-02), `AArchSimCharacter` + `AArchSimGameMode`
+ Enhanced Input wiring on top of ALS-Refactored v4.17 (AS-03), and the
deferred `PendingRankAccumulation` ceiling test from v0.1.3 (AS-10). 3 tags
shipped along the way (v0.1.4 / v0.1.5 / v0.2.0), gate count 137 → 140
(non-cuDSS 135 → 138). Engine source delta across the entire sprint = 0
lines under `Plugins/FrameSolver/Source/FrameCore/` (FROZEN under v4.0.0).
LevelSim delta = 0 (FROZEN v1).

### What landed (per tag)

- **v0.1.4** — `UArchSimGameInstance` skeleton (AS-02a) + real
  `RebaselineCeiling` test (AS-10) with 2 honest spec corrections
- **v0.1.5** — Tick driver loop (AS-02b) + headless smoke test (AS-02c)
- **v0.2.0** — `AArchSimCharacter` (AS-03a/b/c) + headless smoke
  (AS-03d), with a post-tag release-hardening pass that landed 2 small
  production fixes (DeactivateMember `bRegistered` reset; RegisterMember
  PendingPatch clear) + privacy/repro doc sanitisation

### Backlog opened during the sprint

- **AS-11** (LOW): header comment precision for `PendingRankAccumulation`
  reset sites
- **AS-12** (LOW): `GetMaxRankBeforeRebaseline()` production consumer
- **AS-13** (MEDIUM, load-bearing): PIE-world fixture for driver-loop
  observability + AS-10 trip-path + AS-03d input runtime — three deferred
  test branches resolve under one PIE harness
- **AS-14** (LOW): HandleMove `UAlsVector::ClampMagnitude012D` clamp for
  analog stick / gamepad

### Future backlog identified by release-hardening Phase 1

- **AS-15** (HIGH): Enhanced Input lifecycle refit via
  `NotifyControllerChanged` (+ `RemoveMappingContext`) — bundle of agents
  A-02 / D-01 / D-02 / D-03 / D-06 findings
- **AS-16** (HIGH): `CalcCamera` override for ALS camera component pipeline
  (D-08)
- **AS-17** (MEDIUM): empty-`CurrentModel` `StartSession` behavior audit
  (C-02)
- **AS-18** (LOW): document the two-GameInstanceSubsystem teardown order
  dependency (C-04)
- **AS-19** (LOW): retry/log when `MemberData::BeginPlay` runs before
  `GameInstance` is ready (C-06)

### Decisions log additions

| 日期 | 決策 | 理由 | 影響 |
|---|---|---|---|
| 2026-06-26 | Sprint logs split:`docs/SPRINT_NOTES.md` keeps a 1-paragraph high-level pointer per sprint;`docs/logs/S-XX/` carries the per-unit dispatch + review + commit detail | The /work 7-phase chain produces far more per-unit detail than SPRINT_NOTES is meant to hold;readers needing detail should follow the link | Future sprints (S-03+) keep the same split |
| 2026-06-26 | Tick driver "dirty" definition narrowed:registered-count delta only, NOT actor position movement | Demo MVP places static buildings;position-sync is a future feature when dynamic buildings ship | AS-02b shipped with this scope;position-sync defers to a future AS-XX |
| 2026-06-26 | Sanitize convention for sprint logs:reviewer agent IDs replaced with `[sanitized]` placeholder before publish | Phase 1 G audit caught the leak;v2.x release-hardening discipline applies to v0.x as well | `docs/logs/S-XX/agent_*.md` template should drop the ID column going forward |

---

## Sprint S-03 (2026-06-26) — game-body robustness + deferred AS-XX queue

**狀態**: 🟡 進行中
**起始日期**: 2026-06-26
**負責**: agent (parallel dispatch rounds)

### AS-17 audit conclusion (2026-06-26, Unit 2)

**Finding: Case A — production-safe; no guard needed.**

**Source-traced reasoning:**

1. `FromBlueprint(EmptyDef, Cached->Model, OutError)` (FrameCoreUEModelMarshal.cpp L168-251):
   for a fully empty `FFrameModelDef` (all TArrays empty) ALL marshal loops are no-ops and the
   function falls through to `return true`. So `FromBlueprint` passes even for empty input.

2. `new frame::ReSolveSession(Cached->Model, eopts)` is called with a 0-nodes / 0-members engine
   model. Engine ctor does NOT throw — it stores the result of `FrameModel::validate()` internally
   and exposes it via `valid()`.

3. `FrameModel::validate()` (FrameModel.cpp L31): `if (nodes.empty()) { why = "no nodes"; return false; }`
   — empty model fails validation immediately. Therefore `Session->valid()` == `false`.

4. The existing guard at `FrameInteractiveSubsystem.cpp:81-88` catches this:
   ```cpp
   if (!Session->valid())
   {
       OutError = FString(UTF8_TO_TCHAR(Session->diagnostic().c_str())); // "no nodes"
       delete Session; Session = nullptr;
       delete Cached;  Cached  = nullptr;
       return false;
   }
   ```
   Session and Cached are both cleaned up; `IsSessionActive()` returns false. No dirty state.

5. The consumer (`ArchSimModelRegistry::FlushAndStartSession`, cpp:236-240) checks `!Sub->StartSession(...)`
   and logs a Warning + returns false. Consumer side also safe.

**Test oracle (pinned by** `FrameCore.UE.EmptyModelStartSession` **— NEW CODE):**

8 sub-checks across 4 scenarios:
- **Fully empty model**: `StartSession` returns `false`; `OutError` non-empty; `IsSessionActive() == false`
- **Idempotent cleanup**: double `EndSession()` after failed start does not crash
- **Partial empty (mat+sec, 0 nodes)**: also gracefully fails ("no nodes" from validate)
- **Recovery**: subsequent valid cantilever `StartSession` succeeds (failed start does NOT leave dirty state)

**Test result (2026-06-26T17:31):**
```
Test Completed. Result={成功} Name={EmptyModelStartSession} Path={FrameCore.UE.EmptyModelStartSession}
**** TEST COMPLETE. EXIT CODE: 0 ****
```

**Files changed (AS-17-u1):**
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp`
  — +1 `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEEmptyModelStartSessionTest, ...)` (8 sub-checks)
- `Scripts/run_gate.ps1` — `$ExpectedUeTests` 140 → 141 (non-cuDSS 138 → 139)

---

*── 持續更新 ──*
