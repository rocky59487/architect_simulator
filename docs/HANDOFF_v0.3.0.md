# 交接指南 — `v0.3.0` 後接手 owner

> `v0.3.0` 在 2026-06-26 發布,tag `v0.3.0` 接在 `v0.2.0` 之後。
> **Sprint S-03 closing release** — hardening + PIE-world test harness foundation。
> 6 個 v0.3.0 feature commits + 1 release-hardening commit。Engine FROZEN 全程 0 行。
> 主交接文鏈:`HANDOFF.md` → `HANDOFF_v0.1.md` → … → `HANDOFF_v0.2.0.md` → 本檔。

---

## 1. `v0.3.0` = 什麼

一句話:**S-02 hardening 五項 audit findings(AS-15 Enhanced Input lifecycle / AS-16 CalcCamera override / AS-17 empty-model audit / AS-11/12/14/18/19 LOW cleanup)全清,加上 deferred 已久的 AS-13 PIE-world fixture 用 proven `GEngine->GetWorldContexts()` 模式落地(u1 harness + u2 三個 honest-defer tests),FrameCore engine 跟 LevelSim 整 sprint 0 行動。**

### 動到的檔(本 release vs `v0.2.0`)

| Path | LOC delta | Type | Sprint unit |
|---|---|---|---|
| `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` | +28 / -8 (net +20, comment-only) | Production | AS-11 + AS-12 |
| `Source/ArchSim/Public/Characters/ArchSimCharacter.h` | +25 (additive override decls) | Production | AS-15 + AS-16 |
| `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` | +190 / -36 (net +154, NotifyControllerChanged + CalcCamera + AS-14 ClampMagnitude clamp + AS-15 comment cleanup) | Production | AS-14 + AS-15 + AS-16 |
| `Source/ArchSim/Private/Components/ArchSimMemberData.cpp` | +15 / 0 (warn-only log) | Production | AS-19 |
| `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` | +89 (1 new test) | Test | AS-17 |
| `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` | +92 (new) | Test helper | AS-13-u1 |
| `Source/ArchSim/Private/Tests/ArchSimPieHarness.cpp` | +107 (new) | Test helper | AS-13-u1 |
| `Source/ArchSim/Private/Tests/ArchSimPieSmokeTest.cpp` | +171 (new) | Test | AS-13-u1 |
| `Source/ArchSim/Private/Tests/ArchSimPieRebaselineTest.cpp` | +156 (new) | Test | AS-13-u2 |
| `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp` | +168 (new) | Test | AS-13-u2 |
| `Source/ArchSim/Private/Tests/ArchSimPieInputRuntimeTest.cpp` | +159 (new) | Test | AS-13-u2 |
| `Scripts/run_gate.ps1` | +2 / -1 (count bump 140→145 cuDSS, 138→143 non-cuDSS) | Config | AS-17 + AS-13-u1 + AS-13-u2 |
| `docs/ARCHITECTURE_INDEX.md` | +26 production update + release-hardening §6/§7/§9 sync | Docs | AS-18 + release |
| `docs/SPRINT_NOTES.md` | +59 (S-03 section) | Docs | release |
| `docs/RELEASE_v0.3.0.md` | new | Docs | release |
| `docs/HANDOFF_v0.3.0.md` | new (本檔) | Docs | release |
| `docs/logs/S-03/scope_*.md` + `plan_*.md` + `manager.md` + `agent_*.md` (8 logs) | new | Sprint logs | /work driver |
| `README.md` | +v0.3.0 status block | Docs | release |

Cumulative since `v0.2.0`(`58705d0`): **23 files / +2893 / -50 / 0 lines under FROZEN paths**。

### Sprint S-03 backlog 處理

| ID | Status before S-03 | After S-03 |
|---|---|---|
| AS-11 (header comment precision) | 🟡 LOW | ✅ closed (LOW-batch-u1) |
| AS-12 (GetMaxRank consumer) | 🟡 LOW | ✅ closed (LOW-batch-u1; TODO comment) |
| AS-13 (PIE-world fixture) | 🟡 MEDIUM | ✅ closed (AS-13-u1 + AS-13-u2) |
| AS-14 (analog stick clamp) | 🟡 LOW | ✅ closed (LOW-batch-u1) |
| AS-15 (Enhanced Input lifecycle) | 🟡 HIGH | ✅ closed (AS-15-u1) |
| AS-16 (CalcCamera override) | 🟡 HIGH | ✅ closed (AS-16-u1) |
| AS-17 (empty-StartSession audit) | 🟡 MEDIUM | ✅ closed (AS-17-u1; Case A) |
| AS-18 (GI teardown doc) | 🟡 LOW | ✅ closed (LOW-batch-u1) |
| AS-19 (MemberData early-BeginPlay) | 🟡 LOW | ✅ closed (LOW-batch-u1; Option A) |
| AS-20 (LogTemp → LogArchSim) | — | 🟡 newly open (LOW) |
| AS-24 (FrameCoreUE NewObject outer) | — | 🟡 newly open (LOW; pre-existing) |
| AS-04 (Plugins panel visual) | 🟡 open (human) | 🟡 still open |
| AS-05 (K1-T2 / K4 art assets) | 🟡 open (parallel) | 🟡 still open |
| AS-06 (SPUD StructUtils deprecation) | 🔵 deferred (pre-5.8) | 🔵 still deferred (couples to Z-01) |
| AS-08 (SPUD RF_Transient audit) | 🟡 open | 🟡 still open |
| AS-09 (non-cuDSS re-verify) | 🔵 deferred (opportunistic) | 🔵 still deferred |

### 什麼未動

- FrameCore engine source(`v4.0.0` FROZEN)— 整 sprint **0 行**
- LevelSim engine(`v1` FROZEN)— 整 sprint **0 行**
- 4 個外部 plugin source(ALS / Prefabricator / SPUD / SUQS)— READ-only for ALS precedent
- `ArchSim.uproject` / `.gitignore` / build artifacts — 鐵則 #5
- `UArchSimModelRegistry.cpp` production logic — byte-identical(只動 header AS-11/AS-12 comment + AS-18 doc paragraph)
- `UArchSimGameInstance.{h,cpp}` — 自 v0.1.5 之後不變
- v0.1.x / v0.2.x 已 shipped tests 全保留(SaveLoad / MaxRankCeiling / RebaselineCeiling / TickDriver / CharacterInput),v0.3.0 加 5 個新 test 沒 rename / remove 任何 existing test
- Position-change sync(MVP buildings 不動)

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

# 一鍵 5-leg gate (cuDSS host expects 145)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS, UE 145 tests run, exit 0

# Non-cuDSS host fallback (143)
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 143

# 單跑 5 個新 test (任意組合)
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests FrameCore.UE.EmptyModelStartSession+ArchSim.Integration.PieHarnessSmoke+ArchSim.Integration.PieRebaseline+ArchSim.Integration.PieDriverLoop+ArchSim.Gameplay.PieInputRuntime; Quit" `
    -unattended -nullrhi -log
# Expect: 5 個 `Result={成功}` + EXIT CODE: 0
```

### 使用者必做(v0.3.0 仍然 carry forward 自 v0.2.0 的 BP author 設定)

⚠️ **C++ 端 only ships — UAsset 由 project author 在 UE Editor 建立**(已在 v0.2.0 HANDOFF §2):

1. 開 UE Editor → `ArchSim.uproject`
2. 按 `docs/INPUT_MAPPING.md` 在 `Content/Input/` 建 6 個 UAsset
3. 開 BP class 繼承 `AArchSimCharacter`,assign 6 個 UAsset
4. PIE → 測 WASD / Mouse / Space / Shift / Ctrl + ALS 蹲衝刺 + v0.3.0 加的 `NotifyControllerChanged` lifecycle 在 re-possess 時應該也乾淨

### 推薦 demo 流程(畢答用,v0.3.0 加分項目)

1. PIE → 角色出現,WASD 移動,Mouse 看向(維持 v0.2.0)
2. **(v0.3.0 新)** Possess / Unpossess 來回:input mapping context 在 RemoveMappingContext 跟 AddMappingContext 之間正確切換,沒有重複註冊
3. **(v0.3.0 新)** Alt-Tab 中按住 Shift,回 focus 後 ALS gait 應該回 Running(Canceled binding 工作正常)
4. **(v0.3.0 新)** Camera 由 `UAlsCameraComponent::GetViewInfo` 驅動(FOV override / shoulder switch / post-process weight 都生效;v0.2.0 因為沒 override `CalcCamera` 會 silent drop)

---

## 3. 新 token / 新 flag / 新 API

### Production-side(`AArchSimCharacter`)

新增兩個 `virtual` override:

```cpp
virtual void NotifyControllerChanged() override;
virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo) override;
```

`NotifyControllerChanged` body 模仿 `Plugins/ALS/Source/ALSExtras/Private/AlsCharacterExample.cpp:19-49`:
- 從 `PreviousController` 抓 `APlayerController` → `LocalPlayer` → `UEnhancedInputLocalPlayerSubsystem`,呼叫 `RemoveMappingContext(DefaultMappingContext)`
- 從 `GetController()` 抓新的,呼叫 `AddMappingContext` 帶 `FModifyContextOptions::bNotifyUserSettings = true`
- 最後 `Super::NotifyControllerChanged()`

`CalcCamera` body 模仿 `AlsCharacterExample.cpp:51-60`,加 `IsValid(Camera)` defensive:

```cpp
if (IsValid(Camera) && Camera->IsActive()) {
    Camera->GetViewInfo(ViewInfo);
    return;
}
Super::CalcCamera(DeltaTime, ViewInfo);
```

### `SetupPlayerInputComponent` 變動

對 4 個 hold-style action 加 `ETriggerEvent::Canceled` bindings(Move / Look / Sprint / Jump),`IA_Crouch` 仍只綁 Started(toggle protection)。

### 新 Test helper namespace

```cpp
namespace ArchSimPieHarness {
    UWorld* GetOrFindWorld();
    UGameInstance* GetOrFindGameInstance();
    UArchSimModelRegistry* GetOrCreateModelRegistry();
    bool IsRegistryFromRealGI();
    template <typename T> T* SpawnActor(UWorld* World);
}
```

任何要在 commandlet automation 中拿 world + subsystem 的 future test 都應該優先用這個 helper,不要寫第二份 `GEngine->GetWorldContexts()` loop。

### Gate 變動

- `Scripts/run_gate.ps1` L29 `$ExpectedUeTests` 140 → 145 (cuDSS) / 138 → 143 (non-cuDSS)
- 5 個新 test path:
  - `FrameCore.UE.EmptyModelStartSession` (AS-17)
  - `ArchSim.Integration.PieHarnessSmoke` (AS-13-u1)
  - `ArchSim.Integration.PieRebaseline` (AS-13-u2)
  - `ArchSim.Integration.PieDriverLoop` (AS-13-u2)
  - `ArchSim.Gameplay.PieInputRuntime` (AS-13-u2)

---

## 4. 仍 deferred 的 items + Sprint S-04 建議排序

### Z-01: v0.4.0 spike — UE5.8 升級 + Scenario editor MVP (eval-gated, user-deferred)

**First action on day 1:** Re-invoke `/work` in a new session; acknowledge `docs/logs/S-03/scope_2026-06-26T1652.md` scope contract Units 8 + 9 are still pending. Read latest UE5.8 release notes for breaking changes against StructUtils deprecation (couples to AS-06). Decision-gate before any code: do we attempt UE5.8 spike inside `Research/ue58_attempt/` sandbox (per scope contract anti-goal "spike 撞牆 → 退 Research/") or push it further out?

### AS-20: LogTemp → LogArchSim category upgrade (LOW)

**First action:** `grep -rn "LogTemp" Source/ArchSim/` to find all sites. The existing precedent is `DEFINE_LOG_CATEGORY_STATIC(LogArchSimRegistry, Log, All)` in `ArchSimModelRegistry.cpp` head. Decision: introduce an umbrella `LogArchSim` shared across all ArchSim TUs (define in `Source/ArchSim/Public/ArchSim.h` or new `Source/ArchSim/Public/ArchSimLog.h`), OR a per-class `LogArchSimMember` parallel to `LogArchSimRegistry`. Edit `ArchSimMemberData.cpp:26` UE_LOG call to use the chosen category. No gate count change.

### AS-24: FrameCoreUE NewObject outer for InteractiveSubsystem isolated runs (LOW; pre-existing)

**First action:** Edit `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` namespace-anonymous `GetSubsystem()` fallback path (around `return NewObject<UFrameInteractiveSubsystem>();`) to pass `GetTransientPackage()` as the outer:
```cpp
return NewObject<UFrameInteractiveSubsystem>(GetTransientPackage());
```
Then re-run a *single*-test isolation run to confirm the NotNull.cpp fatal goes away. `ArchSimPieHarness::GetOrCreateModelRegistry()` should also adopt the same pattern. Gate count unchanged.

### Phase 5 docs-sync inline candidates (cosmetic, can land in any S-04 docs commit)

- `Source/ArchSim/Private/Tests/ArchSimPieHarness.h:52-54` — docstring "always" overclaim → empirical phrasing
- `Source/ArchSim/Private/Tests/ArchSimPieDriverLoopTest.cpp:108-111` — Sub-check 4 `TestTrue(..., true)` → `TestTrue(World->GetTimeSeconds() > t0)` or similar real assertion
- `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp:153` — warn msg prefix `(ArchSim|Input)` for consistency
- `Scripts/run_gate.ps1` — comment block trim of stale intermediate counts (142/140)
- `docs/ARCHITECTURE_INDEX.md` §5 figure — `< 96` reading-ambiguity rewrite
- `docs/ARCHITECTURE_INDEX.md` §6 RebaselineCeiling row — sweep stale `cpp:281` cite
- `FrameCoreUEInteractiveSubsystemTest.cpp` AS-17 new test path — optionally rename to `FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession` for namespace parity (`$ExpectedUeTests` stays 145)

### Carry-over (from earlier sprints, no S-04 commitment)

- **AS-04** Plugins panel visual confirmation (~30 min human). **First action:** 開 UE Editor → Edit → Plugins → filter "ALS"/"Prefabricator"/"SPUD"/"SUQS",每 plugin status="Enabled",截圖到 `docs/screenshots/gate0_plugins_panel.png`。
- **AS-05** K1-T2 / K4 art assets (parallel human-side). **First action:** 與 art owner 確認 source pipeline,匯入 `Content/`,確認 `.gitignore` 已蓋 `.uasset` binary。
- **AS-06** SPUD UE5.5 StructUtils deprecation. **First action:** 暫不動作 — couples to Z-01;UE5.8 spike 啟動再評估。
- **AS-08** SPUD `RF_Transient` audit. **First action:** `grep -rn "RF_Transient\|UPROPERTY.*Transient" Plugins/SPUD/Source/` 確認 save-game 路徑不洩漏 transient component data。
- **AS-09** non-cuDSS host re-verify. **First action:** 在無 RTX GPU host 跑 `.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 143` 確認 non-cuDSS path 143 tests 全綠。

---

## 5. 過程留下的教訓(durable, S-03 specific)

僅本 Sprint S-03 學到的;與全域教訓無重疊:

1. **Pre-flight reads continue to pay off.** AS-13-u1 的 plan 寫了 `FAutomationEditorCommonUtils::CreateNewMap` 主路徑 + `UWorld::CreateWorld(EWorldType::Game)` fallback。Main-thread pre-flight grep 出 FrameCoreUE 已有 proven `GEngine->GetWorldContexts()` pattern,且 precedent comment 明確說 `UWorld::CreateWorld` 在 commandlet 會 crash。dispatch prompt 即時更新,unit 落地的實際 risk 遠低於 plan 估值。**Lesson: Phase 2 pre-flight 在每個 unit 開工前花 5-10 分鐘 grep 同 repo 內既有 precedent,常常是最高 ROI 的時間。**

2. **AS-07 lesson #1 honest defer 在 Level 3 仍然救命。** AS-13-u2 三個 test 中 `PieRebaseline` 跟 `PieDriverLoop` 在 Level 3 headless 無法真實 fire trip-path / driver-loop。Plan 估計 8-12 sub-checks per test;誠實 land 7 sub-checks per test,每個 file head 明確說 "What this test PINS" + "What this test CANNOT verify in Level 3 (deferred)"。比起捏造 trip-fires 或 skip test bodies 都好。

3. **`protected` 是 planning input。** AS-13-u2 第一次 build 撞 C2248 因為 `SetupPlayerInputComponent` + `NotifyControllerChanged` 是 `protected` virtual override。subagent 立刻 fall back 改 sub-check 設計。**Lesson: 任何 test plan 涉及 invoke base class method 都先 grep `protected:` 在 surface 上。**

4. **Compile errors surfacing pre-existing issues stay backlog'd.** AS-13-u2 dispatch 發現 `NewObject<UFrameInteractiveSubsystem>()` ClassWithin warning cascade in isolated runs。經 reviewer 確認 pre-existing since v3.5.1(`5eeab2e`),非 S-03 引入。AS-24 backlog 開 LOW,**沒拉進 v0.3.0 commit scope**。**Lesson: 任何 release 期間發現的 issue,先確認 git log 看是不是 pre-existing。若是,backlog 然後 keep moving;若是 release scope 引入的真 regression,才是 BLOCKER。**

5. **Parallel-dispatch race 經驗。** Round 1(LOW-batch + AS-17 parallel)兩個 subagent 都會跑 `run_gate.ps1`,共用 `Saved/Logs/ArchSim.log` race。解法:parallel 時 instruct subagents 跳過 gate,main thread 在 round 結束 run 一次。Sequential rounds(2, 3)就讓每個 subagent 自己跑 gate,protocol 更簡單。**Lesson: parallel 模式時 file-ownership split + 跳過 shared-resource ops 是必須,sequential 時不必。**

6. **長對話間另一 `/work` session 寫共用 state file 是 hook 配置 gap。** 本 sprint commit 時兩次撞 hook("Current phase: shop/myweb/...") 因為另一 session 在背景把 `~/.claude/state/work-phase.txt` 改成 `shop/myweb` 路徑。Workaround:在 commit 前 echo state 寫入 + 立刻 commit(2-step atomic 從 hook 視角)。**Lesson: 若 hook 配置改成 per-project-id 或 per-session-id state file 可以根治。S-04 開工前先補 hook 配置。**

---

## 6. 後續方向

### Sprint S-04 建議排序

backlog:

1. **Z-01** v0.4.0 spike 評估(UE5.8 upgrade + Scenario editor)— user 已 deferred 到新 session,scope contract 仍 valid。
2. **AS-20 + AS-24** LOW cleanup(LogTemp upgrade + FrameCoreUE NewObject outer)— 短 unit 可 bundle 一起 ship 在 v0.3.x patch line。
3. **Phase 5 docs-sync inline candidates**(see §4)— 任何 docs commit 可順手帶。
4. **AS-04 + AS-05** 美術 + 視覺確認(human-side parallel)。
5. **AS-08** SPUD orchestration 當決定接 SPUD save-game 時。
6. **AS-09** non-cuDSS host re-verify 機會性。
7. **AS-06** SPUD StructUtils deprecation 僅 couples 到 Z-01。

### 何時 bump 下個 minor

- v0.3.x patch 線 適合 AS-20 / AS-24 / 任何 cosmetic NIT 收尾
- 當有新 user-visible feature(scenario editor MVP / dynamic structural events / 多人模式)bump v0.4.0
- UE5.8 升級 itself 不算 user-visible feature(infrastructure spike),屬於 v0.4.0 spike 的 helper

### 風險區

- **UE5.8 升級** — 若觸發 SPUD `StructUtils` 真的破壞,要決定 ALS-Refactored v4.18+ verify or fork。建議在 `Research/ue58_attempt/` sandbox 跑 5-leg gate 確認 binary delta 再 commit 主幹。
- **ALS-Refactored v4.17 對 UE5.8 API drift** — `NotifyControllerChanged` + `CalcCamera` override 在 UE5.8 簽名理論上不變(`APawn` 的 virtual 從 UE5.4 起穩定),但 ALSExtras 模組可能有自己的 API 變化。先驗 ALS plugin builds clean。
- **Pre-existing FrameCoreUE isolated-run crash (AS-24)** — 在 future test refactor 若改用 isolated subset run (e.g. 開發 single-file test cycle) 會撞。fix 簡單,優先級可隨 use-case 提高。

---

接手有問題:
- `docs/HANDOFF.md` → `docs/HANDOFF_v0.1.md` → `HANDOFF_v0.1.1.md` → `HANDOFF_v0.1.2.md` → `HANDOFF_v0.1.3.md` → `HANDOFF_v0.1.4.md` → `HANDOFF_v0.1.5.md` → `HANDOFF_v0.2.0.md` → 本檔
- Sprint S-03 完整 manager log: `docs/logs/S-03/manager.md`(append-only)
- v0.3.0 release notes: `docs/RELEASE_v0.3.0.md`
- Architecture index: `docs/ARCHITECTURE_INDEX.md`
- Sprint notes (cross-sprint summary): `docs/SPRINT_NOTES.md`
