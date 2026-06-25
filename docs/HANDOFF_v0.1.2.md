# 交接指南 — `v0.1.2` 後接手 owner

> `v0.1.2` 在 2026-06-25 發布,tag `v0.1.2` 接在 `v0.1.1` 之後。
> 主交接文連結:[`HANDOFF_v0.1.md`](HANDOFF_v0.1.md) → [`HANDOFF_v0.1.1.md`](HANDOFF_v0.1.1.md)
> → 本檔。本檔只記 v0.1.1 → v0.1.2 新增的內容(即 AS-01 closure + 一個新
> deferred AS-09)。

---

## 1. `v0.1.2` = 什麼

一句話:**`Scripts/run_gate.ps1` 兩處 surgical edit,把 `ArchSim.*` namespace
正式收進 5-leg gate;鐵則 #2 對遊戲本體 test 強制生效;engine 源 0 行動。**

- **動到 1 個檔**:`Scripts/run_gate.ps1`(2 處 Edit)
  - line 70:filter `'FrameCore;'` → `'FrameCore+ArchSim;'`
  - line 29:`$ExpectedUeTests` 135 → 136;cumulative-release comment 末尾追加
    v0.1.1 ArchSim test 註腳
- **新增 2 個檔**:`docs/RELEASE_v0.1.2.md` + 本檔
- **更新 1 個檔**:`README.md`(加 v0.1.2 patch status block 於 v0.1.1 block 上方)
- engine 源 0 行動(鐵則 #1)/ FROZEN markers + 既有 release breakdown 註解全保留

### 整入了哪些 v0.1.1 deferred items

- **AS-01**(`HANDOFF_v0.1.1.md §4 item AS-01`)→ ✅ **closed**。`run_gate.ps1`
  filter chain 真實生效,UE Automation `+`-separated syntax 不需 fallback。

### 什麼未動

- FrameCore engine(v4.0.0 FROZEN)
- LevelSim engine(v1)
- 4 個 plugin clone
- Source/ArchSim/ 全部(test source 跟 v0.1.1 bit-identical)
- ArchSim.uproject / Config / .gitignore

---

## 2. 怎麼跑

```powershell
# 一鍵 5-leg gate(現含 ArchSim.*)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS, UE 136 tests run, GATE_EXIT=0

# 非 cuDSS host:
.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 134
```

---

## 3. 新 token / 新 flag / 新 API

**無。** v0.1.2 純是 gate driver 編輯,不開新 surface。

唯一可注意的:**`run_gate.ps1 -ExpectedUeTests` 參數現在的預期值是 136(cuDSS)
/ 134(non-cuDSS)**,以後加新 ArchSim test 都要同步 bump。

---

## 4. 仍 deferred 的 items

從 [`HANDOFF_v0.1.1.md §4`](HANDOFF_v0.1.1.md) 對齊,AS-01 已 close,其餘前進:

### 1. **AS-02:A1-06 全套整合**(原 `HANDOFF_v0.1.md §4 item #1`)

- Sprint S-02,8h 預算
- First action:`Source/ArchSim/Private/ArchSimGameInstance.cpp` 加 `Tick(...)`,
  每 frame 把所有 `UArchSimMemberData` 的位置同步進 registry → 觸發 `SetCurrentDemand`
  → BP 更新 `CachedUtilization`

### 2. **AS-03:A2-01 ALS pawn 接入**(原 `HANDOFF_v0.1.md §4 item #3`)

- Sprint S-02,12h 預算
- First action:`Source/ArchSim/Public/Characters/ArchSimCharacter.h` 新建,繼承
  `AAlsCharacter`;加 Enhanced Input mapping;Camera mode 預設 third-person

### 3. **AS-04:Gate 0 視覺確認**(原 `HANDOFF_v0.1.md §4 item #4`)

- 0.5h 人工
- First action:`%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe ArchSim.uproject`
  → Edit → Plugins → 確認 ALS / Prefabricator / SPUD / SUQS 4 個 Enabled →
  `docs/SPRINT_NOTES.md` L141-142 兩個 `[ ]` 打勾

### 4. **AS-05:K1-T2 / K4 美術前置**(原 `HANDOFF_v0.1.md §4 item #5`)

- Sprint S-02 / S-03,24h parallel
- First action:UE Editor → `Content/UI/Fonts/` mkdir;匯入 Noto Sans CJK TC OFL

### 5. **AS-06:SPUD UE5.5 `StructUtils` 棄用風險**(原 `HANDOFF_v0.1.md §4 item #6`)

- 升 UE 5.8 前必查
- First action:**不要** UE 5.7 → 5.8 直接升;查 https://github.com/sinbad/SPUD
  最新 commit 是否替代 `StructUtils`

### 6. **AS-07:A1-07 MaxRank=96 真實 stress test**

- Sprint S-02+,~2h
- First action:`Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp` 末尾加
  `IMPLEMENT_SIMPLE_AUTOMATION_TEST FArchSimMaxRankCeilingTest` test path
  `ArchSim.Persistence.MaxRankCeiling`;loop 註冊 97 個 member;assert 第 97 個
  觸發 rebaseline 或被拒

### 7. **AS-08:SPUD orchestration `RF_Transient` audit**

- 當 S-02+ 接 SPUD 真實 orchestration 時
- First action:`grep -rn "RF_Transient" Source/ArchSim/Private/Components/`;
  生產路徑不可 transient

### 8. **AS-09(本 release 新增):non-cuDSS host gate 再驗**

- 任何 non-cuDSS 機會
- First action:在 non-cuDSS host 上 git checkout v0.1.2、跑
  `.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 134`;預期 GATE: PASS
  + UE automation: 134 tests run(F67/F67s 兩個 cuDSS-only test compile out)。
  若 count 不是 134,診斷 ArchSim test 是否在 non-cuDSS build 中存在(預期存在,
  因為 ArchSim.Persistence.SaveLoadRoundTrip 不依賴 cuDSS)

---

## 5. 過程留下的教訓(durable)

僅本 release 學到的:

1. **UE Automation filter 支援 `+`-separated chain** — `'FrameCore+ArchSim;'`
   匹配所有以 `FrameCore` 或 `ArchSim` 開頭的 namespace,**不需要點號**
   (沒有 `'FrameCore.+ArchSim.;'` group syntax 這種東西)。原本 plan 寫的
   fallback 形式不需要

2. **`run_gate.ps1` 編輯時 line 29 那段歷史 comment 是大段一行** — 整個
   release breakdown 是該行末尾的單行 comment,**不是多行 here-doc**。所以
   edit 必須 append 到那行末尾,不能換行。Edit tool 用 string match 直接抓
   `[int]$ExpectedUeTests = 135` 改 `= 136` + 在最後加新句最穩

3. **micro tag cadence 是有用的** — 30 分鐘工作 + 5 分鐘 release-hardening =
   v0.1.2。把每個 deferred item closure 都當成獨立 release 讓 backlog
   永遠收斂、release notes 永遠 single-issue clean。代價只是「多一個 tag」

4. **`Scripts/run_gate.ps1` 不在 FROZEN zone** — 鐵則 #1 specifically 對
   `Plugins/FrameSolver/Source/FrameCore/`。Gate driver 是 release 工程的一部分,
   每 release 演進(57 → 72 → 96 → 120 → 135 → 136 across v2.x 跟 v3.x)是正常的

---

## 6. 後續方向

### Sprint S-02 接下來(v0.2 minor 累積中)

backlog 排序(我會逐個給 prompt):
1. **AS-07**:A1-07 MaxRank stress(2h)— 跟 SaveLoad test 同一個檔,寫起來快
2. **AS-02**:A1-06 full integration(8h)
3. **AS-03**:A2-01 ALS pawn(12h)
4. **AS-05**:K1-T2/K4 art(24h parallel)
5. **AS-04**:Gate 0 視覺確認(你自己 0.5h)

### 何時 bump v0.2 minor

- 規則:當 micro patches 累積到「整合性 feature」 — 例如 ALS pawn(AS-03)落地後,
  因為角色操作是看得見的玩法躍升,值得 minor bump
- 在那之前繼續 v0.1.x patch 累積

### 風險區

- **AS-06 (SPUD `StructUtils`)**:UE 升 5.8 是時間炸彈
- **AS-09 (non-cuDSS gate)**:目前 gate 在單一 cuDSS host 驗;若有非 NVIDIA
  貢獻者 fork 此 repo 跑 gate,可能撞 count mismatch

---

接手有問題:`docs/HANDOFF_v0.1.md` → `docs/HANDOFF_v0.1.1.md` → 本檔。
Sprint 進度:[`docs/SPRINT_NOTES.md`](SPRINT_NOTES.md)。
