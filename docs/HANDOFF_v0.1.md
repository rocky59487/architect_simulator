# 交接指南 — `v0.1` 後接手 owner

> `v0.1` 在 2026-06-25 發布。Tag `v0.1` 指向 v4.0.0 的 commit + 一筆 release commit
> (game body 首次 tag,沒有前一個 game-body tag 可 diff)。
> 主交接文 `docs/HANDOFF.md`(原始 v3.x 系列, 不動);引擎系列前一輪 `docs/HANDOFF_v4.0.0.md`
> 也保留。本檔只補上 `v0.1` 多出來的 game-body 內容。

---

## 1. `v0.1` = 什麼

- **Elevator pitch**:第一個 UE5 game body 釋出 — 在 FROZEN 的 FrameCore v4.0.0 + LevelSim v1
  之上加 4 個 MIT plugin clone + Source/ArchSim/ 模組 + 完整 architect-simulator 設計
  corpus(master plan / implementation plan / sprint log)。
- **與 v4.0.0 的 source delta**:
  - FrameCore engine source = **0 行**(鐵則 #1 honoured)
  - LevelSim = **0 行**(鐵則 #5 honoured)
  - ArchSim.uproject = **0 行**(鐵則 #5 honoured)
  - 新增 Source/ArchSim/ 4 個 .h/.cpp + 1 個 Build.cs 修改
  - 新增 docs/ 3 個 .md(MASTER_PLAN, IMPLEMENTATION_PLAN, SPRINT_NOTES)+ 2 個 v0.1 release artifacts
  - 修改 Config/DefaultEngine.ini(Enhanced Input default classes,ALS 強制)
  - 修改 Plugins/FrameSolver/Grasshopper/v2/* 6 個檔案(trivial build-env fixes,chore: piggybacked)
- **整入了哪些先前 deferred items**:N/A(首次 game-body release,沒前一輪 deferred)
- **什麼未動(點名清楚)**:
  - FrameCore engine source code 完全未動
  - LevelSim 任何檔案完全未動(連 build script 都沒)
  - FrameCoreUE consumer-side API 未動
  - .uproject 未動
  - .gitignore 未動(鐵則 #5;見 §5 lesson 2)

---

## 2. 怎麼跑

### 一鍵驗證 release

```powershell
cd <repo-root>

# Restore 4 plugin clones (NOT committed; see RELEASE_v0.1.md §4)
git clone --branch 4.17 https://github.com/Sixze/ALS-Refactored Plugins/ALS
git clone --depth 1 https://github.com/unknownworlds/prefabricator-ue5 Plugins/Prefabricator
git clone --depth 1 https://github.com/sinbad/SPUD                       Plugins/SPUD
git clone --depth 1 https://github.com/sinbad/SUQS                       Plugins/SUQS

# Patch Prefabricator/SPUD/SUQS .uplugin to add "EngineVersion": "5.7.0"
# (ALS already has 5.7.0.) Exact patches in docs/SPRINT_NOTES.md Spike 1.

# Build
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# 5-leg gate
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```

Expected: gate PASS, build PASS,UE Editor 開啟見 4 plugins enabled。

### Env prerequisites

| Var | 用途 | 預設 / 範例 |
|---|---|---|
| `UE_ENGINE_ROOT` | UE 5.7 安裝根 | (依本機而定;見 README §Environment) |
| `SUPERNODAL_CONDA` | conda env (含 OpenBLAS + METIS + cuDSS) | 預設指向 `framecore-direct` env |
| `FC_NO_SUPERNODAL` | 跳過 supernodal opt-in lane | unset = 啟用 |

---

## 3. 新加的東西 (給下次接手的人快查)

### 3.1 新 Source/ArchSim/ 模組

| 檔案 | 功能 |
|---|---|
| `Public/Components/ArchSimMemberData.h` | ActorComponent — 標記一個 Actor 為結構成員;持 `MemberIdx`、`MaterialId`、`SectionId`、`EndIOffsetUE`、`EndJOffsetUE`、`CachedUtilization` UPROPERTY(SaveGame) |
| `Public/Subsystems/ArchSimModelRegistry.h` | GameInstanceSubsystem — 持有 `FFrameModelDef` aggregate;debounce 150ms 後呼叫 `UFrameInteractiveSubsystem::ApplyPatchAndResolve`;結果分發到 Component;`MaxRank=96` 觸發 Rebaseline |
| `Private/Components/ArchSimMemberData.cpp` | BeginPlay → Registry.RegisterMember;EndPlay → Registry.DeactivateMember |
| `Private/Subsystems/ArchSimModelRegistry.cpp` | FindOrAddNode (1mm tolerance, O(N) scan, MVP 500 member cap)、PickRefVecForAxis (vertical column +X fallback)、debounce/rebaseline state machine、Singular-safe DistributeSolveResult |

**Calling convention 重點**(實作中已正確 honour,寫在 code 註解):
- `kCmToMm = 10.0`:UE cm → FrameCore mm
- `Member.Id == MemberIdx`:internal index 與 user id 一致,讓 FrameCore patch API 跟 Registry reverse-lookup 不歧義
- `FFrameMemberUtilization.Peak.Risk` 是 D/C ratio(不是 `[i].DC`,marshal struct 命名落差)
- `FFrameDemandSummary.MaxDC` + `SafetyFactor` 都存在(S-00 Spike 2 確認)
- `UFrameInteractiveSubsystem::StartSession` 是 4-param `(Def, Opts, ReOpts, OutError)`

### 3.2 PCG slope filter 設計決策

UE5.7 PCG **無 `Attribute By Slope` 節點**(S-00 Spike 3 確認)。Slope filter 4 個替代方案:
- (a) Custom PCG Node `UPCGSlopeFilterSettings` — ~8h,Phase 2
- (b) GeometryScript face normal · Z 軸 — ~5h
- (c) **Landscape Spline 老師手動標** — ~3h,**MVP 決策**
- (d) 取消 slope filter — 0h,學生體驗差

MVP 走 (c);Phase 2 升級 (a)。決策已記入 SPRINT_NOTES Decision Log。

### 3.3 PCG Spatial Noise 節點

UE5.7 PCG **無獨立 `Perlin Noise` 節點**(S-00 Spike 3 確認)。改用 `Spatial Noise`
節點 `PCGSpatialNoiseMode::Perlin2D`(`PCGSpatialNoise.h:69`)。

---

## 4. Deferred items — Day-1 first actions

每項都對應 [`RELEASE_v0.1.md`](RELEASE_v0.1.md) §7 deferred table。**First action on day 1** 是
接手者隔日開 repo 第一個動作 — 不是「想想看」,而是具體檔案路徑 + 命令。

1. **A1-06 full `DeactivateMember`** (2h)
   - First action:Edit `Source/ArchSim/Private/Subsystems/ArchSimModelRegistry.cpp:377-394`
     — 把 stub `DeactivateMember` 擴成完整 sweep:從 `IndexToComponent` 刪掉 stale weak ptr,
     `FFrameModelPatch::ReactivateMemberIds` 在 member 重新放置時重用 idx;新增 `ReactivateMember(MemberIdx)` 公開 API。

2. **A1-07 SaveLoadRoundTrip UE automation test** (3h)
   - First action:新建 `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/ArchSimSaveLoadRoundTripTest.cpp`,
     用 `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreArchSimSaveLoadRoundTrip,
     "FrameCore.ArchSim.SaveLoadRoundTrip", ...)`;build 10 個 dummy `UArchSimMemberData`
     Actor → `USpudSubsystem` save → reload → assert MemberIdx 全一致;
     `Scripts\run_gate.ps1 $ExpectedUeTests 135 → 136`。

3. **A2-01 `AArchSimCharacter` ALS subclass** (4h)
   - First action:新建 `Source/ArchSim/Public/Characters/ArchSimCharacter.h`
     繼承 `AAlsCharacter`(來自 `Plugins/ALS/`);實作 Enhanced Input
     `MC_Locomotion` + `MC_Building` MappingContext;在 PIE 中驗證 WASD 走動 + E 鍵切換
     建築模式;先不接 Server RPC。

4. **Gate 0 UE Editor → Plugins 視覺確認**(human-action only)
   - First action:打開 UE Editor → 開啟 `ArchSim.uproject` → Edit → Plugins;
     確認 ALS / Prefabricator / SPUD / SUQS 4 個 plugin 顯示 Enabled;
     在 SPRINT_NOTES.md Gate 0 checklist 第 142 行打勾。

5. **K1-T2 / K4-T1 / K4-T2 美術前置**(parallel,Asset/BP work)
   - First action:UE Editor 中建 `Content/UI/Fonts/`,匯入 Noto Sans CJK TC OFL 字體;
     建 `Content/Materials/MI_Member_Default.uasset` 三件套(steel / concrete / wood
     material instance);建 `Content/UI/Styles/DA_UIStyle.uasset` DataAsset(顏色 +
     字級規範)。這些 B1 構件放置前要就緒。

6. **SPUD UE5.5 StructUtils deprecation** (pre-UE5.8 upgrade)
   - First action:**don't upgrade UE 5.7 → 5.8 without first verifying SPUD has
     migrated off `StructUtils`**;在 GitHub SPUD repo 查最新 release notes;
     若未修則 fork + patch 或選替代 plugin。

---

## 5. 過程留下的教訓(durable for the project's CLAUDE.md memory)

1. **`Glob **/Plugins/**` 在寫 plan 前必跑** — 我寫 master plan 時把 LevelSim 當「另一個獨立子專案」
   忽略,實際上它已經 bundled 在 `Plugins/LevelSim/` 內(2026-06-18 PR#8 merged)。Part G 工時被高估 53h。
   下次寫 plan 前先 `Glob`,別只信 memory 描述。

2. **鐵則 #5「絕不碰 .gitignore」對 game body 過嚴** — v0.1 加入 4 個 plugin clone 跟 .claude/
   對話痕跡,有正當理由加 ignore pattern,但鐵則 #5 阻止。本次 release 暫不改 .gitignore,改在
   commit 前手動 git status discipline。**下次接手 owner 可考慮 amend 鐵則 #5 加 game-body
   例外**(這需要 CLAUDE.md formal amendment + user 授權)。

3. **subagent expansion 漏掉 LevelSim 訊號的根因**:subagent prompt 沒指示先掃 Plugins/。
   下次類似 workflow 寫 prompt 時加 "before answering Part X, run `Glob **/Plugins/**`
   and report what exists" 強制 verify。

4. **`MaxMemberDC` 是 prompt 編造的欄位名**,實際 FFrameDemandSummary 是 `MaxDC`。
   下次寫 calling convention 時 verify 對應 header,不要拍腦袋取名。

5. **UE5.7 PCG 節點名跟前一版差很多** — 「Perlin Noise」獨立節點不存在(改用 `Spatial Noise`),
   「Attribute By Slope」直接不存在。寫 plan 引用任何 PCG 節點前必須 grep `Engine/Plugins/PCG/*.h`
   確認 `GetDefaultNodeTitle()`。

6. **SPUD 隱性依賴 deprecated plugin** — UE5.5 deprecated `StructUtils`,SPUD 仍依賴。
   UE5.8 升級會炸。任何第三方 plugin 加進來時,先檢查它的 `.uplugin` dependency block。

7. **Grasshopper bridge 在 working tree 殘留 build-env modifications** 跨多個 release
   仍未 commit — 顯示 release-hardening 流程對「次要 plugin 的次要修正」沒兜住。建議下次
   pre-tag 前 `git status` 抓所有 modified file,即使是 chore: 也 commit 進 release。

---

## 6. 後續方向

### Sprint S-02 (W3-4) — 接續 A1 + 啟動 A2 (115h)

立即接續 deferred items #1 (A1-06 full) + #2 (A1-07 SaveLoadRoundTrip) + #3 (A2-01
ALS Character)。
SPRINT_NOTES Sprint S-01 段已有「淨工時消耗 14h / 預算 115h」,S-02 有 101h 餘裕可吃進
這 3 個 items + 美術前置 (#5)。

### Phase 1 MVP 路徑(主檔 Part L1)

S-00 ~ S-08 共 18 週(已 1/9 完成);Sprint S-08 = MVP Release 教師評估版可玩。

### 風險區

- **UE5.7 鎖定**:不升 5.8 直到 SPUD `StructUtils` 替代方案就緒
- **Source/ArchSim/ 自動化覆蓋**:S-02 必加 A1-07 test 才算 stable
- **Listen Server 低配機效能**:S-09 前在實際教學電腦壓測(計畫書 Part L3 風險清單)

---

接手有問題:
- 主交接:[`docs/HANDOFF.md`](HANDOFF.md)
- 引擎前一輪:[`docs/HANDOFF_v4.0.0.md`](HANDOFF_v4.0.0.md)
- 本檔(game body 首次):本文件
- Sprint S-00 spike 細節:[`docs/SPRINT_NOTES.md`](SPRINT_NOTES.md)
- 設計層權威:[`docs/ARCHITECT_SIM_MASTER_PLAN.md`](ARCHITECT_SIM_MASTER_PLAN.md)
- 實作層:[`docs/IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md)

---

*HANDOFF prepared by release-hardening skill Phase 4. The "First action on day 1"
discipline ensures every deferred item is actionable, not abstract.*
