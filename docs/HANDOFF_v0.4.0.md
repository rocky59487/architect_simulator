# 交接指南 — `v0.4.0` 後接手 owner

> `v0.4.0` 在 2026-06-27 發布,tag `v0.4.0` 接在 `v0.3.1` 之後。
> **S-05 close** — Sprint 5 first **minor** bump on the game-body line since v0.2.0。
> Scenario MVP playable (u1+u2+u3) + Path B AS-25/26/27 cleanup + UE5.8 NO-GO eval。
> Engine FROZEN 全程 0 行。
> 主交接文鏈:`HANDOFF.md` → `HANDOFF_v0.1.md` → … → `HANDOFF_v0.3.1.md` → 本檔。

---

## 1. `v0.4.0` = 什麼

一句話:**首個包含 Scenario MVP 真實 code path 的 minor bump — `UArchSimScenarioWidget` Editor Utility Widget 提供 K1/K2/K4 placement + Tutorial state machine + Registry→Solve→Heatmap 完整 wire;Path B(AS-25/26/27)cleanup 收尾;UE5.8 install eval = NO-GO 但 decision doc + sandbox 為 S-06 升級準備。Engine source FROZEN 全程 0 行(FrameCore v4.0.0 + LevelSim v1 都 honoured)。**

### 動到的檔(本 release vs `v0.3.1`)

詳細表在 [`docs/RELEASE_v0.4.0.md`](RELEASE_v0.4.0.md) §1。 摘要 23 files / +6260 / -11:

- **Production** (5 files): `Source/ArchSim/Public/Editor/ArchSimScenarioWidget.h` (NEW, +328) / `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp` (NEW, +642) / `Source/ArchSim/ArchSim.Build.cs` (Editor-only block +26) / `Source/ArchSim/Private/Tests/ArchSimPieHarness.cpp` (AS-26 +12) / `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` (AS-27 +7)
- **Tests** (4 files): `ArchSimScenarioWidgetTest.cpp` NEW (+152, 7 sub-checks) / `ArchSimScenarioSolveWireTest.cpp` NEW (+183, 7 sub-checks) / `ArchSimScenarioTutorialTest.cpp` NEW (+361, 8 sub-checks) + `Scripts/run_gate.ps1` `$ExpectedUeTests` 145→148 / 143→146
- **Docs** (2 files): `docs/ARCHITECTURE_INDEX.md` §7 backlog + Latest tag + § 2 class map + § 6 test inventory + § 5 dataflow note (+12) + `docs/logs/S-04/manager.md` post-release notes (+151)
- **Sprint logs** (12 files NEW under `docs/logs/S-05/`): scope contract + plan + manager + 7 agent logs + ue58_eval.md + u3_pie_smoke.md

### 什麼未動

- FrameCore engine source (`v4.0.0` FROZEN since 2026-06-23) — 整 release **0 行**
- LevelSim engine (`v1` FROZEN) — 整 release **0 行**
- 4 個外部 plugin source (ALS / Prefabricator / SPUD / SUQS) — READ-only per scope anti-goal
- `Plugins/FrameSolver/Source/FrameCoreUE/` public USTRUCT / UCLASS layout 不變
- `ArchSim.uproject` / `.gitignore` / build artifacts — 鐵則 #5
- `Plugins/LevelSim/*` — 整 plugin dir 0 lines (LevelSim v1 FROZEN)
- `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` / `.cpp` (consume-only per scope: u2/u3 widget reads Registry surface but doesn't modify Registry source)
- `UFrameInteractiveSubsystem` / FrameCoreUE plugin source (consume-only)
- `CLAUDE.md` "現況" block — per v0.x cadence (CLAUDE.md tracks FrameCore engine v4.x/v3.x/v2.x line only; v0.x game-body track is documented via ARCH_INDEX + per-version HANDOFF/RELEASE notes — matches v0.3.x/v0.2.x/v0.1.x precedent)

---

## 2. 怎麼跑

```powershell
# Pre-req
$env:UE_ENGINE_ROOT      # 指 UE 5.7 install root
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"
# openseespy 在 system Python (pip install openseespy) — NOT 在 framecore-direct env

# UE editor incremental build
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# 一鍵 5-leg gate (cuDSS host expects 148)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS, UE 148 tests run, exit 0

# Non-cuDSS host fallback (146)
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 146

# Isolated Scenario suite (3 tests)
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Gameplay.Scenario; Quit" `
    -unattended -nullrhi -log
# Expect: ScenarioWidget + ScenarioSolveWire + ScenarioTutorial 各 Result={成功} + EXIT CODE: 0
```

### 使用者必做(v0.4.0 carry forward 自 v0.3.1 + 新增 Scenario MVP BP child)

⚠️ **C++ 端 only ships — UAsset 由 project author 在 UE Editor 建立**:

1. (carry from v0.3.1) 開 UE Editor → `ArchSim.uproject`,按 `docs/INPUT_MAPPING.md` 在 `Content/Input/` 建 6 個 UAsset,assign 到 BP `AArchSimCharacter` 子類
2. **(NEW for v0.4.0)** 建 `UArchSimScenarioWidget` 的 BP 子類:Content Browser 右鍵 → Editor Utilities → Editor Utility Widget → parent class `UArchSimScenarioWidget`(Filter "ArchSim Scenario Widget")。 命名 `EUW_ScenarioWidget` 或類似
3. **(NEW for v0.4.0)** Implement BP overlay UI:在 BP 子類的 Designer 加 buttons(Place K1 / Place K2 / Place K4 / Test Structure / Reset)及 Text overlay。Event Graph 接 `On Tutorial State Changed`(渲染當前 prompt text)+ `On Voice Prompt Should Play`(可選:接任何 BP-callable TTS node 或 ignore)
4. **(NEW for v0.4.0)** 跑 PIE 5min student trial smoke(v0.4.0 hard gate — 見 `docs/logs/S-05/u3_pie_smoke.md` 9-section flow);若 PASS,v0.4.0 站得住;若 FAIL,參考 post-publish hotfix protocol(`docs/RELEASE_v0.4.0.md` § 4 deferred 項目)

### v0.4.0 額外可確認(可選)

5. 跑 `FrameCore.UE.InteractiveSubsystem.*` 子樹 isolated 仍有 4 個 sub-tests(v0.3.0 carry,verified PASS)
6. 跑 `ArchSim.Gameplay.Scenario*` 子樹 isolated 應有 3 個 sub-test class:ScenarioWidget(u1/u3 extended)/ ScenarioSolveWire(u2)/ ScenarioTutorial(u3)

---

## 3. 新 token / 新 flag / 新 API

### Production-side (`UArchSimScenarioWidget` Editor Utility Widget)

**全套新 BP-callable API**(只在 `#if WITH_EDITOR` build 下存在;packaged shipping 完全不 link):

| API | Signature | 用途 |
|---|---|---|
| `PlaceK1Column(FVector)` | `AActor* PlaceK1Column(FVector LocationWorld)` | 放 K1 column placeholder(default geometry: 1m beam along +X);U1 |
| `PlaceK2Beam(FVector)` | `AActor* PlaceK2Beam(FVector LocationWorld)` | 放 K2 horizontal beam placeholder(2m along +X);U3 |
| `PlaceK4Brace(FVector)` | `AActor* PlaceK4Brace(FVector LocationWorld)` | 放 K4 diagonal brace placeholder(2m 45° XZ;hypotenuse ≈200.8cm);U3 |
| `RequestSolveAndVisualize()` | `bool RequestSolveAndVisualize()` | 觸發 Registry debounced solve + delegate subscribe + lazy-spawn `AFrameUtilizationHeatmapActor`;U2 |
| `AdvanceTutorialStep()` | `void AdvanceTutorialStep()` | 推進 `EArchSimTutorialState` 線性 state machine;fires `OnTutorialStateChanged` + `OnVoicePromptShouldPlay`;U3 |
| `GetCurrentPromptText()` | `FText GetCurrentPromptText() const` | BlueprintPure 回 current state 對應 FText(LOCTEXT namespace `ArchSimTutorial`);U3 |
| `ResetWidgetState()` | `void ResetWidgetState()` | unsubscribe delegate + destroy HeatmapActor + reset state → Welcome + 清 PlacedActors reference list(不銷毀 K-set actors 本身);U3 |
| `OnTutorialStateChanged(EArchSimTutorialState NewState, const FText& PromptText)` | `BlueprintImplementableEvent` | BP 子類 override 顯示 overlay UI;U3 |
| `OnVoicePromptShouldPlay(const FString& PromptText)` | `BlueprintImplementableEvent` | BP 子類 override 接 voice TTS(可選);C++ 端 NOT 主動 link SDK;U3 |

### Production-side (`EArchSimTutorialState` UENUM)

```cpp
UENUM(BlueprintType)
enum class EArchSimTutorialState : uint8
{
    Welcome,          // Widget just opened
    PromptPlaceK1,    // Ask student to place a column
    PromptPlaceK2,    // Ask student to place a beam
    PromptPlaceK4,    // Ask student to place a brace
    PromptPressTest,  // Ask student to press Test Structure
    FreeExplore,      // Tutorial done; student free (terminal)
};
```

Linear state machine, `FreeExplore` is terminal(`AdvanceTutorialStep` no-op once reached).

### New BlueprintReadOnly UPROPERTY surface

- `EArchSimTutorialState TutorialState` — initial `Welcome`;BP 可讀 current state(`#if WITH_EDITORONLY_DATA` wrapped)
- `TObjectPtr<AFrameUtilizationHeatmapActor> HeatmapActor` — lazy-spawned via u2;BP 可讀 reference(`#if WITH_EDITORONLY_DATA` wrapped)
- `TArray<TObjectPtr<AActor>> PlacedActors` — widget-owned soft list of spawned K-set actors;ResetWidgetState 清 list 但不銷毀 actors(`#if WITH_EDITORONLY_DATA` wrapped)

### Test path additions

新 3 個 IMPLEMENT_SIMPLE_AUTOMATION_TEST class,全 in `ArchSim.Gameplay.Scenario*` namespace:
- `ArchSim.Gameplay.ScenarioWidget`(7 sub-checks;u1 + extended by u3)
- `ArchSim.Gameplay.ScenarioSolveWire`(7 sub-checks;u2)
- `ArchSim.Gameplay.ScenarioTutorial`(8 sub-checks;u3)

### Build.cs 變動

新 `if (Target.Type == TargetType.Editor)` block 在 `Source/ArchSim/ArchSim.Build.cs`:
```csharp
if (Target.Type == TargetType.Editor)
{
    PrivateDependencyModuleNames.AddRange(new string[] {
        "Blutility", "UMG", "UMGEditor", "UnrealEd",
    });
}
```
Non-Editor target 完全不 link Editor deps,packaged shipping 不增 binary size。

### Gate 變動

- `Scripts/run_gate.ps1` L29 `$ExpectedUeTests = 148`(原 v0.3.1 145);non-cuDSS fallback 146(原 143)

### Hook 變動 (OUTSIDE repo)

`~/.claude/hooks/work-phase-guard.ps1` L104 foreign-state regex `^S-\d+$` → `^S-[\w]+$` + WHY comment block(AS-25-u1)。 本 repo 不受影響;OUTSIDE repo,跨 machine 需手動 sync。

---

## 4. 仍 deferred 的 items + Sprint S-06 建議排序

### Path B 餘 (LOW;outside repo / cosmetic)

#### AS-28: Hook case-sensitivity + .bak header comment sync (LOW; OUTSIDE repo)

**Status**:S-05 AS-25-u1 review (Findings #1 + #2)。 PowerShell `-notmatch` 是 case-insensitive,`s-04a` 小寫會 match `^S-[\w]+$`;current `/work` convention 全大寫,無實際 impact。

**First action on day 1**:Edit `~/.claude/hooks/work-phase-guard.ps1` L104 `-notmatch` → `-cnotmatch`(case-sensitive variant);regenerate `.bak` 讓 header comment 同步 production。 跑 4-scenario stdin test 確認 backward-compat。 OUTSIDE repo,no ArchSim commit。

#### AS-29: `run_gate.ps1` standalone leg PowerShell env race diagnosis (LOW)

**Status**:S-05 AS-27-u1 review (Finding #3)。 AS-27-u1 subagent 觀察 `[1/5] standalone: exit 1` 在 PowerShell session 但 direct `build.bat` PASS;AS-26-u1 subagent 同 host 同 script 卻得 `[1/5] standalone: ALL PASS`。 Likely shell-state / cwd / PATH race during parallel dispatches。 Workaround 已 documented(direct `Plugins/FrameSolver/Standalone/build.bat`)。

**First action on day 1**:Add `Push-Location $PSScriptRoot/..` + `Pop-Location` 在 `Scripts\run_gate.ps1` Leg [1/5] 包覆 `Invoke-Expression` `build.bat`;或改用 `& cmd /c "$PSScriptRoot/../Plugins/FrameSolver/Standalone/build.bat"` 顯式 cmd shell 隔離 PowerShell env state。 跑 5-leg gate 從 2 個 terminal session 並行驗。

### Path A spike follow-up(若 user 跑 PIE smoke FAIL post-publish)

#### v0.4.0 PIE 5min smoke real-world adjudication (USER-DRIVEN gate)

**Status**:v0.4.0 release uses headless gate + reviewer u3_pie_smoke.md actionability assessment as proxy。 User can run actual PIE 5min smoke any time post-publish per `docs/logs/S-05/u3_pie_smoke.md` 9-section flow。

**First action on day 1**(若 user 想跑 PIE smoke 確認):

```powershell
# (a) UE Editor 開
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor.exe" "$PWD\ArchSim.uproject"

# (b) 建 BP child class for UArchSimScenarioWidget
# Content Browser → Add → Editor Utilities → Editor Utility Widget
# Parent class: ArchSim Scenario Widget (Filter "ArchSim")
# Name: EUW_ScenarioWidget

# (c) 在 BP Designer 加 UI controls;Event Graph 接 OnTutorialStateChanged + OnVoicePromptShouldPlay

# (d) 開 widget: Window → Editor Utility Widgets → EUW_ScenarioWidget

# (e) PIE → 跟 docs/logs/S-05/u3_pie_smoke.md 9-section flow

# (f) 結果記錄 in u3_pie_smoke.md §9 evidence template
```

若 PIE smoke PASS → v0.4.0 站得住;Scenario MVP claim verified end-to-end。
若 PIE smoke FAIL → 走 post-publish hotfix protocol(`docs/RELEASE_v0.4.0.md` § 4 末):mark `v0.4.0` prerelease + ship `v0.4.0.1` hotfix 修真 bug,OR revert 到 `v0.3.2`-style patch path (cherry-pick Path B-only AS-25/26/27 commits 出來)。

### Z-01 PhaseB (UE5.8 sandbox plugin build)

**Status**:S-05 SPIKE-UE5.8-eval。 Phase A NO-GO(UE5.8 install not detected on host)。 decision doc 完整 + sandbox `Research/ue58_attempt/` 留 untracked 供 idempotent re-run。

**First action on day 1**(S-06):
1. 安裝 UE5.8 via Epic Games Launcher → `E:\project\UE_5.8\`(與 UE 5.7 同 disk root style)
2. 設 `$env:UE_ENGINE_ROOT_58 = "E:\project\UE_5.8"`
3. 跑 `docs/logs/S-05/ue58_eval.md` § 5 Step 2 PowerShell sandbox build(copy 4 plugin → `Research/ue58_attempt/Plugins/` + 建 `UE58Probe.uproject` + Build.bat)
4. 預期 SPUD RED(`StructUtils` module missing in UE5.8)→ 套 fix path(`SPUD.Build.cs` 移除 `"StructUtils"` dep;include side 已有 `ENGINE_MINOR_VERSION >= 5` guard 自動 cover)
5. 全 4 plugin GREEN 後 → 更新 4 個 `.uplugin` `EngineVersion` 為 `"5.8.0"` + `ArchSim.uproject` `EngineAssociation` 為 `"5.8"` → 跑全 5-leg gate

### Carry-over from earlier sprints (no S-06 commitment)

- **AS-04** Plugins panel visual confirmation (~30 min human). **First action:** 開 UE Editor → Edit → Plugins → filter "ALS"/"Prefabricator"/"SPUD"/"SUQS",截圖到 `docs/screenshots/gate0_plugins_panel.png`
- **AS-05** K1-T2 / K4 art assets (parallel human-side). **First action:** 與 art owner 確認 source pipeline + 為新 K2/K4 placeholder 也準備 mesh asset(`Static Mesh` plate or simple rectangular box 即可);BP child class 在 Details panel assign
- **AS-06** SPUD UE5.5 `StructUtils` deprecation。 **First action:** couples to Z-01-PhaseB 上(已 included in S-06 first action)
- **AS-08** SPUD `RF_Transient` audit。 **First action:** `grep -rn "RF_Transient\|UPROPERTY.*Transient" Plugins/SPUD/Source/` 確認 save-game 路徑不洩漏 transient component data
- **AS-09** non-cuDSS host re-verify。 **First action:** 在無 RTX GPU host 跑 `.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 146` 確認 non-cuDSS path 146 tests 全綠

---

## 5. 過程留下的教訓 (durable, S-05 specific)

僅本 Sprint S-05 學到的;與全域教訓無重疊:

1. **Per-unit Phase 3 review with inline-fix-then-accept is faster than re-dispatch loops.** S-05 7 個 review 全 NITS-with-inline-fix (zero BLOCKER)。 平均 review wall 100-140s + 1-3 line inline-fix。 對比 BLOCKER → Phase 2 re-dispatch 預估 wall ~30min 新 subagent + Phase 3 again。 適用 case:reviewer 的 finding 是 documentation precision / 1-line wording / 1-line API mismatch;**不**適用:架構錯誤 / FROZEN 違反 / silent fabrication(這些必須 BLOCKER + re-dispatch)。

2. **Anthropic session-limit 中段截斷 subagent 是 platform constraint 不是 subagent ESCALATE.** SPIKE-Scenario-u2 dispatch hit session-limit at 60/50 steps + 16.5 min。 Subagent 沒寫 final markdown report 但 working tree 留下實質 code。 Main thread substitute oracle(build + isolated tests + 5-leg gate 全跑一次)是 valid recovery path,不需 re-dispatch。 Phase 3 reviewer 仍 independently verify code quality;Special section "session-limit truncation honesty assessment" pin 此 anomaly 路徑為合規收尾。

3. **Subagent verification-scope misread is a real risk; main thread must verify-by-default after spike units.** SPIKE-Scenario-u3 subagent 把 PIE smoke USER-driven 推廣到 build + tests + gate 也 USER-driven(誤判 verification scope)。 Main thread 跑 UE build 才發現 `FUNC_BlueprintImplementableEvent` undefined in UE5.7(主因:subagent 沒驗 reflection API constant 存在性)。 inline-fix L306+L325 為 `FUNC_BlueprintEvent`,build pass。 **Lesson:對 spike unit(尤其 add new IMPLEMENT_SIMPLE_AUTOMATION_TEST 或 reflection-heavy test),Phase 2 prompt 應明確 list build / test / gate 都 subagent's job;Phase 3 review 前 main thread 跑一次 build 確認 compile clean。**

4. **`#if WITH_EDITORONLY_DATA` for UPROPERTY inside `#if WITH_EDITOR` class is the UHT 5.4+ canonical pattern.** u2 HeatmapActor + u3 TutorialState + PlacedActors 三個 UPROPERTY 全 wrap `#if WITH_EDITORONLY_DATA`(不是 `#if WITH_EDITOR`)。 UHT 5.4+ 規則: UProperties should not be wrapped by WITH_EDITOR(只 ENG-side 編譯但 UHT codegen 仍 visible)→ 用 `WITH_EDITORONLY_DATA` 真正排除 packaged-cook 端的 metadata。 兩個 macros 在 Editor build 下都 true 所以行為 identical;UHT 不會 warn。

5. **`AddUObject` to multicast delegate needs EXPLICIT Remove(Handle) in BeginDestroy.** SPIKE-Scenario-u2 Phase 3 reviewer Finding #1:`AddUObject` 的 implicit weak-object protection 是 implementation detail not API contract;subagent 原本只 Reset 本地 `FDelegateHandle` 但沒 call `Registry->OnSolveComplete.Remove(Handle)`。 Inline-fix:加 `TWeakObjectPtr<UArchSimModelRegistry> SubscribedRegistry` private member;subscribe 時 cache;BeginDestroy 用 cache pointer call Remove。 此 pattern 在 u3 BeginDestroy 也 carry forward。 **Lesson:future multicast subscribe code 一律走 explicit Remove pattern,不依賴 weak-object protection。**

6. **File-scope static helpers in .cpp avoid transitive header pollution to unity TUs.** `BuildMemberGeometryFromRegistry` 在 .cpp 是 file-scope static(不是 class member)。 WHY:如 declare in .h 會讓所有 include `ArchSimScenarioWidget.h` 的 TU(包括 UHT gen.cpp unity TUs)必 transitively include `FrameCoreUEVisualTypes.h`,可能造成 ODR/compile issues in non-Editor unity build path。 同 pattern 應用 to u3's `GetPromptForState` + `NextTutorialState` static helpers。 **Lesson:any helper that needs full type visibility for `TArray<>` return type but is only called from same-TU functions,放 .cpp file-scope static,不要 declare in .h。**

7. **PIE 5min smoke is USER-driven by design; release-time proxy + post-publish verification is acceptable.** v0.4.0 hard gate per scope contract 是 "試玩學生實地跳 PIE 走 5 分鐘 不炸"。 Headless commandlet 不能 cover 5min unattended PIE input + viewport observation。 User 在 release-hardening 階段 authorize the headless gate + reviewer doc-actionability assessment 作為 release-time proxy;real PIE smoke 可任意 post-publish 跑。 若 post-publish PIE smoke FAIL → hotfix protocol(v0.4.0.1)或 fall-back patch path。 **Lesson:不是所有 hard gate 都能 in-release verify;有些是 release-time proxy + post-publish real verification 的兩段式。**

---

## 6. 後續方向

### Sprint S-06 建議排序

backlog after v0.4.0:

1. **Z-01-PhaseB** UE5.8 install + sandbox plugin build(per `docs/logs/S-05/ue58_eval.md` §5)— headline for S-06 if user wants UE5.8 path forward
2. **PIE 5min smoke real-world adjudication** by user(若 user 想 verify v0.4.0 claim)→ 若 PASS 則 done;若 FAIL 走 hotfix
3. **AS-28 + AS-29** LOW cleanup — outside-repo hook + run_gate.ps1 env race。 Bundle in 一個 v0.4.x patch 或 outside-repo only
4. **Scenario MVP iteration**:加入更多 K-set types(K3 = shell / wall?), tutorial overlay BP UI 真實 BP-side polish, voice TTS BP node 接入(可選 Windows SAPI 或 Azure TTS via BP plugin)
5. **AS-04 + AS-05** 美術 + 視覺確認 (human-side parallel)
6. **AS-08** SPUD orchestration 當決定接 SPUD save-game 時
7. **AS-06** SPUD `StructUtils` deprecation 跟 Z-01-PhaseB 一起做
8. **AS-09** non-cuDSS host re-verify 機會性

### 何時 bump 下個 minor

- v0.4.x patch line 適合 AS-28/29 + 任何 cosmetic NIT 收尾 + Scenario MVP iteration polish
- 當 UE5.8 upgrade GO + 主 worktree 升級 → bump v0.5.0
- 當 Scenario MVP playable for 試玩學生 (post-publish PIE smoke PASS 確認) → 也可獨立 bump v0.5.0 為 "Scenario MVP verified" minor

### 風險區

- **UE5.8 升級** — Z-01-PhaseB 是 prerequisite;SPUD `StructUtils` dep removal 是已知 required fix。 若 SPUD 升級失敗 → 需要 ALS-Refactored / Prefabricator 各別 verify;最壞 case fork SPUD
- **`UCLASS(Abstract)` on `UArchSimScenarioWidget`** — production usage 是 BP child class instantiation。 若 user 忘記建 BP child 直接 try open `Editor Utility Widget` window 會看不到 ScenarioWidget。 doc 在 HANDOFF §2 "使用者必做" 已 cover,但仍是 minor footgun
- **`HeatmapActor` lifecycle 跨 PIE sessions** — `IsValid()` guard 有 cover GC-pending case,但 stale TObjectPtr 在 PIE-A 結束 / PIE-B 開始之間若 GC 未跑可能短暫 confuses lazy-spawn check。 Headless test 不 cover 此 cross-PIE timing path;`docs/logs/S-05/u3_pie_smoke.md` 步驟 6 reload 子 step 是 covering check

---

接手有問題:
- `docs/HANDOFF.md` → `docs/HANDOFF_v0.1.md` → `HANDOFF_v0.1.1.md` → … → `HANDOFF_v0.3.0.md` → `HANDOFF_v0.3.1.md` → 本檔
- Sprint S-05 完整 manager log: `docs/logs/S-05/manager.md` (append-only)
- v0.4.0 release notes: `docs/RELEASE_v0.4.0.md`
- v0.4.0 PIE 5min smoke instructions(USER-DRIVEN gate): `docs/logs/S-05/u3_pie_smoke.md`
- UE5.8 eval decision doc: `docs/logs/S-05/ue58_eval.md`
- Architecture index: `docs/ARCHITECTURE_INDEX.md`
- Sprint notes (cross-sprint summary): `docs/SPRINT_NOTES.md`
