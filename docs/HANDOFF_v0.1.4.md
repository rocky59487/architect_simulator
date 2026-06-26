# 交接指南 — `v0.1.4` 後接手 owner

> `v0.1.4` 在 2026-06-26 發布,tag `v0.1.4` 接在 `v0.1.3` 之後。
> 主交接文連結:`HANDOFF_v0.1.md` → `HANDOFF_v0.1.1.md` → `HANDOFF_v0.1.2.md`
> → `HANDOFF_v0.1.3.md` → 本檔。本檔記 v0.1.3 → v0.1.4 兩個落地單元
> (AS-02a + AS-10)+ 兩個新 NITS backlog (AS-11, AS-12)。

---

## 1. `v0.1.4` = 什麼

一句話:**Sprint S-02 第一個 release。AS-02a 落 `UArchSimGameInstance` skeleton
(production-only;`FTickableGameObject` 整合,Tick body 只是 telemetry counter,
driver loop 留 AS-02b);AS-10 close v0.1.3 deferred 的真實 `PendingRankAccumulation`
ceiling test(`ArchSim.Persistence.RebaselineCeiling`,7 sub-checks,誠實標記 headless
GI-guard 阻擋 trip path 的限制)。Engine 源 0 行動;LevelSim 源 0 行動。**

### 動到的檔(7 個 + sprint logs + 2 個新 docs + README 一段)

| Path | LOC delta | Type | Sprint unit |
|---|---|---|---|
| `Source/ArchSim/Public/ArchSimGameInstance.h` | +75 (new) | Production | AS-02a |
| `Source/ArchSim/Private/ArchSimGameInstance.cpp` | +68 (new) | Production | AS-02a |
| `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` | +24 (mod) | Production | AS-10 |
| `Source/ArchSim/Private/Tests/ArchSimRebaselineTest.cpp` | +220 (new) | Test | AS-10 |
| `Config/DefaultEngine.ini` | +8 (mod) | Config | AS-02a |
| `Scripts/run_gate.ps1` | L29 137→138 + comment append | Build | AS-10 |
| `docs/ARCHITECTURE_INDEX.md` | new track + §2/§6/§7 update + L232 cheat-sheet fix | Docs | AS-10 + NITS fix |
| `docs/RELEASE_v0.1.4.md` | new | Docs | release |
| `docs/HANDOFF_v0.1.4.md` | new (本檔) | Docs | release |
| `docs/logs/S-02/{scope,plan,agent_AS-02a,agent_AS-10,manager}.md` | new tree | Sprint logs | /work driver |
| `README.md` | + v0.1.4 status block | Docs | release |

### v0.1.3 deferred items 處理

| ID | Status | 處理 |
|---|---|---|
| AS-10 | ✅ closed in v0.1.4 (with honest limitation) | 落地 + spec correction |
| AS-02 | 🟡 still open (拆 a/b/c) | a 落 v0.1.4;b/c 留 v0.1.5 |
| AS-03 | 🟡 still open (拆 a/b/c/d) | 全留 v0.2.0 |
| AS-04, AS-05 | 🔵 stretch goal,deferred next session | 同 v0.1.3 |
| AS-06 | 🔵 deferred (UE5.8 升前) | 同 v0.1.3 |
| AS-08, AS-09 | 🟡 open / 🔵 deferred | 同 v0.1.3 |

### 新加 backlog (來自 AS-10 NITS)

- **AS-11** — `ArchSimModelRegistry.h` 內 comment 引 `cpp:303/315/324/331` 4 個 reset 點,reviewer 建議補上 per-branch label(L303 = no-Sub / L315 = session-start-fail / L324 = post-rebaseline / L331 = end-of-ExecuteSolve)。LOW,cosmetic doc。
- **AS-12** — `GetMaxRankBeforeRebaseline()` static constexpr getter 目前無 production consumer,只 test 用。建議解法:(a) 加 HUD/heatmap "已用 rank / 上限" indicator,或 (b) 加 TODO comment 標 expected consumer。LOW,borderline anti-goal。

### 什麼未動

- FrameCore engine source(`v4.0.0` FROZEN)
- LevelSim engine(`v1` FROZEN)
- 4 plugin clones (ALS / Prefabricator / SPUD / SUQS)
- `ArchSim.uproject` / `.gitignore` / build artifacts
- `UArchSimModelRegistry` 的 production logic (RequestSolve / ExecuteSolve / PatchRank 全 byte-identical;只加 3 個 const noexcept getter 到 header)
- v0.1.x shipped tests (`SaveLoadRoundTrip` + `MaxRankCeiling` 兩個 vacuous assertion 保留,bisect 路徑保留)

---

## 2. 怎麼跑

```powershell
# 一鍵 5-leg gate (現在預設 138 cuDSS / 136 non-cuDSS)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS, UE 138 tests run, GATE_EXIT=0

# 非 cuDSS host:
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 136

# AS-10 單跑
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Persistence.RebaselineCeiling; Quit" `
    -unattended -nullrhi -log

# AS-02a smoke (沒專屬 test,看 reflection)
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="list class /Script/ArchSim.ArchSimGameInstance; Quit" `
    -unattended -nullrhi -log
# 看 Saved/Logs/ArchSim.log 含 ArchSimGameInstance reflection 結果
```

---

## 3. 新 token / 新 flag / 新 API

### Production-side(`UArchSimGameInstance`)

- `UArchSimGameInstance::Init()` / `Shutdown()` 跟 UE 標準 lifecycle 順序一致
- `UArchSimGameInstance::Tick(float)` 只有 telemetry counter(AS-02b 會擴);**不要在 v0.1.4 之上加 driver loop,留給 AS-02b 動**
- `IsTickable()` 三條件 AND(`bIsActive && GetWorld() && !IsTemplate()`) — 改任何條件前先確認 race-free
- `LogArchSim` 是 module-wide log category(`DECLARE_LOG_CATEGORY_EXTERN` in header)
- BP-pure accessors:`GetTickCount()` / `GetAccumulatedTime()`(AS-02c smoke 會用)

### Production-side(`UArchSimModelRegistry` 3 個 telemetry getter)

```cpp
[[nodiscard]] int32 GetPendingRankAccumulation() const noexcept;
[[nodiscard]] bool  IsRebaselineDue() const noexcept;
[[nodiscard]] static constexpr int32 GetMaxRankBeforeRebaseline() noexcept;  // 回 96
```

⚠️ 三個都是 pure observer,不改 behavior。任何看到「為什麼這些 getter 存在」的維護者,請看 AS-12 backlog 條目。

### Config 變動

`Config/DefaultEngine.ini` 新增 section:

```ini
[/Script/EngineSettings.GameMapsSettings]
GameInstanceClass=/Script/ArchSim.ArchSimGameInstance
```

⚠️ 改 `Config/DefaultEngine.ini` 在 UE build 時會 trigger `Invalidating makefile for ArchSimEditor (DefaultEngine.ini modified)` warning — 預期行為,非 bug(SPRINT_NOTES Spike 1 已記)。

### Gate 變動

- `Scripts/run_gate.ps1` line 29 `$ExpectedUeTests` 137 → 138 (cuDSS)
- non-cuDSS fallback 135 → 136
- 新 test path `ArchSim.Persistence.RebaselineCeiling` (同 `ArchSim.*` namespace,run_gate filter `FrameCore+ArchSim` 已 cover)

---

## 4. 仍 deferred 的 items + 下個 session 排序

### Z-01: AS-02b 是下個 dispatch unit(同 session 繼續)

如果 `/work` session 接著跑(現在 main thread 在 phase-4 published 完之後會回 phase-2 dispatch AS-02b),first action:

1. Read `Source/ArchSim/Public/ArchSimGameInstance.h`(看 AS-02a 留下的 Tick body)
2. Read `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h`(看 RequestSolve API + MoveMemberEndpoint 沒有 → 需要 deactivate+re-register 替代 pattern)
3. 在 `ArchSimGameInstance::Tick(float)` 加 dirty-detection + sync member positions + `RequestSolve(patch)`
4. Tick 跟 `UArchSimModelRegistry::DebounceTimer` 不可打架(後者已 debounce 150ms,Tick 內不可繞過 debounce 強制 solve)
5. 5-leg gate 138 維持(沒新加 test)

### Z-02: AS-02c smoke test 後續(同 session)

End-to-end smoke 在 `ArchSimGameInstanceTest.cpp` 新建,test path `ArchSim.Integration.TickDriver`。spawn 5 actors → register → tick → move 1 → tick → assert `CachedUtilization` 變化。`$ExpectedUeTests` 138 → 139 (cuDSS) / 136 → 137 (non-cuDSS)。

### Z-03: AS-03a..d ALS pawn stack(同 session)

`AAlsCharacter` subclass → Enhanced Input → Camera mode → automation test。v0.2.0 minor bump 在 AS-03d 後出。

### Z-04: AS-04 + AS-05 stretch goal

若 session-task-cap 10 還有空間:font import + Plugins panel 視覺(後者你人工)。

### Z-05: AS-11 + AS-12 (來自 v0.1.4 AS-10 NITS)

下次 cleanup 窗口處理。LOW priority。

### Z-06: 從 v0.1.3 carry-forward 的 deferred items 不變

- AS-06 (SPUD/StructUtils, UE5.8 前再處理)
- AS-09 (non-cuDSS host 再驗)

---

## 5. 過程留下的教訓(durable)

僅本 release 學到的;與全域教訓無重疊:

1. **`UGameInstance` + `FTickableGameObject` 整合的 lifecycle 順序是不對稱的** —
   Init 中 `bIsActive=true` 必須**最後**(避免 Super::Init 內部觸發假 IsTickable);
   Shutdown 中 `bIsActive=false` 必須**最先**(避免 Shutdown 期間 race)。這是非顯
   然的對稱性破壞,active-voice 註解在 `ArchSimGameInstance.cpp` 中標出。

2. **GameInstance reflection 是 Config `GameInstanceClass=` 解析的必要條件** —
   不是「有 C++ class 就 work」,還要 UHT generated `Z_Construct_UClass_*` 真實註冊。
   AS-02a 的 sanity check 3 用 UE-Cmd 的 `list class` 已驗,但 `list class` 的 output
   不走 stdout(走 internal log channel),`Select-String` 抓不到 — 改讀
   `Saved/Logs/ArchSim.log`,或對 UHT generated file (`Intermediate/Build/.../UHT/*.gen.cpp`)
   做 oracle 確認。

3. **`RequestSolve` 的 GI-null guard 在 headless 不可繞過** —
   `RequestSolve` (cpp:269) 先 `MergePatch + PatchRank` accum(L271-272),然後
   `GetGameInstance()` null check(L274-275),最後 trip check(L281)。**accum
   增量 happens BEFORE GI guard,所以 headless 可觀察 accumulator;但 GI guard
   early-return 之後,trip code 在 headless 永遠不執行**。
   future 想完整測 trip path(`bNeedsRebaseline=true` + `ExecuteSolve` + `Sub->Rebaseline()`)
   必須 PIE world fixture,headless `NewObject<T>` fallback 不夠。**AS-10 的 7
   sub-checks 是 honest partial coverage,trip 觀察留 future PIE fixture(AS-13?)**。

4. **「Spec 跟 production 不符」第二次出現 = pattern 而非偶然** —
   v0.1.3 是 AS-07 spec 寫「register 97 reject」、production 是「register 97 OK」;
   v0.1.4 AS-10 是 plan 寫「第 96 次 trip」、production 是「strict `>`,第 97 才 trip」。
   AS-07 lesson #1 在 v0.1.4 再次成立:**test 是 production 行為的契約 snapshot,
   不是 spec 的願望投影**。Future units 寫 test 時,先 Read production code 確
   定真實行為,再寫 test 不要倒過來。

5. **「動 production header 加 telemetry getter」是 anti-goal 的灰色地帶** —
   AS-10 加了 3 個 const getter (`GetPendingRankAccumulation` /
   `IsRebaselineDue` / `GetMaxRankBeforeRebaseline`)。前 2 個有 plausible
   future consumer(HUD 顯示 rank budget);第 3 個 static constexpr 目前**只**
   給 test 用,reviewer 標 borderline。判斷標準:若 future production consumer
   合理且不寫 「為 test 而加」 ,則 OK;否則 NITS(AS-12 已開)。

6. **Sprint logs `docs/logs/S-XX/` 是 /work 7-phase 的 audit trail,不是 scratch** —
   v0.1.4 是第一個有 `docs/logs/S-02/` 樹的 release。`scope` / `plan` / `agent_*` /
   `manager.md` 各有用途,future session 接手讀 `manager.md` append-only 序列
   就能還原當時決策。**不要刪 sprint logs,即使內容看起來「太細」** — bisect
   過去決策需要它。

---

## 6. 後續方向

### Sprint S-02 接下來(同 session,v0.2.0 minor 累積)

backlog 排序(plan §AS-XX 內):

1. **AS-02b**(3h)— `ArchSimGameInstance::Tick` driver loop(member sync → RequestSolve)
2. **AS-02c**(2h)— end-to-end smoke + BP accessors → `v0.1.5` patch
3. **AS-03a**(3h)— `AArchSimCharacter` subclass `AAlsCharacter`
4. **AS-03b**(3h)— Enhanced Input mapping + IMC/IA specs
5. **AS-03c**(3h)— Camera mode + `AArchSimGameMode` wire
6. **AS-03d**(3h)— Character automation test + BP wire → **v0.2.0 minor bump**
7. (stretch)AS-05 美術 + AS-04 視覺確認 → 下個 session

### 何時 bump v0.2 minor

`AS-03d` 落地,user-visible feature(ALS pawn = 角色操作)真的可在 UE Editor demo。

### 風險區

- **AS-02b Tick driver 跟 `RequestSolve` debounce 互動** — Tick 在 game frame rate (60fps),`RequestSolve` 內部 debounce 150ms。Tick 若沒 dirty detection 直接 call RequestSolve,patch 列每幀膨脹 → 撞 MaxRank=96 trip ceiling。**對策**:Tick 內必 cache last position,沒變動不 RequestSolve。
- **AS-03 ALS plugin runtime issue** — ALS-Refactored v4.17 在 UE5.7 已驗 build(SPRINT_NOTES Spike 1)但 runtime spawn character + input simulation 沒 spike 過,風險中。撞牆就開 spike worktree。

---

接手有問題:`docs/HANDOFF_v0.1.md` → `HANDOFF_v0.1.1.md` → `HANDOFF_v0.1.2.md` →
`HANDOFF_v0.1.3.md` → 本檔。
Sprint S-02 進度:`docs/logs/S-02/manager.md`(append-only)。
v0.1.4 release notes:`docs/RELEASE_v0.1.4.md`。
