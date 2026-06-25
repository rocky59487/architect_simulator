# 交接指南 — `v0.1.3` 後接手 owner

> `v0.1.3` 在 2026-06-25 發布,tag `v0.1.3` 接在 `v0.1.2` 之後。
> 主交接文連結:`HANDOFF_v0.1.md` → `HANDOFF_v0.1.1.md` → `HANDOFF_v0.1.2.md` →
> 本檔。本檔只記 v0.1.2 → v0.1.3 新增的內容(即 AS-07 closure +
> MaxRank semantic correction + AS-10 deferred)。

---

## 1. `v0.1.3` = 什麼

一句話:**AS-07 落地兼修正 v0.1.1 / v0.1.2 文檔對 `MaxRankBeforeRebaseline=96`
語意的誤解;engine 源 0 行動;0 行 production code 改動。**

- **動到 2 個檔**:
  - `Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp`:append
    `FArchSimMaxRankCeilingTest`(+160 行,包含 25 行 header comment 引 file:line
    所有 production claim)
  - `Scripts/run_gate.ps1`:line 29 `$ExpectedUeTests` 136 → 137;cumulative
    comment 末尾追加 AS-07 註腳 + MaxRank 真實語意說明
- **新增 2 個檔**:`docs/RELEASE_v0.1.3.md`(含對外的 spec 更正章節)+ 本檔
- **更新 1 個檔**:`README.md`(加 v0.1.3 patch status block,demote v0.1.2)
- 0 production code 改動(鐵則 「不准改 production 配合 test」嚴格守住)

### 整入了哪些 v0.1.2 deferred items

- **AS-07**(`HANDOFF_v0.1.2.md §4 item 6`)→ ✅ **closed**(但實際行為跟原 spec 不同;
  詳見 §5)

### 什麼未動

- FrameCore engine(v4.0.0 FROZEN)
- LevelSim engine(v1)
- 4 個 plugin clone
- ArchSim.uproject / Config / .gitignore
- ArchSimModelRegistry 任何 production code(這是關鍵 — agent 沒改任何 production
  來配合 test;反而 test 寫對真實行為 + 留 self-guard 註解 + RELEASE notes 公開更正)
- A1-07 SaveLoadRoundTrip test(保留 v0.1.1 內 vacuous assertion 不刪,維持 bisect
  歷史;AS-07 新 header comment 取代解釋)

---

## 2. 怎麼跑

```powershell
# 一鍵 5-leg gate(現在預設 137)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS, UE 137 tests run, GATE_EXIT=0

# 非 cuDSS host:
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 135

# AS-07 單跑
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Persistence.MaxRankCeiling; Quit" `
    -unattended -nullrhi -log
```

---

## 3. 新 token / 新 flag / 新 API

**無生產面 surface 新增。**

唯一可注意的:
- **`-ExpectedUeTests` 預設 137**(cuDSS)/ **135**(non-cuDSS)
- 新 test path:`ArchSim.Persistence.MaxRankCeiling`(同 `ArchSim.*` namespace,
  跟 `ArchSim.Persistence.SaveLoadRoundTrip` 並列)

---

## 4. 仍 deferred 的 items

從 [`HANDOFF_v0.1.2.md §4`](HANDOFF_v0.1.2.md) 對齊,AS-07 已 close,新加 AS-10:

### 1. **AS-02:A1-06 全套整合**(原 `HANDOFF_v0.1.md §4 item #1`)

- Sprint S-02,8h 預算
- First action:`Source/ArchSim/Private/ArchSimGameInstance.cpp` 加 `Tick(...)`,
  同步 member 位置進 registry → 觸發 `SetCurrentDemand` → BP 更新 `CachedUtilization`

### 2. **AS-03:A2-01 ALS pawn 接入**(原 `HANDOFF_v0.1.md §4 item #3`)

- Sprint S-02,12h 預算
- First action:`Source/ArchSim/Public/Characters/ArchSimCharacter.h` 新建,繼承
  `AAlsCharacter`;加 Enhanced Input mapping;Camera mode 預設 third-person

### 3. **AS-04:Gate 0 視覺確認**(原 `HANDOFF_v0.1.md §4 item #4`)

- 0.5h 人工
- First action:`%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe ArchSim.uproject`
  → Edit → Plugins → 確認 ALS / Prefabricator / SPUD / SUQS 4 個 Enabled

### 4. **AS-05:K1-T2 / K4 美術前置**

- Sprint S-02 / S-03,24h parallel
- First action:UE Editor → `Content/UI/Fonts/` mkdir;匯入 Noto Sans CJK TC OFL

### 5. **AS-06:SPUD UE5.5 `StructUtils` 棄用風險**

- 升 UE 5.8 前必查
- First action:查 https://github.com/sinbad/SPUD 最新 commit 是否替代 `StructUtils`

### 6. **AS-08:SPUD orchestration `RF_Transient` audit**

- 當 S-02+ 接 SPUD 真實 orchestration 時
- First action:`grep -rn "RF_Transient" Source/ArchSim/Private/Components/`;生產
  路徑不可 transient

### 7. **AS-09:non-cuDSS host gate 再驗**

- 任何 non-cuDSS 機會
- First action:non-cuDSS host 跑 `.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 135`;
  預期 GATE: PASS + UE automation: 135 tests run(F67/F67s 兩個 cuDSS-only test compile out)

### 8. **AS-10(本 release 新增):真實 `PendingRankAccumulation` ceiling test**

- AS-07 closed 但只 cover register-count 行為;真正的 `MaxRankBeforeRebaseline=96`
  bound 是 `RequestSolve` 內 `PendingRankAccumulation`(patch deactivate/reactivate 累計),
  AS-07 transient registry 沒 GameInstance 無法觸發
- Sprint S-02+,~3-4h(需要 GameInstance fixture)
- First action:
  1. 在 `Source/ArchSim/Private/Tests/` 新建 `ArchSimRebaselineTest.cpp`(或 append
     SaveLoadTest)
  2. 用 `FAutomationTestBase` 加 `GetTestFlags()` 含 `EAutomationTestFlags::EditorContext`
     讓 GameInstance 可用(否則 `RequestSolve` 在 cpp:274 early-return)
  3. 註冊 1 個 member → 對它呼叫 96 次 `DeactivateMember` + `RegisterMember` toggle 循環
  4. assert 第 96 次 toggle 後 `RequestSolve` 內 force-rebaseline branch 觸發
     (`ArchSimModelRegistry.cpp:284`);若 Registry 暴露 `bRebaselineDue` getter 或
     `OnRebaselined` delegate 則 assert 它;若無 expose,則加 `[[nodiscard]]`
     getter 在 production header(這算 production 改動,得 separate task)

---

## 5. 過程留下的教訓(durable)

僅本 release 學到的;與全域教訓無重疊:

1. **「Spec 模糊但 test 必須寫」時,不准 fabricate 行為** — AS-07 spec 寫
   「reject -1 或 rebaseline 二選一」,production 兩者都不是。Agent 正確選擇
   **Read 真實 production code → 把 test 寫成 pin 真實行為**,不偽造行為來配 spec。
   這條跟鐵則 #3 honest-verify 同一條經緯,但具體到 test 寫作層面:**test 是 production
   行為的契約 snapshot,不是 spec 的願望投影**

2. **Vacuous assertion 是 silent regression vector** — A1-07 的
   `RegisteredCount <= 96`(5 個 member 永真)在 v0.1.1 / v0.1.2 兩個 release 都
   沒被抓出來。原因:assertion 字面對(`5 <= 96`),所以 test PASS;但它根本沒
   exercise 任何邊界。**對策**:future code review 看到 ceiling-like assertion
   時,問「測值 vs 上限差幾倍?」差 10x 以上要求加 stress test 真實觸碰 ceiling

3. **Frozen-history 規則允許「在新 release 公開更正過去 release 的誤解」** —
   `HANDOFF_v0.1.1.md` / `HANDOFF_v0.1.2.md` 都保留不動(skill 鐵則),但
   `RELEASE_v0.1.3.md §6` 加「Spec correction notice」對外公佈 corrections
   並列舊 doc 路徑。**模式**:不刪舊文,但新 doc 必須交叉指出 stale 段。讀者
   讀 v0.1.1 時不會看到更正;讀 v0.1.3 時看得到。bisect 路徑保留

4. **Agent 的 self-defence 註解是 release artifact** — `ArchSimSaveLoadTest.cpp:284-286`
   的「若未來 refactor 真加上 register-count ceiling,這 test 會在第 97 個 register
   處 assert idx==96 失敗、強迫 maintainer 同步」是 maintainer-future-proof
   coding。release 收這種 comment 進去當文化 anchor,future PR review 看到類似
   pattern 會自然延續

5. **Engine source 0 行 + Production code 0 行 + Test 100% 行為 pin** 是這次
   release 的核心對稱 — 把 production 視為 frozen reference,test 視為對它的
   contract snapshot。下次有類似 spec-vs-real mismatch,套用同 pattern

---

## 6. 後續方向

### Sprint S-02 接下來(v0.2 minor 累積中)

backlog 排序:
1. **AS-10**:真實 PendingRankAccumulation ceiling test(3-4h)— 跟 AS-07 同
   區塊,寫起來 context 還在
2. **AS-02**:A1-06 full integration(8h)
3. **AS-03**:A2-01 ALS pawn(12h)
4. **AS-05**:K1-T2/K4 art(24h parallel)
5. **AS-04**:Gate 0 視覺確認(你自己 0.5h)

### 何時 bump v0.2 minor

- 當有 user-visible feature 落地(ALS pawn AS-03 = 角色操作)
- 或當 micro patches 累積到 5+ 個 backlog 全 close

### 風險區

- **AS-06 (SPUD `StructUtils`)**:UE 升 5.8 時間炸彈
- **AS-09 (non-cuDSS gate)**:單一 cuDSS host 驗;非 NVIDIA fork 可能撞 count mismatch
- **AS-10 真實 ceiling 測試**:若 production 沒 expose `bRebaselineDue` getter,
  得加 production code → 觸發另一個 release cycle

---

接手有問題:`docs/HANDOFF_v0.1.md` → `docs/HANDOFF_v0.1.1.md` →
`docs/HANDOFF_v0.1.2.md` → 本檔。
Sprint 進度:`docs/SPRINT_NOTES.md`。
