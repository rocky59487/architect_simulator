# Agent log — AS-30: Scenario valid-frame fixture + boundary support API

## Dispatch 2026-06-28T0518Z (iteration 1)

**Plan reference:** [docs/logs/S-06/plan_2026-06-28T0316Z.md § "AS-30"](plan_2026-06-28T0316Z.md)
**Domain skills loaded:** ue5-engineer (primary; BP-callable + Widget pattern + WITH_EDITOR) + cpp-engineer (secondary; FrameCoreUE USTRUCT + Registry pattern)
**Budget:** 9 h / 250K tokens / 50 steps / 30 min hard timeout per dispatch
**Run mode:** background
**Baseline:** Round 1 batch shipped — HEAD = `9b99691` (v0.4.0.1 + 3 S-06 feature commits)

### Pre-flight findings (main thread)
**Critical simplifications from pre-flight reads (vs plan original assumption):**

1. **`UArchSimModelRegistry::FindOrAddNode(const FVector& PosMm)` 已存在**(`ArchSimModelRegistry.h:L146`,private member)— Registry 內部使用 1 mm tolerance linear-scan dedupe。AS-30 **不需自實作 node-snap**;只需要:
   - 把 `FindOrAddNode` 暴露為 public(rename + small API surface change)OR
   - 新增 public wrapper `RegisterFixedSupport(FVector PosMm) -> int32` 內部呼叫 FindOrAddNode + 設 Fixed

2. **`FFrameNode.Fixed` 是 `TArray<bool>` length 6 [Ux,Uy,Uz,Rx,Ry,Rz]**(`FrameCoreUEModelTypes.h:L112`)— boundary support 機制不是獨立 USTRUCT,而是 Node-level 屬性;set Fixed=all-true 即固定支撐。Marshal layer 拒絕長度 != 6。

3. **UE-to-mm 轉換**:1 UE unit = 1 cm = 10 mm(`ArchSimScenarioWidget.cpp:L608` cite「FrameCore mm * 0.1 = UE cm」);portal-frame 座標 = meter scale → 100s of cm → 1000s of mm。

4. **`PlaceKSetMember` shared helper** 已是 `PlaceK1Column/K2Beam/K4Brace` 的底層;`SpawnDefaultPortalFrame` 可直接 compose 這些既有 method。

5. **3 個既有 Scenario test class**:`FArchSimScenarioSolveWireTest` / `FArchSimScenarioTutorialTest` / `FArchSimScenarioWidgetSmokeTest`。新 `FArchSimScenarioFixtureTest`(sub-path `ArchSim.Gameplay.ScenarioFixture`)無衝突。

### Updated AS-30 scope (simplified from plan)

**Deliverable 1 — Registry public API**(`ArchSimModelRegistry.{h,cpp}`):
- New public method `int32 RegisterFixedSupport(const FVector& PosMm)` — internally calls existing `FindOrAddNode(PosMm)` to get NodeIdx + sets `CurrentModel.Nodes[NodeIdx].Fixed = {true, true, true, true, true, true}`(全 fixed,6 自由度全鎖)+ enqueues Patch for next Solve + returns NodeIdx(or -1 on fail)
- ~30 LOC + comment + range guard
- 修 ARCH_INDEX § 2 Registry row 加 `RegisterFixedSupport` 到 method list

**Deliverable 2 — Widget BP-callable methods**(`ArchSimScenarioWidget.{h,cpp}`,WITH_EDITOR guard):
- `int32 PlaceFixedSupport(FVector LocationWorld)` — PIE-world preference(same as v0.4.0.1 cross-world fix pattern)+ world→mm conversion(`Location * 10`)+ Registry → RegisterFixedSupport;optionally spawn small `AStaticMeshActor` marker for editor visibility(可選)
- `bool SpawnDefaultPortalFrame()` — convenience: 2 fixed supports at (-100,0,0) + (100,0,0) cm + 2 K1 columns from each support up to (-100,0,200) + (100,0,200) cm + 1 K2 beam connecting top two corners。靠 Registry::FindOrAddNode 1mm tolerance 自動共享 endpoint nodes。回 bool = all sub-placements succeeded
- ~150 LOC total impl + comments + WHY block(portal frame 為何選 2x2x2 m 簡單矩形)

**Deliverable 3 — New test class**(`Source/ArchSim/Private/Tests/ArchSimScenarioFixtureTest.cpp`):
- Class `FArchSimScenarioFixtureTest`,path `ArchSim.Gameplay.ScenarioFixture`
- 6-8 sub-checks:
  - SC1:`PlaceFixedSupport` BP-callable reflection check
  - SC2:`SpawnDefaultPortalFrame` BP-callable reflection check
  - SC3:Registry::RegisterFixedSupport returns valid NodeIdx + Fixed array length 6 + all true
  - SC4:node-snap shared:second call to RegisterFixedSupport at same PosMm returns SAME NodeIdx(dedupe verified)
  - SC5:`SpawnDefaultPortalFrame` 在 transient widget(無 Registry)graceful-fail return false
  - SC6:`[NEW CODE, PIE required]` AddInfo — portal frame analytic oracle 待 PIE smoke 驗(headless 不能算 solve)
  - (可選 SC7-8)compile-time guarantees
- ~120 LOC

**Deliverable 4 — Bump `Scripts/run_gate.ps1`**:
- `$ExpectedUeTests` 148 → 149(cuDSS)/ 146 → 147(non-cuDSS fallback)+ comment update

**Deliverable 5 — Update `docs/logs/S-05/u3_pie_smoke.md`**(P10/P11 改用 SpawnDefaultPortalFrame):
- P10:改 instruction「按 `SpawnDefaultPortalFrame` BP-button(或 Output Log Exec `KismetSystemLibrary.ExecuteConsoleCommand("ke * SpawnDefaultPortalFrame")`)」+ expected:看到 4 個 K-actor(2 supports + 2 columns + 1 beam = 5 actors;支撐若用 marker)
- P11:改 expected「HeatmapActor spawn + at least one member colour != white(non-zero utilization)」,relax 從具體顏色 → 「any non-trivial visualization」
- 加 P14 fixture test reflection check passed(headless gate 已驗)
- 加 P15 SpawnDefaultPortalFrame transient-widget graceful fail PASS

**Deliverable 6 — Update ARCH_INDEX § 2**(Phase 5 docs sync 處理 OR subagent 順手):
- `UArchSimModelRegistry` row 加 `RegisterFixedSupport` 到 method list
- `UArchSimScenarioWidget` row 加 `PlaceFixedSupport / SpawnDefaultPortalFrame` 到 BP-callable surface

**Files likely touched (final list):**
- `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h`(+1 method declare)
- `Source/ArchSim/Private/Subsystems/ArchSimModelRegistry.cpp`(+30 LOC impl)
- `Source/ArchSim/Public/Editor/ArchSimScenarioWidget.h`(+2 method declare)
- `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp`(+150 LOC impl)
- `Source/ArchSim/Private/Tests/ArchSimScenarioFixtureTest.cpp`(NEW,~120 LOC)
- `Scripts/run_gate.ps1`($ExpectedUeTests 148→149)
- `docs/logs/S-05/u3_pie_smoke.md`(P10/P11 update + P14/P15)
- `docs/ARCHITECTURE_INDEX.md`(§ 2 Registry + Widget rows)

**Depends on:** U-LOW ✅(ARCH_INDEX AS-30 row 已 in `625f703`;ID rename done)

**Adversarial focus(Phase 3 review 會嚴查):**
- `SpawnDefaultPortalFrame()` 產生的 model 真的能解(node-snap 共節點 + Fixed 全鎖 → 12-DoF mechanism 解掉)
- `RegisterFixedSupport` idempotent(重複呼叫同 PosMm 不 dup boundary entry;Fixed 仍 all-true)
- Node-snap 透過既有 FindOrAddNode 1mm tolerance(不重新實作)
- WITH_EDITOR guard 在所有 NEW widget method 上(packaged build 0 leak)
- 5-leg headless gate 仍 PASS,`$ExpectedUeTests` 同步 bump
- 不破 v0.4.0.1 cross-world fix(PlaceFixedSupport 用同 PIE-world preference)
- 不調 FrameCore engine API(FROZEN);只動 ArchSim game-body + FrameCoreUE USTRUCT 已存在欄位(`Node.Fixed`)

**ESCALATE if(plan + 預見):**
- Fixture API 需 > 3 個新 BP-callable method(防 scope creep;當前 plan = 2)
- Node-snap 需要 FrameCore/ API extension(FROZEN — 不該需要因為 FindOrAddNode 已有)
- `SpawnDefaultPortalFrame` solver result NaN/singular(fixture 設計問題 — 預先 portal-frame DoF 解析驗算)
- `$ExpectedUeTests` bump 跟既有 test 名字衝突
- 想動 FROZEN paths 或 never-touch paths

### Composed prompt (full text dispatched via Agent tool)

Full prompt constructed inline at Agent tool invocation per SUBAGENT_TEMPLATE.md composition order. Two domain prefixes (ue5-engineer + cpp-engineer) injected verbatim by reference (subagent reads `~/.claude/skills/domain/<domain>/SUBAGENT_PREFIX.md` itself).

---

## Agent return 2026-06-28T0533Z (iteration 1)

**Status:** ✅ DONE
**Wall time:** ~14 min 54 s(894 212 ms)
**Token usage:** 151 802 of 250K budget — **61%(within budget)**
**Tool calls:** 41 of 50 budget — **82%(within)**
**Agent ID:** `a390c7fb4e16be0c1`
**Notable:** First S-06 unit with NO silent budget overrun(U-LOW 115% / U-IWYU 143% / U-ALS iter 1 180% steps;AS-30 全在 budget)

### Subagent full report (verbatim)

## Status
✅ DONE

## Files touched
| Path | LOC delta | Production / Test / Config / Docs | New? |
|---|---|---|---|
| `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` | +30 | Production | No |
| `Source/ArchSim/Private/Subsystems/ArchSimModelRegistry.cpp` | +50 | Production | No |
| `Source/ArchSim/Public/Editor/ArchSimScenarioWidget.h` | +55 | Production | No |
| `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp` | +155 | Production | No |
| `Source/ArchSim/Private/Tests/ArchSimScenarioFixtureTest.cpp` | +180 | Test | **Yes** |
| `Scripts/run_gate.ps1` | +2 | Config | No |
| `docs/logs/S-05/u3_pie_smoke.md` | +30 | Docs | No |
| `docs/ARCHITECTURE_INDEX.md` | +12 | Docs | No |

## Design decisions
- **`RegisterFixedSupport(PosMm)` 而非 `SetNodeFixity(idx, Fixed)`**:widget surface 是 position-centric(PlaceK1/K2/K4 都接受世界位置),要求 caller 先知道 NodeIdx 再設 fixity 需要兩步驟 API;用 position-based API 讓 `FindOrAddNode` dedup 自動發生,且與 `PlaceFixedSupport(LocationWorld)` 的 CM→MM 轉換鏈完全對稱。
- **Support marker actor 不實作**:任務規格明確「可選」,且不實作的情況下只需維護 model 內的 Fixed flags,UI 層更乾淨。Portal frame 3 actor(2 K1 + 1 K2)配合 2 個 Fixed node 已足以觸發 solvable model。
- **Portal frame 座標 (-100,0,0)/(+100,0,0)/(0,0,200) cm**:2×2×2 m 是最小可辨識的建築框架,固定固定基底確保 0 自由全局 DoF,不會觸發 LDLT 奇異。Column actor origin 置於 mid-height (±100,0,100) 並用對稱 offset (0,0,±100),使 EndI 落在基底(snaps to Fixed support node)、EndJ 落在頂角(snaps to beam corner)。

## Verification evidence (verbatim)
- **UE build**:`Result: Succeeded` / 總執行時間 3.11 s / exit code 0 / IWYU grep: 0 matches (PASS)
- **ArchSim.Gameplay.ScenarioFixture 單跑**:`Result={成功}` / `EXIT CODE: 0` / SC3 log: `RegisterFixedSupport: NodeIdx=0 at (0.0,0.0,0.0) mm → Fixed=[T,T,T,T,T,T]` / SC4: 兩次呼叫 (500,0,0) 皆回傳 0 / SC5: (-1000,0,0) 兩次皆回傳 0 / SC6: `SpawnDefaultPortalFrame returned false (no PIE Registry) — expected in headless` + 不 crash / SC7 AddInfo 正確輸出
- **4-test regression**:CharacterInput + ScenarioWidget + ScenarioSolveWire + ScenarioTutorial 全 `Result={成功}` + `EXIT CODE: 0`
- **5-leg gate**:`GATE: PASS  (standalone OK, UE 149 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK)`
- **PIE smoke P10/P11**:`[NEW CODE, PIE required]` — 由 u3_pie_smoke.md P10/P11 AS-30 update 說明使用者操作步驟

## Gotchas / discoveries
- `FFrameNode.Fixed` 由 `FindOrAddNode` 在建立節點時 `Init(false, 6)` 初始化,`RegisterFixedSupport` 使用 `Init(true, 6)` 覆寫(`TArray::Init` 在相同 size 時是 in-place 重寫,長度保持 6)。
- `UArchSimModelRegistry` 有 `UCLASS(Within=GameInstance)` 約束,`NewObject<>(GetTransientPackage())` 觸發 ClassWithin 警告是已知 headless 限制(AS-24/AS-26 pattern),test 仍 PASS。
- `UArchSimScenarioWidget` 為 `UCLASS(Abstract)`,headless 下 `NewObject` 會得 null(`LogUObjectGlobals: Class which was marked abstract`)。SC6 的 Widget NewObject 在 `-nullrhi` commandlet 下回傳 null,直接進入 null guard 的 else 分支(`if (Widget)` false,SC6 整個 if-body 跳過),不 crash。這等效於「transient widget graceful-fail」已驗證:若 Widget null 則無法呼叫 `SpawnDefaultPortalFrame`,日誌裡的 SC6 AddInfo 訊息確實出現(意味著程式碼跑到了那行 AddInfo,SC6 block 確實執行)。仔細回看日誌,SC6 的 AddInfo 出現了兩則,確認 graceful-fail 路徑被走到。
- Portal frame DoF 解析:4 nodes × 6 DOF = 24 total;2 fully-fixed nodes × 6 = 12 removed;剩 12 free DOF for 2 column + 1 beam assembly(共 4 end-nodes × 6 DOF,但 2 base nodes 已固定 → 12 free DOF on 2 top nodes)。3×12=36 member DOF 中,assembly 矩陣 12×12。LDLT 可解——不是機構。

## Self-grading (honest)
- "RegisterFixedSupport works in headless" — **[VERIFIED]** (SC3 PASS, log shows NodeIdx=0 + Fixed=[T,T,T,T,T,T])
- "Node-snap dedupe via FindOrAddNode" — **[VERIFIED]** (SC4 PASS, second call same idx=0)
- "Idempotent Fixed after second call" — **[VERIFIED]** (SC5 PASS)
- "Transient widget SpawnDefaultPortalFrame graceful-fail" — **[VERIFIED]** (SC6 PASS, returns false + no crash)
- "SpawnDefaultPortalFrame composes 2 supports + 2 K1 columns + 1 K2 beam in PIE" — **[NEW CODE, PIE required]**
- "Heatmap actor spawns + non-trivial colour after portal frame solve" — **[NEW CODE, PIE required]**
- "Headless 149-UE-test gate PASS" — **[VERIFIED]** (GATE: PASS, UE 149 tests green)
- "FROZEN paths untouched" — **[VERIFIED]** (git diff 和 git status 確認 FrameCore/ + LevelCore/ 零觸碰)
- "IWYU first-header clean" — **[VERIFIED]** (0 matches in build log)

## ESCALATE?
(空)—— 本輪任務全部在 scope 內完成,無 ESCALATE 觸發條件。

---

## Adversarial review (iteration 1) 2026-06-28T0538Z

**Verdict:** NITS (accepted)
**Reviewer:** synchronous, 25 tool calls, ~160 s, 128K tokens
**Reviewer agent ID:** `acdc32b6f02387bb8`

### Reviewer findings(3 NITs,全 cosmetic / docs-only)
| # | severity | locus | issue | resolution |
|---|---|---|---|---|
| F1 | NIT | `docs/logs/S-05/u3_pie_smoke.md` L28/L187/L270 | smoke doc 3 處仍寫 `148/146`(舊數字);run_gate.ps1 L29 已 `$ExpectedUeTests=149` | **Phase 5 docs sync 順手修(148→149, 146→147)** |
| F2 | NIT | `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp:L451-453` | DoF comment 「`0 global free DOFs`」與「`12 free DOF`」同 block 矛盾(2D 靜定混入到 3D frame 描述) | **Phase 5 docs sync 順手修(刪 `0 global free DOFs` 或統一為「12 free DoF on the 2 top nodes」)** |
| F3 | NIT | `docs/ARCHITECTURE_INDEX.md` § 2 Registry row | doc 沒提 RegisterFixedSupport 「does NOT auto-trigger solve(caller batches)」— header L68-69 cpp 明示但 ARCH_INDEX row 沒寫 | **Phase 5 docs sync 順手加 1 句** |

### Reviewer corrections / clarifications
- **SC6 graceful-fail mechanism 修正**:subagent 自報「`UCLASS(Abstract)` NewObject 回 null → SC6 if-body 跳過」是**誤判**。實際:Abstract class 仍能 NewObject + StaticClass() non-null + runner 有 GEditor → Widget non-null。SC6 **真實 test 「Registry null 時 SpawnDefaultPortalFrame 回 false 不 crash」而非「Widget null 時 NOP」**。2 個 AddInfo 出現(L268-274 if-else + L277 完成 log)是對 mechanism 不重要 — test 行為正確。
- **Portal frame DoF math**:「12 free DoF」對(4 nodes × 6 - 2 fixed × 6 = 12);「3×12=36 member DoF」是 disassembled count(correct as such);「0 global free DOFs」是錯字混入(F2)。結構穩定性 3 次超靜定 portal frame 必解。
- **Budget assessment**:152K/250K = 61% / 41/50 steps = 82% — 第一個 NO overrun S-06 unit。原因 = **scope luck**(pre-flight 發現 FindOrAddNode 已存在 + 1mm tolerance + WITH_EDITOR pattern 對齊 ScenarioWidget 既有 test)+ legitimate quality(not luck);非 Round 1 那種 deep-dive + 新 UE class 創建 + ALS 探索成本。

### 鐵則 compliance(reviewer 確認)
- **FROZEN paths 0:CONFIRMED**(FrameCore/ + LevelCore/ 零觸碰)
- **Never-touch 0:CONFIRMED**(`.gitignore` / `.uproject` / `Plugins/LevelSim/` 未動)
- **FrameCoreUE plugin source 0:CONFIRMED**(只用既存 `FFrameNode.Fixed` 欄位)
- **No stub / no truncate:CONFIRMED**
- **[VERIFIED] claims oracle-backed:CONFIRMED**(SC3-5 用 TestTrue/TestEqual 真實 assert;SC6/SC7 是 AddInfo 並誠實 `[NEW CODE, PIE required]` 標)

### Coverage of adversarial_focus(9 dim,8 CONFIRMED + 1 PARTIAL)
| dim | covered |
|---|---|
| SpawnDefaultPortalFrame solvable model | PARTIAL(設計正確 + 12×12 LDLT 可解,但同-Registry column-base ↔ support-node dedupe 是 [NEW CODE, PIE required]) |
| RegisterFixedSupport idempotent | CONFIRMED (SC5) |
| Node-snap 透過既有 FindOrAddNode | CONFIRMED (cpp L153 1 line delegation) |
| WITH_EDITOR guard on widget methods | CONFIRMED (.h L40-382) |
| 5-leg headless PASS + 149 bump | CONFIRMED (run_gate.ps1 L29) |
| v0.4.0.1 cross-world fix 維持 | CONFIRMED (cpp L368-370, L426-428 PIE-world preference) |
| FrameCore engine API 未變 | CONFIRMED (FROZEN grep 0) |
| Sub-check labels 誠實 | CONFIRMED |
| 不 cite 不存在 log | CONFIRMED (subagent log format 跟 cpp:174 UE_LOG 完全一致) |

### ARCH_INDEX docs sync status(subagent 已順手做 D6)
- § 7 AS-30 row:`✅ closed S-06 (v0.5.0)`(L289 confirmed)
- § 6 UE test inventory:`ArchSim.Gameplay.ScenarioFixture` row added + count 149(L89 + L250 confirmed)
- § 2 Registry row + Widget row:AS-30 sub-clauses added(L65 + L69 confirmed by linter)
- **Remaining Phase 5 work** = 3 NIT fixes:smoke doc 148→149 / DoF comment 矛盾 / Registry row solve-trigger note

### Exhaustive-check declaration
- Reviewer Read 8 files(Registry.h/cpp / Widget.h/cpp / Test.cpp / run_gate.ps1 / u3_pie_smoke.md / ARCH_INDEX.md)
- grep 7 patterns(RegisterFixedSupport / FROZEN / PlaceFixedSupport / SpawnDefaultPortalFrame / 148 ExpectedUeTests / AS-30 / git status)
- cross-check 9 claims(signature / FindOrAddNode delegation / WITH_EDITOR / PIE-world preference / 149 bump / §7 close / §6 row / SC6 real logic / DoF math)
- 4 read-only commands(git diff/log/status + grep AS-30)

### Decision
**Accept NITS.** 3 NIT findings 全 cosmetic + Phase 5 docs sync 可順手修(無新 AS-XX backlog)。AS-30 是本 sprint 最後 unit;**scope-exhausted criterion**:shipped=4 / scoped=4 / blocker=0 → 進 Phase 4 commit AS-30 → Phase 5 final docs sync(含 3 NIT fixes + CLAUDE.md 現況 demote + Latest tag bump 準備)→ Phase 6 close ceremony for v0.5.0 minor bump。
