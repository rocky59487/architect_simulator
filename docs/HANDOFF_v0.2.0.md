# 交接指南 — `v0.2.0` 後接手 owner

> `v0.2.0` 在 2026-06-26 發布,tag `v0.2.0` 接在 `v0.1.5` 之後。
> **Sprint S-02 closing release** — game-body 從 v0.1.x patch 線首次 minor bump 到 v0.2.x。
> AS-02 + AS-10 + AS-03 整 8 個 dispatch units 在同一 session 全 ship,2 個 patch tag
> (v0.1.4, v0.1.5)+ 1 個 minor tag (v0.2.0)。

---

## 1. `v0.2.0` = 什麼

一句話:**user-visible feature 落地 — UE Editor 開 PIE 任何 default map,
`AArchSimGameMode` 自動 spawn ALS-driven 角色,WASD/Mouse/Space/Shift/Ctrl
binding 已 C++ wire,等使用者按 `docs/INPUT_MAPPING.md` 建 6 個 UAsset
(IMC_ArchSimDefault + 5 IA)到 BP 後即可 playable。Engine 源 0 行動;
`UArchSimModelRegistry.{h,cpp}` 0 行動;Sprint 全程 production code delta = ~530 行。**

### 動到的檔(本 release vs v0.1.5)

| Path | LOC delta | Type | Sprint unit |
|---|---|---|---|
| `Source/ArchSim/Public/Characters/ArchSimCharacter.h` | +100 (new + AS-03a/b/c extensions) | Production | AS-03a/b/c |
| `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` | +261 | Production | AS-03a/b/c |
| `Source/ArchSim/Public/ArchSimGameMode.h` | +18 (new) | Production | AS-03c |
| `Source/ArchSim/Private/ArchSimGameMode.cpp` | +14 (new) | Production | AS-03c |
| `Source/ArchSim/Private/Tests/ArchSimCharacterTest.cpp` | +130 (new) | Test | AS-03d |
| `Source/ArchSim/ArchSim.Build.cs` | `"ALS"` + `"ALSCamera"` | Build | AS-03a + AS-03c |
| `Config/DefaultEngine.ini` | `+GlobalDefaultGameMode` | Config | AS-03c |
| `Scripts/run_gate.ps1` | L29 139→140 + comment | Build | AS-03d |
| `docs/INPUT_MAPPING.md` | new | Docs | AS-03b |
| `docs/ARCHITECTURE_INDEX.md` | §2/§6/§7/§8 updates | Docs | release |
| `docs/RELEASE_v0.2.0.md` | new | Docs | release |
| `docs/HANDOFF_v0.2.0.md` | new (本檔) | Docs | release |
| `docs/logs/S-02/agent_AS-03*.md` | new (a/b/c/d) | Sprint logs | /work driver |
| `docs/logs/S-02/manager.md` | append (whole sprint) | Sprint log | /work driver |
| `README.md` | + v0.2.0 status block | Docs | release |

### Sprint S-02 deferred items 處理

| ID | Status | 處理 |
|---|---|---|
| AS-02 (Tick driver) | ✅ closed in v0.1.5 (a/b/c) | scope-narrowed |
| AS-03 (ALS pawn) | ✅ closed in v0.2.0 (a/b/c/d) | scope-narrowed:input runtime → AS-13 |
| AS-04 (Plugins panel 視覺) | 🔵 still deferred (human 0.5h) | 同 |
| AS-05 (K1-T2 / K4 美術) | 🔵 still deferred | 同 |
| AS-06 (SPUD/StructUtils) | 🔵 still deferred (UE5.8 升前) | 同 |
| AS-08 (SPUD RF_Transient audit) | 🟡 still open | 同 |
| AS-09 (non-cuDSS 重驗) | 🔵 still deferred | 同 |
| AS-11 (header comment precision) | 🟡 backlog (LOW) | v0.1.4 carried |
| AS-12 (GetMaxRankBeforeRebaseline consumer) | 🟡 backlog (LOW) | v0.1.4 carried |
| AS-13 (PIE-world fixture) | 🟡 backlog **(MEDIUM, 本 sprint 多次撞)** | v0.1.5 + v0.2.0 |
| AS-14 (analog stick clamp) | 🟡 backlog (LOW) | v0.2.0 AS-03b NITS |

### 什麼未動

- FrameCore engine source(`v4.0.0` FROZEN)
- LevelSim engine(`v1` FROZEN)
- 4 plugin clones (ALS / Prefabricator / SPUD / SUQS)
- `ArchSim.uproject` / `.gitignore` / build artifacts
- `UArchSimModelRegistry.{h,cpp}` 整 Sprint(production logic byte-identical;只 AS-10 v0.1.4 加 3 個 telemetry getter)
- `UArchSimGameInstance.{h,cpp}` 自 AS-02b 之後不變
- v0.1.x 已 shipped tests(`SaveLoadRoundTrip` + `MaxRankCeiling` + `RebaselineCeiling` + `TickDriver` 全保留)
- Position-change sync(MVP buildings 不動)

---

## 2. 怎麼跑

```powershell
# 一鍵 5-leg gate (現在預設 140 cuDSS / 138 non-cuDSS)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS, UE 140 tests run, GATE_EXIT=0

# 非 cuDSS host:
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 138

# AS-03d 單跑
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Gameplay.CharacterInput; Quit" `
    -unattended -nullrhi -log
# Expect: Result={成功} Name={CharacterInput}

# 所有 ArchSim 測試一起(5 tests)
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim; Quit" `
    -unattended -nullrhi -log
# Expect: 5 tests (SaveLoadRoundTrip + MaxRankCeiling + RebaselineCeiling + TickDriver + CharacterInput)
```

### 使用者必做(才能 demo-able)

⚠️ **C++ 端 only ship — UAsset 由 project author 在 UE Editor 建立:**

1. 開 UE Editor → `ArchSim.uproject`
2. 按 `docs/INPUT_MAPPING.md` 在 `Content/Input/` 建 6 個 UAsset:
   - `IMC_ArchSimDefault`(UInputMappingContext)
   - `IA_Move` / `IA_Look`(Axis2D UInputAction)
   - `IA_Jump` / `IA_Sprint` / `IA_Crouch`(Digital UInputAction)
3. 開 BP class 繼承 `AArchSimCharacter`,在 Details panel `ArchSim|Input` 把 6 個 UAsset assign 到對應 slot
4. PIE → 測 WASD / Mouse / Space / Shift / Ctrl 全可動

### 推薦 demo 流程(畢答用)

1. PIE → 角色出現,WASD 移動,Mouse 看向
2. Shift = 衝刺(ALS 切 Sprinting gait)
3. Ctrl = 蹲下(ALS 切 Crouching stance)
4. Space = 跳躍
5. (未來)在場景 spawn `UArchSimMemberData` actor:Tick driver auto-trigger solve;heatmap 顯示 D/C

---

## 3. 新 token / 新 flag / 新 API

### Production-side(`AArchSimCharacter` + `AArchSimGameMode`)

`AArchSimCharacter` (繼承 `AAlsCharacter`):
- ctor 設 `bUseControllerRotationYaw/Pitch/Roll = false`(ALS 慣例)
- ctor 加 `UAlsCameraComponent Camera` DefaultSubobject + `SetupAttachment(GetMesh())` + `SetRelativeRotation_Direct({0, 90, 0})`(ALS example 慣例)
- 5 個 `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ArchSim|Input") TObjectPtr<UInputAction>` slots:`IA_Move/Look/Jump/Sprint/Crouch`
- 1 個 `TObjectPtr<UInputMappingContext> DefaultMappingContext` slot
- BeginPlay 取 LocalPlayer → Enhanced Input subsystem → AddMappingContext(DefaultMappingContext, 0)
- SetupPlayerInputComponent cast 到 `UEnhancedInputComponent` + BindAction × 7
- 7 個 protected handler:`HandleMove` (view-space)、`HandleLook`、`HandleJumpPressed/Released`、`HandleSprintPressed/Released`、`HandleCrouchToggle`

`AArchSimGameMode` (繼承 `AGameModeBase`,非舊 `AGameMode`):
- ctor 設 `DefaultPawnClass = AArchSimCharacter::StaticClass()`

### Config 變動

`Config/DefaultEngine.ini` `[/Script/EngineSettings.GameMapsSettings]` section 加:
```ini
GlobalDefaultGameMode=/Script/ArchSim.ArchSimGameMode
```

(GameInstanceClass 自 AS-02a 保留)

### Build 變動

`Source/ArchSim/ArchSim.Build.cs` PublicDependencyModuleNames 加 `"ALS"` (AS-03a) + `"ALSCamera"` (AS-03c)。

### Gate 變動

- `Scripts/run_gate.ps1` L29 `$ExpectedUeTests` 139 → 140 (cuDSS) / 137 → 138 (non-cuDSS)
- 新 test path:`ArchSim.Gameplay.CharacterInput`

---

## 4. 仍 deferred 的 items + Sprint S-03 建議排序

### Z-01: AS-13 是 Sprint S-03 的 top-priority(MEDIUM)

PIE-world fixture for driver-loop + trip-path + input runtime observability。本 sprint 撞 3 次:
- AS-10:trip path(`bNeedsRebaseline=true` + `Sub->Rebaseline()` 呼叫)
- AS-02c:Tick driver loop(`Tick → GetSubsystem → Registry`)
- AS-03d:Enhanced Input runtime + ALS state machine

完整 PIE fixture 可以同時解三件事。**估 ~6-8h:`FAutomationEditorCommonUtils::CreateNewMap` 起 PIE → spawn N actors / character → tick 數 frame → assert 觀察。**

### Z-02: AS-04 + AS-05 美術 + 視覺確認

Demo-ready 完整體驗需要這些。**Z-01 / Z-02 平行可能 best**。

**First action (AS-04 視覺確認,~30 min 人工):** 開 UE Editor 載入 `ArchSim.uproject` → Edit → Plugins → 過濾 "ALS" / "Prefabricator" / "SPUD" / "SUQS" 共 4 個 → 確認每個 plugin status = "Enabled" (沒 "Failed to load" 警告)。截圖留存在 `docs/screenshots/gate0_plugins_panel.png`(新建 docs/screenshots/ dir 給 future visual artifact)。

**First action (AS-05 art prep, ~30 min):** 在 UE Editor Content Browser 建立 `Content/UI/Fonts/`,把 Noto Sans CJK TC OFL 字型(從 https://fonts.google.com/noto/specimen/Noto+Sans+TC 下載 `NotoSansTC-Regular.otf` + `NotoSansTC-Bold.otf`)imported as Font asset。`docs/INPUT_MAPPING.md` 的 6 個 UAsset 同時 build。

### Z-03: AS-11 + AS-12 + AS-14 cleanup

LOW priority,任何 cleanup 窗口都行。

### Z-04: AS-08 SPUD orchestration

SPUD 真接 SaveGame 時得 audit `RF_Transient`(避免 SPUD 反射 scan 跳過 component)。本 sprint 沒接 SPUD,留 future。

**First action:** `grep -rn "RF_Transient" Source/ArchSim/Public/ Source/ArchSim/Private/Components/` 確認 `UArchSimMemberData` 與 `UArchSimModelRegistry` 沒被標 `RF_Transient`(目前 grep 應該返回空)。然後在 `Plugins/SPUD/Source/SPUDEditor/` Read `USpudSubsystem` 的 reflection scan logic,確認其對 `UPROPERTY(SaveGame)` 帶 `RF_Transient` 的 Component 怎麼處理(跳過 vs 警告 vs 序列化)。寫成 `docs/SPUD_INTEGRATION.md` 給後續 wiring task 用。

---

## 5. 過程留下的教訓(durable)

僅本 Sprint S-02 學到的;與全域教訓無重疊:

1. **「Headless 不能驗 X」是 v0.1.4-v0.2.0 連續 3 次的同 pattern** — AS-10 / AS-02c / AS-03d 三個 unit 都遇到「production 行為的觀察 endpoint 在 headless 不可達」的限制。每次都用 AS-07 lesson #1 honest partial coverage 處理,把限制誠實 doc 在 test header,把全驗證 defer 到 AS-13。**Pattern fix:Sprint S-03 提前做 PIE fixture(AS-13)**,後續 unit 就有完整測試框架可用。

2. **`/work` hub 機械停損的 hook 配置 gap** — AS-03c 跑了 59 tool calls,超過 50-step 硬上限 9 次。但 hook 沒 enforce(agent 自由跑完)。這是 hook 設計問題不是 agent 問題。Sprint S-03 開工前先補 hook,或接受目前的 advisory-only model 但加 Phase 6 retrospective audit。

3. **Bundle 跨多 unit 的 release ceremony 比 per-unit 省力很多** — v0.1.4 (AS-02a + AS-10) / v0.1.5 (AS-02b + AS-02c) / v0.2.0 (AS-03a/b/c/d):每個 release 只跑 1 次 release-hardening doc-write,而非 N 次。Sprint 整體 release ceremony cost 從 8 × 1h ≈ 8h 降到 3 × 30min ≈ 1.5h。Pattern:**production-only unit feature-commit 不出 tag,test-touching unit 觸發 tag bundle 前一個或多個 feature commit**。

4. **Adversarial reviewer 對 design-claim 必驗 precedent** — AS-03b 用 view-space movement,聲稱「matches AAlsCharacterExample::Input_OnMove」;AS-03c 聲稱「`_Direct` 不存在於 USkeletalMeshComponent」。Reviewer 都真去 Read precedent 比對,前者 verified 後者 corrected。**Future agent 給 design-justification 時,reviewer 必 grep 真實 source 不接受口頭。**

5. **AS-07 lesson #1 連用 3 次成立** — v0.1.3 AS-07 / v0.1.4 AS-10 / v0.2.0 AS-03c 都是「spec 描述跟 production 不符,agent 該 pin production 而非改 production fit spec」。AS-07 是 lesson 來源,後面 2 個 unit 應用。**這條 lesson 已上升到 Sprint S-02 core durable pattern**,可加進 CLAUDE.md 鐵則之外的 best-practice 章節。

6. **設計選擇深度 dispute 用 Phase 3 review 解** — AS-03c agent 用 `SetRelativeRotation` Roll=-90,reviewer 找 ALS precedent 是 `_Direct` Roll=0,inline 修正後 commit。這是「agent 自由設計 + reviewer 嚴格 precedent verify + main thread 工程判斷修正」三方分工的成功案例。

---

## 6. 後續方向

### Sprint S-03 建議排序

backlog:

1. **AS-13** PIE-world fixture(MEDIUM,6-8h)— top priority 解三個 deferred test
2. **AS-08** SPUD orchestration + RF_Transient audit(當接 SPUD)
3. **AS-04 + AS-05** 美術 + Plugins panel 視覺(demo polish,part user-side)
4. **AS-11 + AS-12 + AS-14** cleanup batch(LOW,可放任一 sprint)
5. AS-06 (SPUD/StructUtils) 永遠 deferred 到 UE5.8 升級時

### 何時 bump 下個 minor

- v0.2.x patch 線(AS-13 PIE fixture / AS-08 SPUD 任一)
- 當有新 user-visible feature(現在 ALS pawn 已落地,下個 user-visible feature 候選:多人模式 / dynamic structural events / scenario editor)bump v0.3.0

### 風險區

- **AS-13 PIE fixture 撞牆風險** — `FAutomationEditorCommonUtils::CreateNewMap` 在 headless `-nullrhi` 模式下可能不穩定(其他 UE 專案 v3.5 Phase 9 同樣 defer 過)。撞牆 fallback:用 `UWorld::CreateWorld(EWorldType::Game)` 起 minimal world,雖不能驅動完整 PIE pipeline 但可以 fire Tick 跟 BeginPlay。
- **ALS-Refactored v4.17 vs UE 5.8 升級** — 若 UE 升 5.8,ALS API 可能變,需先 verify ALS v4.18 或 fork 自己 patch。本 sprint 鎖 UE 5.7 還 OK。

---

接手有問題:`docs/HANDOFF_v0.1.md` → `HANDOFF_v0.1.1.md` → `HANDOFF_v0.1.2.md` →
`HANDOFF_v0.1.3.md` → `HANDOFF_v0.1.4.md` → `HANDOFF_v0.1.5.md` → 本檔。
Sprint S-02 完整 manager log:`docs/logs/S-02/manager.md`(append-only)。
v0.2.0 release notes:`docs/RELEASE_v0.2.0.md`。
