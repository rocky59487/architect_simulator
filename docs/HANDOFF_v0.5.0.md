# 交接指南 — `v0.5.0` 後接手 owner

> **From:** `v0.5.0`(tag-time commit;見 `git log --oneline -2`)
> **To:** next session owner(any Claude / human)
> **Date:** 2026-06-28
> **Prior handoffs:** [`HANDOFF_v0.4.0.1.md`](HANDOFF_v0.4.0.1.md)(S-05 hotfix)+ [`HANDOFF_v0.4.0.md`](HANDOFF_v0.4.0.md)(S-05 main)+ [`HANDOFF_v3.6.0.md`](HANDOFF_v3.6.0.md)(engine pre-FROZEN anchor)
> **Release notes:** [`RELEASE_v0.5.0.md`](RELEASE_v0.5.0.md)
> **Sprint log:** [`docs/logs/S-06/manager.md`](logs/S-06/manager.md)

---

## Z-01 first action on day 1

**Run the USER-DRIVEN PIE smoke per scope contract hard gate.**

```powershell
# Step 1: confirm git state
cd E:/project/ArchSim
git log --oneline -3                  # 期望 release commit + v0.5.0 tag
git tag --sort=-creatordate | head -3 # 期望 v0.5.0 / v0.4.0.1 / v0.4.0

# Step 2: confirm 3 NIT fixes landed (Phase 5 batch into release commit)
grep "149/147" docs/logs/S-05/u3_pie_smoke.md    # 期望 3 hits (L28 / L187 / L270)
grep "Does NOT auto-trigger" docs/ARCHITECTURE_INDEX.md  # 期望 1 hit (Registry row)
grep "0 global free DOFs" Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp  # 期望 0 hits (NIT fixed)

# Step 3: confirm ALS plugin patch applied locally
grep -n "AnimationInstance.IsValid()" Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp | head -10
# 期望 7 hits incl L411 (新加的 guard)

# Step 4: re-verify 5-leg gate
.\Scripts\run_gate.ps1 -RequireOpenSees
# 期望 GATE PASS — UE 149 / OpenSees / audit 104 / CLI / standalone

# Step 5: USER-DRIVEN PIE smoke (the hard gate per scope contract)
# 開 UE Editor: E:\project\ArchSim\ArchSim.uproject
# 跟 docs/logs/S-05/u3_pie_smoke.md P1..P15 一一執行
# 特別注意 P10/P11 新行為:
#   - P10: BP-button or Output Log `ke * SpawnDefaultPortalFrame` → 3-5 actors spawn
#   - P11: HeatmapActor spawn + at least one member non-trivial colour (非白)
#   - P14: headless 149-test ScenarioFixture sub-checks PASS (用 run_gate.ps1 已驗)
#   - P15: transient widget SpawnDefaultPortalFrame graceful-fail (Registry null) PASS
```

如果 PIE smoke P1..P15 全 PASS → 在 manager.md 加一筆「v0.5.0 PIE smoke PASS confirmed YYYY-MM-DD,ready for student trial」+ 開始 S-07 scope。

如果 PIE smoke P10/P11 FAIL(SpawnDefaultPortalFrame 沒 spawn 5 actors / heatmap 沒亮)→ 走 post-publish hotfix protocol(`gh release edit v0.5.0 --prerelease` + ship v0.5.0.1)。Root cause 排查順序:
1. BP child 不存在(`Content/BP_ArchSimScenarioWidget.uasset` 缺) → 跑 `Tools/setup_pie_smoke_widget.py`
2. `Registry::Get(World)` returns null in PIE → 重 verify cross-world fix(v0.4.0.1 已修但若 BP-side 改變可能 regress)
3. `RegisterFixedSupport` + `RegisterMember` debounce timing 衝突 → manual `Registry::RequestSolve({})` 試 force kick
4. ALS character 仍 crash in PIE → 走 AS-33(BP child + GameMode.cpp swap path)

---

## 1. `v0.5.0` = 什麼

- **一句話**:v0.4 series 全部 known-residual 工作收完,讓 v0.x 從乾淨 baseline 進入 next-gen feature scope。
- **與 v0.4.0.1 的 source-line delta**:5 commits including release commit(`625f703` U-LOW / `f195746` U-IWYU / `9b99691` U-ALS / `5caa751` AS-30 / release commit at tag time)。Tracked:~30 files / ~3500 LOC additive(8 NEW files + 22 modified)。
- **整入了哪些先前 deferred items**:
  - HANDOFF_v0.4.0.1 「ALS character mesh null deref」(U-ALS 解 root cause + 2-layer fix)
  - HANDOFF_v0.4.0.1 「AS-29(actually fixture)」rename to AS-30 + 實作完成(U-LOW + AS-30)
  - HANDOFF_v0.4.0.1 「AS-28-followup IWYU validator」(U-IWYU)
  - ARCH_INDEX § 7 AS-28 closed(U-LOW 同時負責 -cnotmatch fix)
- **什麼未動**:
  - FrameCore engine FROZEN since v4.0.0(`git diff v0.4.0.1..v0.5.0 -- Plugins/FrameSolver/Source/FrameCore/` = 0 lines)
  - LevelSim core FROZEN since v2.2+1(`git diff v0.4.0.1..v0.5.0 -- Plugins/LevelSim/` = 0 lines)
  - FrameCoreUE plugin source(只用既有 `FFrameNode.Fixed` 欄位;不擴新 USTRUCT / UCLASS)
  - `Config/DefaultEngine.ini`(GlobalDefaultGameMode 仍 ArchSimGameMode,v0.4.0.1 已 revert 確認)
  - `ArchSim.uproject`(never-touch per iron rule #5)
- **v0.4.0 prerelease 狀態**:carry from v0.4.0.1(端到端 PIE 流被 cross-world bug 破)。`v0.4.0.1` + `v0.5.0` 是 canonical Scenario MVP path。

---

## 2. 怎麼跑(主要 reproduce paths)

### 一鍵驗證 release(headless,~10-15 min)
```powershell
cd E:/project/ArchSim
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex
# 期望 Result: Succeeded;`Expected ... first header` grep 0 matches

.\Scripts\run_gate.ps1 -RequireOpenSees
# 期望 GATE PASS — UE 149 / OpenSees / audit 104 / CLI / standalone
```

### 單跑 Scenario fixture test(秒級)
```powershell
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Gameplay.ScenarioFixture; Quit" `
    -unattended -nullrhi -log
# 期望 Result={成功} EXIT 0 + 7 sub-checks PASS
```

### IWYU validator self-check
```powershell
python Tools\check_iwyu_first_header.py
# 期望 「IWYU check: N files scanned, 0 violations — PASS」<1 s warmup
python Tools\test_iwyu_validator.py
# 期望 「Ran 7 tests in 0.03s — OK」
```

### Fresh checkout: apply ALS patch
```bash
cd <ArchSim repo root>
# 假設 ALS-Refactored v4.17 已安裝在 Plugins/ALS/
git apply Tools/patches/als_l400_animinstance_guard.patch
# 詳 Tools/patches/README.md
```

### PIE smoke(USER-DRIVEN,~5-10 min)
詳 [`docs/logs/S-05/u3_pie_smoke.md`](logs/S-05/u3_pie_smoke.md) P1..P15。**這是 v0.5.0 scope contract hard gate;ship 後仍需此驗證才宣告「ready for student trial」。**

---

## 3. 新 API / 新 token / 新 surface(給後續用)

### Registry public API(`Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h`)
```cpp
[[nodiscard]] int32 RegisterFixedSupport(const FVector& PosMm);
```
- Register fully-fixed support node at given FrameCore-mm position
- Internally calls `FindOrAddNode(PosMm)` for 1 mm tolerance dedupe
- Sets `FFrameNode.Fixed.Init(true, 6)`(全 6 DOF lock)
- Idempotent — 重複呼叫同 PosMm 回 same NodeIdx + Fixed 仍 all-true
- **Does NOT auto-trigger Solve** — caller batches supports + members + relies on `RegisterMember` 150 ms debounce
- Returns `int32` NodeIdx >= 0 on success / -1 on validation fail

### Widget BP-callable methods(`Source/ArchSim/Public/Editor/ArchSimScenarioWidget.h`,WITH_EDITOR)
```cpp
UFUNCTION(BlueprintCallable, Category="Scenario|Fixture")
int32 PlaceFixedSupport(FVector LocationWorld);

UFUNCTION(BlueprintCallable, Category="Scenario|Fixture")
bool SpawnDefaultPortalFrame();
```
- `PlaceFixedSupport`:world→mm(`*10`)+ PIE-world preference + Registry call;return NodeIdx
- `SpawnDefaultPortalFrame`:convenience composer 2 fixed supports + 2 K1 columns + 1 K2 beam in 2×2×2 m configuration;return bool(all sub-placements succeed)

### Character runtime asset wiring(`Source/ArchSim/Public/Characters/ArchSimCharacter.h`)
```cpp
virtual void PostInitProperties() override;
virtual void BeginPlay() override;
private:
    void LoadAlsAssetsLate();  // ALS asset loader (runtime-late timing)
```
- `LoadAlsAssetsLate`:LoadObject<UAlsCharacterSettings/UAlsMovementSettings/USkeletalMesh>() + FClassFinder<UAnimInstance> 載 4 asset
- `PostInitProperties` first attempt;`BeginPlay` fallback IFF Settings still null
- **`BeginPlay` calls helper BEFORE `Super::BeginPlay()`**(ALS L138-146 fires `ALS_ENSURE` inside Super)

### Tools 新增
- `Tools/check_iwyu_first_header.py` — IWYU first-header validator
- `Tools/test_iwyu_validator.py` — 7 fixtures pytest
- `Tools/patches/als_l400_animinstance_guard.patch` — ALS plugin null-guard
- `Tools/patches/README.md` — patches/ convention guide

### Docs 新增
- `docs/RELEASE_v0.5.0.md` — 本 release notes
- `docs/HANDOFF_v0.5.0.md` — 本檔
- `docs/IWYU_VALIDATOR.md` — IWYU 工具 1-page README

---

## 4. 仍 deferred / Pending(從 release notes 對齊)

| ID | Title | First action on day 1 |
|---|---|---|
| AS-04 | UE Editor Plugins panel visual | 開 UE Editor → Window → Plugins → screenshot panel state;非 code work |
| AS-05 | K1-T2 / K4 art assets | 3D modelling deliverable;非 code work |
| AS-06 | SPUD UE5.5 StructUtils deprecation | 🔵 deferred until UE 5.8 upgrade(SPIKE-UE5.8 NO-GO eval 2026-06-27 確認 ALS + SPUD + Prefabricator + SUQS compat 風險) |
| AS-08 | SPUD orchestration `RF_Transient` audit | wait until SPUD wired into save-game flow(currently disabled in `Config/DefaultEngine.ini`) |
| AS-09 | Re-verify gate on non-cuDSS host | 🔵 opportunistic;非 cuDSS host pass `-ExpectedUeTests 147` to `run_gate.ps1` |
| AS-29 | `run_gate.ps1` standalone PowerShell env race | LOW;workaround = direct `Plugins\FrameSolver\Standalone\build.bat` if leg [1/5] exit 1 unexpectedly |
| **AS-31 (NEW)** | S-06 cosmetic NIT bundle(IWYU README L79 NOP / hook L41 exit-code message conflation / L38 unquoted spaces / AlsCharacter.cpp L406 off-by-2) | open IWYU README:L79 刪「`cp ... # already there`」NOP行;.git/hooks/pre-commit L41 加 `if [ "$EXIT_CODE" -eq 2 ]; then echo "IWYU validator internal error"; exit 1; fi`;L38 改 `while IFS= read -r f; do ...`;AlsCharacter.cpp L406 改 comment 7 行號 |
| **AS-32 (NEW)** | ALS L411 guard upstream contribution | file ALS-Refactored issue:「`RefreshMeshProperties` L400 unguarded `AnimationInstance->MarkPendingUpdate()` when AnimationInstance null」;reference our `Tools/patches/als_l400_animinstance_guard.patch`;update Tools/patches/README.md 用 upstream issue # 替 「none filed yet」 |
| **AS-33 (NEW)** | Evaluate BP child + GameMode.cpp DefaultPawnClass swap vs current LoadObject path | review PIE smoke results 後;若 LoadObject pattern 維護成本高(每 ALS upgrade 都要重 verify path),create `Content/BP_ArchSimCharacter.uasset` via Editor + 改 `AArchSimGameMode::DefaultPawnClass` 指 BP child;test via PIE |
| **AS-34 (NEW)** | PIE smoke P10/P11 「heatmap colour」具體 oracle 化 | run u3_pie_smoke 收集 1 次 healthy heatmap baseline screenshot → 加 expected colour pattern(e.g.「base columns red / beam yellow」)+ analytic stress check |

---

## 5. 過程留下的教訓(durable;S-06 specific)

### 5.1 Pre-flight 讀現有 surface 能省大量工作量
AS-30 dispatch 前 main thread 讀 `ArchSimModelRegistry.h:L146` 發現 `FindOrAddNode` 私有 method **已存在** 1 mm tolerance node-snap → AS-30 subagent 不需重新實作 node-snap → 從 plan 9h 估降到實際 15 min wall + 152K/250K budget 內(第一個 S-06 unit 無 silent overrun)。**未來凡新 API 加 widget surface 必先 grep 既有 helper。**

### 5.2 Subagent 自報 budget 超出但「ESCALATE=None」是 mild violation
U-LOW 115% / U-IWYU 143% / U-ALS iter 1 180% steps 全 silent overrun without ESCALATE。Phase 3 reviewer 判 NIT 非 BLOCKER(因工作品質完整),但下次 dispatch prompt 應強調「~80% budget 仍未完 → 必 ESCALATE,不要 silent overrun」。S-06 AS-30 證明 scope-appropriate planning 可達 budget 內。

### 5.3 Subagent 自報 `[VERIFIED]` 必須 oracle-backed,可虛 reference 不存在 log
U-ALS iter 1 引 `Saved/Logs/ArchSim-backup-2026.06.27-07.44.22.log:L1532` 作 root cause `[VERIFIED]` evidence,但 reviewer 確認 該 log file 不存在 → 鐵則 #3 邊緣違反。Iter 2 改 `[INFERRED from code pattern]` + cite reviewer 找到的 `Saved/Logs/ArchSim-backup-2026.06.28-03.37.00.log:L916-929` 真實 log → 正確。**所有 [VERIFIED] 必須:**file 真實存在 + line 真實 contain claimed content + reviewer 自驗 reproduce 可得。

### 5.4 UCLASS(Abstract) headless NewObject 行為複雜
U-ALS iter 1 + AS-30 SC6 都遇:`UArchSimScenarioWidget` 是 `UCLASS(Abstract)`,headless `NewObject<>()` 在 UE Automation runner(GEditor exists)實際**回 non-null Widget**(StaticClass() non-null);只在 commandlet `-nullrhi -unattended` 沒 GEditor 場景才回 null。Test SC6 graceful-fail 真正測的是「Registry null path」非「Widget null path」— subagent 描述 mechanism 不對但 test 真實 work。**Future Abstract-class headless test 設計需明確區分 GEditor present vs absent path。**

### 5.5 Third-party plugin patch 應分 in-tree wiring + patch file 兩層
U-ALS 用 Fix A(in-tree ArchSimCharacter LoadObject)+ Fix B(out-of-tree ALS L411 patch file)兩層策略。優勢:
- in-tree fix 是 forward-compatible 主路徑
- patch file 是 defensive second line + 文件化 ALS upstream 應修的 bug
- Plugins/ALS/ 670 MB untracked 不污染 git history
- Fresh checkouts 透過 `git apply Tools/patches/*.patch` 一鍵 apply

`Tools/patches/README.md` 成為 third-party patch convention,未來凡需 patch 第三方 plugin 都走此 pattern。

### 5.6 Phase 5 mid-sprint 不要 demote CLAUDE.md「現況」
Phase 5 mid-sprint 只更新 ARCH_INDEX § 7 backlog ticks + § 2 class map 行內 surgical edit;CLAUDE.md「現況」block + ARCH_INDEX「Latest tag」line + RELEASE_*.md + HANDOFF_*.md 全留 Phase 6 release-hardening 統一處理。原因:mid-sprint tag 不存在,demote 現有 anchor 會留中介狀態 invalid view。

### 5.7 Mode A v0.5.0 direct minor bump 是合理 user-explicit 選項
Phase 6 SKILL.md「Mode A auto-trigger 須 ≥ 2 patches」是預設 trigger,但 user 在 scope contract Tier 2 round 1 直接選 Mode A (single v0.5.0 minor) 也是合理 mode。**未來 Mode 決策應尊重 scope contract 的 explicit 選擇優先於 auto-trigger heuristic。**

---

## 6. 後續方向(無排序)

### Next major (e.g. v0.6 or v1.0):
- **完整 PIE-driven Scenario MVP playable surface** — 真實 student trial(15-20 students × 10 min each)
- **`UFrameInteractiveSubsystem` integration** — Scenario widget 改用 `UFrameInteractiveSubsystem`(GameInstanceSubsystem wrapping `frame::ReSolveSession`)取代 stateless library;60 fps interactive re-solve
- **更豐富的 fixture library** — `SpawnTrussFrame` / `SpawnCantileverTip` / `SpawnSimpleBeamWithLoad` 給不同教學情境
- **PCG-based fixture generator**(UE 5.7 Spatial Noise + Landscape Spline)— 自動生成 building footprint 給 BESO/topology demo

### Minor (e.g. v0.5.x):
- **AS-31 cosmetic cleanup** — 4 items / batched / ~1h work / opportunistic
- **AS-32 ALS upstream contribution** — file issue / track in Tools/patches/README
- **AS-33 BP child evaluation** — if maintenance cost of LoadObject path proves high
- **AS-34 heatmap oracle** — 1 healthy baseline run + expected pattern document

### 風險區(還沒驗證,但下次 release 必看):
- **PIE smoke 真實結果** — v0.5.0 ship 時 P10/P11 仍 [NEW CODE, PIE required];USER 必須跑;若 FAIL 走 v0.5.0.1 hotfix protocol
- **ALS upstream 升級行為** — 若 ALS-Refactored 升 v4.18+ → Tools/patches/als_l400_animinstance_guard.patch 可能不再 apply;需 verify L398-415 contextual lines 是否仍相符
- **UE 5.8 upgrade window** — SPIKE-UE5.8-eval (S-05) NO-GO + AS-06 (SPUD StructUtils deprecation) blocker;若 UE 5.8 LTS 釋出該重 spike
- **headless `-nullrhi -unattended` ALS content mount timing** — iter 2 PostInitProperties LoadObject 在 commandlet 仍回 null(預期 Warning);BeginPlay fallback 是可靠 PIE-only path;若未來想加 headless integration test 需 wait-for-mount 機制

### Out of /work hub scope (need separate planning):
- Voice prompt TTS 整合(`OnVoicePromptShouldPlay` BlueprintImplementableEvent 已存在 v0.4.0 SPIKE-Scenario-u3,但無 TTS SDK 連)
- 多 K-set 預設 fixture library 教材化
- BESO / topology optimization Scenario module(超 v0.x 範圍)

---

接手有問題:`docs/HANDOFF.md` → `docs/HANDOFF_v0.4.0.md` → `docs/HANDOFF_v0.4.0.1.md` → 本檔。

Scenario-specific:`docs/logs/S-05/u3_pie_smoke.md`(P1..P15 user-driven gate)+ `docs/logs/S-06/scope_*.md` + `manager.md`。

ALS-specific:`Tools/patches/als_l400_animinstance_guard.patch` + `Tools/patches/README.md` + ARCH_INDEX § 2 `AArchSimCharacter` row S-06 U-ALS sub-paragraph。

IWYU-specific:`docs/IWYU_VALIDATOR.md` + `Tools/check_iwyu_first_header.py` + `Tools/test_iwyu_validator.py`。

🤖 Generated with [Claude Code](https://claude.com/claude-code)
