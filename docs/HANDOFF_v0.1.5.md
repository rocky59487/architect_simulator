# 交接指南 — `v0.1.5` 後接手 owner

> `v0.1.5` 在 2026-06-26 發布,tag `v0.1.5` 接在 `v0.1.4` 之後。Sprint S-02 第二個
> release。AS-02 trilogy(a/b/c)落地完成。下一階段:AS-03 ALS pawn stack 推到 v0.2.0。

---

## 1. `v0.1.5` = 什麼

一句話:**AS-02b 完成 `UArchSimGameInstance::Tick()` 的真實 driver loop(registered-count
delta detection → empty `RequestSolve` 觸發 debounce + first solve);AS-02c 新增 headless
smoke test (`ArchSim.Integration.TickDriver`, 7 sub-checks)。Engine 源 0 行動;
`ArchSimModelRegistry.{h,cpp}` 0 行動;ArchSim production code delta = 75 行。**

### 動到的檔

| Path | LOC delta | Type | Sprint unit |
|---|---|---|---|
| `Source/ArchSim/Public/ArchSimGameInstance.h` | +26 (mod) | Production | AS-02b |
| `Source/ArchSim/Private/ArchSimGameInstance.cpp` | +49 (mod) | Production | AS-02b |
| `Source/ArchSim/Private/Tests/ArchSimGameInstanceTest.cpp` | +147 (new) | Test | AS-02c |
| `Scripts/run_gate.ps1` | L29 138→139 + comment append | Build | AS-02c |
| `docs/ARCHITECTURE_INDEX.md` | §2/§6/§7/§8 updates | Docs | release |
| `docs/RELEASE_v0.1.5.md` | new | Docs | release |
| `docs/HANDOFF_v0.1.5.md` | new (本檔) | Docs | release |
| `docs/logs/S-02/{agent_AS-02b,agent_AS-02c,manager}.md` | new + append | Sprint logs | /work driver |
| `README.md` | + v0.1.5 status block | Docs | release |

### v0.1.4 deferred items 處理

| ID | Status | 處理 |
|---|---|---|
| AS-02 (Tick driver) | ✅ closed in v0.1.5 (a/b/c) | scope-narrowed:dirty = registered-count delta(非 position) |
| AS-03 (ALS pawn) | 🟡 still open (拆 a/b/c/d) | 下個 dispatch unit AS-03a 開始 |
| AS-04, AS-05 | 🔵 stretch deferred next session | 同 v0.1.4 |
| AS-06, AS-08, AS-09 | 🟡 / 🔵 | 同 v0.1.4 |
| AS-11, AS-12 | 🟡 backlog | 同 v0.1.4 |

### 新加 backlog (來自 AS-02c CLEAN review)

- **AS-13** — PIE-world fixture for driver-loop + trip-path observability
  - AS-10 trip path (`bNeedsRebaseline=true` flag set + `Sub->Rebaseline()`)
    + AS-02 driver-loop (`Tick → GetSubsystem → Registry`)兩個分支都因為 GameInstance
    subsystem pipeline 不啟動而在 headless `NewObject<T>()` fixture 不可達。完整測 trip
    + driver loop 需要真實 PIE world。
  - Sprint: S-03 或 later。Priority: MEDIUM。
  - 解法:用 `FAutomationEditorCommonUtils::CreateNewMap`(或等同)起 PIE world,
    place 96+ `UArchSimMemberData` actors,tick world 數 frame,assert
    `Registry->IsRebaselineDue()` flip + `GameInstance->GetSolveTriggerCount()` increment。

### 什麼未動

- FrameCore engine source(`v4.0.0` FROZEN)
- LevelSim engine(`v1` FROZEN)
- 4 plugin clones (ALS / Prefabricator / SPUD / SUQS)
- `ArchSim.uproject` / `.gitignore` / build artifacts
- `UArchSimModelRegistry.{h,cpp}`(`RegisterMember` / `DeactivateMember` / `RequestSolve` /
  `ExecuteSolve` / `PatchRank` byte-identical)
- v0.1.x shipped tests(全保留)

---

## 2. 怎麼跑

```powershell
# 一鍵 5-leg gate (現在預設 139 cuDSS / 137 non-cuDSS)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS, UE 139 tests run, GATE_EXIT=0

# 非 cuDSS host:
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 137

# AS-02c smoke 單跑
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Integration.TickDriver; Quit" `
    -unattended -nullrhi -log
# Expect: Result={成功} Name={TickDriver}

# 所有 ArchSim 測試一起
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim; Quit" `
    -unattended -nullrhi -log
# Expect: 4 tests (SaveLoadRoundTrip + MaxRankCeiling + RebaselineCeiling + TickDriver)
```

---

## 3. 新 token / 新 flag / 新 API

### `UArchSimGameInstance`(AS-02b 擴充)

- `Tick(float DeltaSeconds)` body 現在 (a) 增 TickCount/AccumulatedSeconds (b) 取 Registry subsystem (c) 比對 `GetRegisteredCount()` 跟 `LastSeenRegisteredCount` (d) 若 `!=` 則 emit `RequestSolve(FFrameModelPatch{})` + ++SolveTriggerCount + 更新 LastSeen
- 新 private members:`LastSeenRegisteredCount = -1` / `SolveTriggerCount = 0`
- 新 BP-pure getters:`GetLastSeenRegisteredCount()` / `GetSolveTriggerCount()`(供 future 觀察用)
- 全部 `[[nodiscard]]` + `const`

### Gate 變動

- `Scripts/run_gate.ps1` L29 `$ExpectedUeTests` 138 → 139 (cuDSS) / 136 → 137 (non-cuDSS)
- 新 test path:`ArchSim.Integration.TickDriver`(同 `ArchSim.*` namespace 已 cover)

### Documents

- `RELEASE_v0.1.5.md`(新)、`HANDOFF_v0.1.5.md`(本檔)
- `ARCHITECTURE_INDEX.md`:§2 class map 把 `UArchSimGameInstance` 從 "Planned" 移到 "Production classes";§6 UE test inventory 加 `ArchSim.Integration.TickDriver`;§7 AS-02 標 ✅ closed v0.1.5 + 加 AS-13;§8 cheat-sheet count sync

---

## 4. 仍 deferred 的 items + 下個 session 排序

### Z-01: AS-03a 是下個 dispatch unit(同 session 繼續)

`/work` session 接著跑(主對話會在 v0.1.5 publish 之後回到 phase-2 dispatch AS-03a),first action:

1. 確認 ALS-Refactored v4.17 plugin 已 enabled(SPRINT_NOTES Spike 1 已確認)
2. 新建 `Source/ArchSim/Public/Characters/ArchSimCharacter.h`,繼承 `AAlsCharacter`(from `Plugins/ALS/Source/ALS/Public/AlsCharacter.h`)
3. `Source/ArchSim/ArchSim.Build.cs` 加 `"ALS"` 依賴(Public 或 Private)
4. Constructor 設 `bUseControllerRotationYaw = false`(ALS 內部 handle)
5. UE build 通過,5-leg gate 維持 139(沒新 test 在 AS-03a)

### Z-02: AS-03b (Enhanced Input mapping + IMC/IA specs)

`SetupPlayerInputComponent` 用 Enhanced Input。InputAction 用 `TObjectPtr<UInputAction>` 成員 + `BindAction(IA, ETriggerEvent::Triggered, ...)`。IMC + IA 是 UAsset(必須在 UE Editor 建立);agent 寫 `docs/INPUT_MAPPING.md` 給使用者按表建。

### Z-03: AS-03c (Camera + ArchSimGameMode)

ALSCamera 模組提供 `UAlsCameraComponent`(third-person)。新 `ArchSimGameMode` 繼承 `AGameModeBase`,`DefaultPawnClass = AArchSimCharacter::StaticClass()`。`Config/DefaultEngine.ini` set `GlobalDefaultGameMode=/Script/ArchSim.ArchSimGameMode`。

### Z-04: AS-03d (Test + BP)

`ArchSimCharacterTest.cpp` automation 用 Enhanced Input simulation(若 headless 不能跑 → defer 到 AS-13 PIE fixture)。`$ExpectedUeTests` 139 → 140 (cuDSS) / 137 → 138 (non-cuDSS)。AS-03d 落地 = **v0.2.0 minor bump**(user-visible feature ALS pawn)。

### Z-05: AS-11 / AS-12 / AS-13

下次 cleanup 窗口處理。AS-13 是 MEDIUM priority(test 完整度的補救);AS-11/12 是 LOW。

---

## 5. 過程留下的教訓(durable)

僅本 release 學到的;與全域教訓無重疊:

1. **「RegisterMember 不自動 trigger solve」是設計缺口,Tick driver 的本職** — Read 完
   `ArchSimModelRegistry.cpp:131-208`(RegisterMember body)發現它只 mutates
   `CurrentModel + IndexToComponent`,沒呼 RequestSolve。相比之下,
   `DeactivateMember`(cpp:391-393)會。所以 Tick driver 不需要做「actor 移動偵測」
   就能填 70% 的場景(static buildings 的 PIE 載入 burst)。Position-sync 是 future
   feature,不是 AS-02 主軸。

2. **`!=` 比 `<` 或 `>` 更安全** — Tick body 內判 count delta 用 `!=` 同時 cover add
   跟 remove。Remove 由 DeactivateMember 自己 RequestSolve;Tick 內 `!=` 的功能是讓
   `LastSeenRegisteredCount` 在 remove 後也更新,避免下次 idle frame 重複 fire。
   `<` 或 `>` 都會漏 update 跟 idle bypass 兩個邊界。

3. **Headless `NewObject<UArchSimGameInstance>()` `GetSubsystem<T>()` 回 null** —
   跟 v0.1.4 AS-10 的 GI guard 同個 pattern。在 headless `-nullrhi -unattended`,
   `UGameInstance` 沒有被 UE engine 正確 init,subsystem pipeline 沒串,所以
   `GetSubsystem<T>()` 一律回 null。AS-02c smoke test 必須對「null Registry → Tick body
   early-bail」的事實誠實,而非 mock subsystem 假裝跑了。

4. **AS-13 PIE-world fixture 是 v0.1.4+v0.1.5 兩個 unit 的 follow-on** — AS-10 trip
   path 跟 AS-02 driver loop 兩個 branch 都因為「headless 沒有 live GameInstance」
   而不可達。其中一個 PIE fixture 可以同時解兩件事。Priority MEDIUM(test 完整度
   問題,不是 production bug)。

5. **Adversarial reviewer 對 flag 選擇的 precedent verification 是好 pattern** — AS-02c
   agent 用了 `SmokeFilter` 而非 prompt template 的 `ProductionFilter`,自己給的理由是
   「matches `ArchSimSaveLoadTest.cpp` precedent」。Reviewer 沒 trust 這個 justification,
   真去 Read SaveLoadTest L82 + RebaselineTest L73 + MaxRankCeiling L288 三個 precedent
   檔確認,結論「matches 是真的,非 fabricated」。Future agent 對 flag/convention 選擇
   給 justification 時,reviewer 應該逐個 verify precedent,不接受口頭解釋。

6. **AS-02 trilogy(a/b/c)在一個 session 完成的 cost** — AS-02a 14.4min / AS-02b 8.8min
   / AS-02c 9.5min。三個 subagent dispatch + 三個 review + 兩個 release ceremony
   (v0.1.4 bundle + v0.1.5 bundle)。約 70-80min wall + main thread coordination
   tokens。Sprint 預算 8h 內,實際 ~1.5h。**早期 dispatch 多收斂,後期會更快**:
   AS-02b/c 比 AS-02a 快是因為 GameInstance 結構已熟悉,後續 AS-03 stack 同理。

---

## 6. 後續方向

### Sprint S-02 接下來(同 session,v0.2.0 minor 在 AS-03d 落地)

backlog 排序:

1. **AS-03a**(3h)— `AArchSimCharacter` subclass `AAlsCharacter`
2. **AS-03b**(3h)— Enhanced Input mapping + IMC/IA specs
3. **AS-03c**(3h)— Camera + `AArchSimGameMode` wire
4. **AS-03d**(3h)— Character automation test + BP → **v0.2.0 minor bump**
5. (stretch)AS-05 美術 + AS-04 視覺確認 → 下個 session

### 風險區

- **AS-03a ALS plugin runtime** — Build 已 verify(SPRINT_NOTES Spike 1)但 character
  spawn + ALS state machine 在 PIE 沒 spike 過,風險中
- **AS-03b Enhanced Input IMC/IA** — UAsset binary,agent 不能直接建,需 spec doc 給
  使用者在 Editor 按表建。可能需要 user 在 Phase 4 / Phase 6 提供 Editor 端配合
- **AS-03d automation test** — 若 ALS character 也撞 headless GetSubsystem null 問題,
  同樣走 honest partial coverage + AS-13 PIE fixture deferred 套路

---

接手有問題:`docs/HANDOFF_v0.1.md` → `HANDOFF_v0.1.1.md` → `HANDOFF_v0.1.2.md` →
`HANDOFF_v0.1.3.md` → `HANDOFF_v0.1.4.md` → 本檔。
Sprint S-02 進度:`docs/logs/S-02/manager.md`(append-only)。
v0.1.5 release notes:`docs/RELEASE_v0.1.5.md`。
