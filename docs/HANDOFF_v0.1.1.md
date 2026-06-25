# 交接指南 — `v0.1.1` 後接手 owner

> `v0.1.1` 在 2026-06-25 發布,tag `v0.1.1` 接在 `v0.1` 之後。
> 主交接文是 [`docs/HANDOFF_v0.1.md`](HANDOFF_v0.1.md);本檔只補上 `v0.1` → `v0.1.1`
> 多出來的內容(即 A1-07 SaveLoadRoundTrip 的落地 + 它揭露的 gate gap)。
> Commit SHA 由 `git log v0.1.1 -1` 取得。

---

## 1. `v0.1.1` = 什麼

一句話:**A1-07 SaveLoadRoundTrip 測試落地;FrameCore + LevelSim 兩條引擎源碼皆 0 行動。**

- **新增 1 個檔**:`Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp`(262 行,
  `IMPLEMENT_SIMPLE_AUTOMATION_TEST FArchSimSaveLoadRoundTripTest`)
- **更新 1 個檔**:`docs/SPRINT_NOTES.md`(加 S-01 區段;A1-07 sub-section + Decision Log 第 3 條)
- **新增 2 個檔**:`docs/RELEASE_v0.1.1.md` + 本檔
- **更新 1 個檔**:`README.md`(加 v0.1.1 patch status block 於 v0.1 block 上方)
- **`git diff --stat v0.1..v0.1.1`** 預期 5 files changed,+~700 / -~30
- **engine 源**:`Plugins/FrameSolver/Source/FrameCore/` = 0 / `Plugins/FrameSolver/Source/FrameCoreUE/` = 0 / `Plugins/LevelSim/` = 0(鐵則 #1 + #5 全守)

### 整入了哪些 v0.1 deferred items

- **A1-07 SaveLoadRoundTrip**(`HANDOFF_v0.1.md §4 item #1` 的依賴項)→ 落地。
  原 §4 item #1 是「A1-06 全套整合」,A1-07 為其前置;A1-06 仍 deferred。
- **SPUD UE5.5 risk**(`HANDOFF_v0.1.md §4 item #6`)→ A1-07 採 proxy-archive
  stub 路徑,**未直接動 SPUD**,該 deferred 仍懸而未決(UE5.7 → 5.8 升級前必查)。

### 什麼未動

- FrameCore engine(v4.0.0 FROZEN)
- LevelSim engine(v1)
- `Scripts/run_gate.ps1`(故意不動 — 詳見 §4 item AS-01)
- 4 個外部 plugin clone(`Plugins/ALS` / `Prefabricator` / `SPUD` / `SUQS`)
- `ArchSim.uproject` / `Config/DefaultEngine.ini` / `.gitignore`

---

## 2. 怎麼跑

### 跑 A1-07 本身

```powershell
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Persistence.SaveLoadRoundTrip; Quit" `
    -unattended -nullrhi -log

# Verify
Select-String -Path Saved\Logs\ArchSim.log `
    -Pattern 'ArchSim\.Persistence\.SaveLoadRoundTrip','EXIT CODE'
# Expect: Result={成功}  +  **** TEST COMPLETE. EXIT CODE: 0 ****
```

### 跑既有 5-leg gate(v0.1 引擎驗證,不含 A1-07 — 見 §4 item AS-01)

```powershell
Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS (UE 135 FrameCore tests)
```

### 跑 A1-07 + 5-leg gate(臨時包,直到 AS-01 落地)

```powershell
# 兩條 invocation 串接;exit code 任一非零即 FAIL
Scripts\run_gate.ps1 -RequireOpenSees ; if ($LASTEXITCODE -ne 0) { exit 1 }
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Persistence.SaveLoadRoundTrip; Quit" `
    -unattended -nullrhi -log
```

---

## 3. 新 token / 新 flag / 新 API

**無。** v0.1.1 純加 1 個 automation test;公開 API、CLI token、`SolveOptions` flag
全不動。

唯一 surface 新增是 test 自己的 path:
- **Automation filter**:`ArchSim.Persistence.SaveLoadRoundTrip`(`ArchSim.*` 命名空間
  下第一個 test;未來其他 `ArchSim.*` test 沿襲此命名)

---

## 4. 仍 deferred 的 items(從 RELEASE_v0.1.1.md §7 對齊)

每項加 **First action on day 1** 一行 — 直接照做,不用再想。

### 1. **AS-01:`ArchSim.*` namespace 不在 `Scripts/run_gate.ps1`**

- 來源:Phase 4.5 integrator gate-coverage 檢查抓到 `Scripts/run_gate.ps1:70` 的
  `ExecCmds = 'Automation RunTests FrameCore; Quit'` 只跑 `FrameCore.*`
- First action:`Scripts/run_gate.ps1:70` 改 `ExecCmds = 'Automation RunTests FrameCore+ArchSim; Quit'`
  (UE Automation 支援 `+`-separated filter chain);同時 `:29` 的 `$ExpectedUeTests = 135`
  → 136;non-cuDSS fallback `-ExpectedUeTests 133` → 134;備註行加 "v0.1.1 +1 ArchSim test"
- 風險:bump 後 FrameCore-only build 跑 gate 會 fail count guard 除非 ArchSim test 也編進去 →
  確認 `ArchSim` module 在 default editor build target 中,實際 UE build 已含

### 2. **AS-02:A1-06 全套整合**(原 `HANDOFF_v0.1.md §4 item #1`)

- Sprint S-02,8h 預算
- First action:`Source/ArchSim/Private/ArchSimGameInstance.cpp` 加 `Tick(...)`,
  每 frame 把所有 `UArchSimMemberData` 的位置同步進 registry → 觸發 `SetCurrentDemand`
  → BP 更新 `CachedUtilization`
- A1-07 已驗 SaveLoad 契約,A1-06 可解開依賴鏈

### 3. **AS-03:A2-01 ALS pawn 接入**(原 `HANDOFF_v0.1.md §4 item #3`)

- Sprint S-02,12h 預算
- First action:`Source/ArchSim/Public/Characters/ArchSimCharacter.h` 新建,繼承自
  `AAlsCharacter`;加 Enhanced Input mapping;Camera mode 預設 third-person

### 4. **AS-04:Gate 0 視覺確認**(原 `HANDOFF_v0.1.md §4 item #4`)

- 0.5h 人工動作
- First action:`%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe ArchSim.uproject`
  → Edit → Plugins → 過濾 "ALS" / "Prefabricator" / "SPUD" / "SUQS" 四個 Enabled
  → `docs/SPRINT_NOTES.md` L141-142 兩個 `[ ]` 打勾

### 5. **AS-05:K1-T2 / K4 美術前置**(原 `HANDOFF_v0.1.md §4 item #5`)

- Sprint S-02 / S-03,24h 預算(parallel)
- First action:UE Editor → `Content/UI/Fonts/` mkdir;匯入 Noto Sans CJK TC OFL
  字體;建 `UFont` asset

### 6. **AS-06:SPUD UE5.5 `StructUtils` 棄用風險**(原 `HANDOFF_v0.1.md §4 item #6`)

- 升 UE 5.8 前必查
- First action:**不要** UE 5.7 → 5.8 直接升;先到 https://github.com/sinbad/SPUD
  releases 查最新 commit 是否已替代 `StructUtils`;若未替代則 fork + patch 或評估
  替代 plugin(如 `EasyMultiSave` / 自寫 SaveGameSubsystem)

### 7. **AS-07(本 release 新增):A1-07 MaxRank=96 真實 stress test**

- Sprint S-02+,~2h
- First action:`Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp` 末尾加
  `IMPLEMENT_SIMPLE_AUTOMATION_TEST FArchSimMaxRankCeilingTest` test path
  `ArchSim.Persistence.MaxRankCeiling`;loop 註冊 97 個 fake member;assert 第 97 個
  觸發 rebaseline(`Registry->IsRebaselineDue()` 為 true)或 `RegisterMember` 回 -1

### 8. **AS-08(本 release 新增):SPUD orchestration `RF_Transient` audit**

- 當 S-02+ 接 SPUD 真實 orchestration 時觸發
- First action:`grep -rn "RF_Transient" Source/ArchSim/Private/Components/` —
  生產路徑的 `UArchSimMemberData` 構造 **不能** 帶 `RF_Transient`,否則 SPUD 反射掃描
  跳過該 component → save 內容遺失。A1-07 test 用 `RF_Transient` 是因為 proxy archive
  手動呼叫,所以 OK;生產 spawn 必須移除

---

## 5. 過程留下的教訓(durable)

僅本 release 學到的;與 CLAUDE.md 全域教訓無重疊:

1. **`AActor` 無預設 `RootComponent`** — `SpawnActor(Class, FTransform(Location), Params)`
   在 base `AActor::StaticClass()` 上 **silently drops the Location** 因為沒有
   transform-bearing root。修法:`NewObject<USceneComponent>` + `Actor->SetRootComponent(Root)`
   + `Root->RegisterComponent()` + `Actor->SetActorLocation(Loc)`。`v0.1.1` test 內留 inline
   comment 給後人(`ArchSimSaveLoadTest.cpp:119-121`)。**S-02+ 新測試 actor 都套這個模板**。

2. **MSVC 預設 codepage 不吃 UTF-8 multi-byte 字元 in literal** — `§` (U+00A7) 觸發
   "`\xA74` escape sequence out of range" 編譯錯。Project-wide:**test diagnostic
   字串只用 ASCII**;非 ASCII 字元只放 `.md`(UTF-8 BOM-less)+ `UE_LOG(LogTemp, ...)`
   的 `TEXT()` macro 包起來

3. **Headless `-nullrhi -unattended` 不能 `UWorld::CreateWorld`** — 編輯器 GameInstance
   template 沒 load。canonical workaround:走 `GEngine->GetWorldContexts()` walk 拿
   existing。`Source/ArchSim/Private/Tests/` 下任何後續 test **必須** 用同 pattern
   (test 內的 `FindSpawnWorld()` helper 可抽到 `ArchSimTestHelpers.h` 給後人共用,
   defer 到第二個 test 出現時再做 dedup,避免 PMC-DUP-01 的 premature abstraction 反例)

4. **`ArchSim.*` 自成命名空間** — 從本 release 起,所有 `Source/ArchSim/` 下的 UE
   test 都以 `ArchSim.<Category>.<TestName>` 命名(對應 v0.1.1 的
   `ArchSim.Persistence.SaveLoadRoundTrip`)。`FrameCore.*` 保留給 engine layer。
   `ArchSim.*` 下子類別:`Persistence` / `Integration` / `Gameplay` / `UI`(暫定)

5. **release-hardening 即使 1 個 commit、1 個 test 也要跑** — 本 cycle 證明:單一
   小 commit 在 phase 4.5 仍能抓出 gate-coverage gap(AS-01)+ 兩個前瞻 deferred
   (AS-07 stress + AS-08 RF_Transient)。整合者視角(cross-cutting view)在每個 cycle 都有價值

---

## 6. 後續方向

### Sprint S-02(下個 minor:v0.2)115h 預算

主軸(順位排好,我審完每個 task 再給下一個 prompt):
1. **AS-01**:Scripts/run_gate.ps1 加 ArchSim 命名空間(~30 min)
2. **AS-02**:A1-06 全套整合(8h)
3. **AS-03**:A2-01 ALS pawn 接入(12h)
4. **AS-04**:Gate 0 視覺確認(0.5h 人工)
5. **AS-05**:K1-T2 / K4 美術前置(24h parallel)
6. **AS-07**:A1-07 MaxRank 真實 stress test(2h)

### v0.1.x patch:可能的 micro-cycles

- 若 Sprint S-02 中 AS-01 落地後想立刻 ship,可開 v0.1.2 patch(只動 `run_gate.ps1`)
- 後續 cycles 累積 ≥ 3 任務就 bump v0.2 minor

### 風險區(下一 release 必看)

- **AS-06 (SPUD `StructUtils`)**:UE 升 5.8 是個時間炸彈,優先程度比想像高
- **`FObjectAndNameAsStringProxyArchive` 的 SPUD compat**:當實際接 SPUD 時,確認
  proxy archive 序列化的 byte 格式跟 SPUD 自己存的 byte 格式相容(可能不需要相容,但
  要驗)

---

接手有問題:先讀 [`docs/HANDOFF_v0.1.md`](HANDOFF_v0.1.md) → 本檔。
`Source/ArchSim/` 子系統問題讀 [`docs/IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md)
S-01 / S-02 章節。Sprint 進度看 [`docs/SPRINT_NOTES.md`](SPRINT_NOTES.md)。
