# Agent log — AS-36-u1: PlaceKSetMember 雙柱同 node-pair root-cause + fix + test

## Dispatch 2026-07-02T0030Z (iteration 1)

**Plan reference:** docs/logs/S-08/plan_2026-07-02T0019Z.md § "AS-36-u1"
**Domain skills loaded:** ue5-engineer (primary), cpp-engineer (secondary)
**Budget:** 3h / 250K tokens / 100 tool calls / 30min timeout
**Dispatch mode:** foreground (serial flow)

### Pre-flight reads(main thread,dispatch 前)

- `docs/ARCHITECTURE_INDEX.md` § 2 / § 7 / § 8 — AS-36 在 backlog 表;無等價 surface 重複風險;gate 數字現況 149 (cuDSS) / 147 (non-cuDSS) + leg 6 PIE(`Scripts/run_gate.ps1:30-31` 實檔確認,SKILL_CONFIG 的 135/133 為 stale)
- `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp`(846 行全讀)— `PlaceKSetMember` Step 2 (L216-232) spawn 裸 `AActor::StaticClass()`,**無 RootComponent**
- `Source/ArchSim/Private/Subsystems/ArchSimModelRegistry.cpp:195-201` — `RegisterMember` 端點計算 `Comp->GetOwner()->GetActorTransform().TransformPosition(Comp->EndIOffsetUE)` → transform 依賴 actor RootComponent
- git churn check:widget 檔最近 5 commits = v0.5.0 release / AS-30 / AS-28 / u3 / u2,本 sprint 無並行改動
- **主假說(main thread 提供給 agent 驗證,非結論)**:裸 AActor 無 RootComponent → `SpawnActor` 的 FTransform Location 被吞(S-01 已知踩雷,SPRINT_NOTES L174 v0.1.1 test 端同 bug)→ `GetActorTransform()` 恆 identity → ColA/ColB 端點都退化為 raw offsets `(0,0,∓100)/(0,0,±100)` cm → 同 node pair;柱子構成 floating substructure(不接支承)→ LDLT rank-deficient → heatmap 永不 spawn。此假說同時解釋 SC2b log `Member[0] I=2 J=3 / Member[1] I=2 J=3`。
- 次假說(HANDOFF 原推測):offset 計算錯誤 / FindOrAddNode 1mm dedup 錯誤合併。

### Composed prompt

完整 prompt 以下列結構送出(domain prefix 兩份 verbatim 全文注入 Agent call;本 log 以路徑引用以免重複 ~640 行,內容 = 檔案當下全文,無摘要):

1. role(ue5-engineer, repo root, 中文回報)
2. 鐵則 verbatim(FROZEN ×2 / 不動 protected files / no `git add -A` / 不 commit / 6-leg gate `Scripts\run_gate.ps1 -RequireOpenSees`,ExpectedUeTests 現值 149/147 / honest verify)
3. Top-tier discipline verbatim(NO STUBS / NO HALF-FINISH / READ BEFORE WRITE / PIN ACTUAL BEHAVIOR / EDGE CASES / WHY comments)
4. Architecture index pointer(§2 §6 §7 §9)
5. Baseline:S-08 / v0.5.1 @ `4567c40` / branch main / 0 commits since tag
6. Domain prefixes(verbatim inject):
   - `~/.claude/skills/domain/ue5-engineer/SUBAGENT_PREFIX.md`(310 行)
   - `~/.claude/skills/domain/cpp-engineer/SUBAGENT_PREFIX.md`(329 行)
7. Unit spec:AS-36-u1 任務全文(症狀 / 兩假說 / 5 deliverables / constraints / ESCALATE triggers / adversarial focus)— 見下方 Agent prompt unit-spec 段落副本
8. Verification:literal commands(UE build / 單測 / run_pie_gate / 6-leg gate)+ host 紀律(勿平行 build、log 讀 Saved\Logs\ArchSim.log、PS5.1 NativeCommandError、CJK Result={成功})
9. Reporting format(Status / Files touched / Design decisions / Verification evidence / Gotchas / Self-grading / ESCALATE?)
10. ABSOLUTELY NOT 清單

#### Unit-spec 段落副本(送出內容之核心,便於 review 對照)

- 症狀:v0.5.1 commandlet PIE(leg 6)SC2b log `Member[0] I=2 J=3` + `Member[1] I=2 J=3`(兩柱共用 node pair)→ LDLT rank-deficient → solve silently fails → HeatmapActor 不 spawn。
- Deliverable 1:root-cause 數值 trace(每 member 每端點 world 座標逐步算出 + node 表 before/after,file:line 佐證;兩假說擇真,或找出第三真因)
- Deliverable 2:production fix(最小改動;若主假說成立,參考 S-01 test 端修法 `NewObject<USceneComponent>` + `SetRootComponent` + `SetActorLocation`,見 `ArchSimSaveLoadTest.cpp`)
- Deliverable 3:headless regression test(擴充 `ArchSimScenarioFixtureTest.cpp` 或新檔):(a) 兩 member 不同位置 → Registry model 內 node pair 相異 + node count 正確(headless `NewObject<UArchSimModelRegistry>()` pattern);(b) actor location 回歸斷言(place 後 `GetActorLocation()` == 指定位置)。新 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` 則 bump `run_gate.ps1` `$ExpectedUeTests` 149→150(L31 註解 147→148 同步)
- Deliverable 4:commandlet PIE 證據:`run_pie_gate.ps1` 後 log SC2b 顯示相異 node pair + HeatmapActor spawn 訊號
- Deliverable 5:誠實揭露:fix 影響所有 K-set placement 路徑;S-06 user-driven P10/P11 宣稱需人工 re-verify(本 session 無人在 editor,留 backlog 註記)
- ESCALATE:root cause 指向 FrozenCore / 需動 .uproject / leg 6 因 fix 不穩且 budget 內無法收斂
- 越界禁令:PIE smoke test 的 DEFINE 順序 NIT / run_pie_gate stderr NIT 屬 NITS-u1,即使看到也不修

## Agent return 2026-07-02T0052Z (iteration 1)

**Status:** ✅ DONE(self-reported;待 Phase 3 review)
**Wall time:** 15m 23s(of 30min budget)
**Token usage:** 160,627 of 250K budget
**Tool calls:** 32 of 100 budget

### Full report (verbatim from agent)

## Status
✅ DONE — AS-36-u1 PlaceKSetMember 雙柱同 node-pair 問題已 root-cause 確認、production fix 落地、headless regression test 新增、commandlet PIE SC2b 確認相異 node pair + HeatmapActor spawn、6-leg gate PASS。

## Root cause

**假說擇真:主假說完全成立,次假說排除。**

**數值 trace(逐步):**

`SpawnDefaultPortalFrame()` 在 `ArchSimScenarioWidget.cpp:490-499` 呼叫:
- `PlaceKSetMember(FVector(-100, 0, 100), (0,0,-100), (0,0,+100), "K1-ColA")`
- `PlaceKSetMember(FVector(+100, 0, 100), (0,0,-100), (0,0,+100), "K1-ColB")`

**Step 2(L220-223 舊版): `SpawnActor<AActor>(AActor::StaticClass(), FTransform(ZeroRot, LocationWorld, OneScale))`**

`AActor` 基類無 `RootComponent`。UE 行為: 無 RootComponent 的 actor 的 SpawnActor FTransform Location **靜默丟棄**(`GetActorTransform()` 恆回 `FTransform::Identity`)。

**RegisterMember(L197-203)端點計算:**
```
ActorT = Identity (因無 RootComponent)
WorldIUE = Identity.TransformPosition(0,0,-100) = (0, 0, -100) cm
WorldJUE = Identity.TransformPosition(0,0,+100) = (0, 0, +100) cm
→ 不論 actor 原本在 (-100,0,100) 或 (+100,0,100),結果都一樣!
```

**支承先放的 node 索引:**
- SupportA `(-1000,0,0) mm` → Node 0 (Fixed)
- SupportB `(+1000,0,0) mm` → Node 1 (Fixed)

**ColA(預期位置 (-100,0,100) cm 但被靜默丟棄):**
- `EndI: (0,0,-100) cm → (0,0,-1000) mm` → FindOrAddNode = Node 2 (新)
- `EndJ: (0,0,+100) cm → (0,0,+1000) mm` → FindOrAddNode = Node 3 (新)
- Member[0]: I=2, J=3

**ColB(預期位置 (+100,0,100) cm 但被靜默丟棄 → 同 Identity):**
- `EndI: (0,0,-100) cm → (0,0,-1000) mm` → FindOrAddNode **命中 Node 2**!
- `EndJ: (0,0,+100) cm → (0,0,+1000) mm` → FindOrAddNode **命中 Node 3**!
- Member[1]: I=2, J=3 ← 與 ColA 完全相同!

**Commandlet log 佐證(v0.5.1 HANDOFF §4):**
```
SC2b: Member[0] I=2 J=3
SC2b: Member[1] I=2 J=3   ← 完全對應以上計算
```

→ 兩柱共用同一 floating substructure,12 free DOF 無約束 → LDLT rank-deficient → bSingular → HeatmapActor 永不 spawn。

**Node 表(before/after fix):**

| | Before fix | After fix |
|---|---|---|
| Node 0 | (-1000, 0, 0) mm Fixed | (-1000, 0, 0) mm Fixed |
| Node 1 | (+1000, 0, 0) mm Fixed | (+1000, 0, 0) mm Fixed |
| Node 2 | (0, 0, -1000) mm Free | (-1000, 0, 2000) mm Free |
| Node 3 | (0, 0, +1000) mm Free | (+1000, 0, 2000) mm Free |
| Member[0] | I=2, J=3 (degenerate) | I=0, J=2 (ColA base→top) |
| Member[1] | I=2, J=3 (degenerate) | I=1, J=3 (ColB base→top) |
| Member[2] | I=2, J=3 (degenerate) | I=2, J=3 (Beam top-left→top-right) |

**次假說(FindOrAddNode 或 EndIOffsetUE 計算錯誤)排除**: FindOrAddNode 的 1mm dedup 完全正確,它只是在錯誤輸入(兩個 actor 都回 Identity)下正確地 dedup 了。EndIOffsetUE/EndJOffsetUE 數值也正確,問題在 transform 被丟棄。

## Files touched

| Path | LOC delta | Production / Test / Config / Docs | New? |
|---|---|---|---|
| `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp` | +31 | Production | No |
| `Source/ArchSim/Private/Tests/ArchSimScenarioFixtureTest.cpp` | +136 | Test | No |

## Design decisions (non-obvious only)

1. **Spawn at Identity then graft root**(vs. spawn with `FTransform(loc)` 或換 class): `SpawnActor<AActor>(AActor::StaticClass(), FTransform(loc))` 在 UE 5.7 behaviorally 等同 spawn at Identity when no RootComponent — 所以光換 FTransform 沒用。最小改動:spawn Identity → NewObject `USceneComponent` → RegisterComponent → SetRootComponent → SetActorLocation。這與 S-01 完全相同 pattern(`ArchSimSaveLoadTest.cpp:L128-134`)。

2. **SC8 用 lambda 而非全域 helper**: lambda 保持 SC8 self-contained。

3. **SC8 不在 `WITH_EDITOR` guard 內**: SC8 的驗證(用 `UArchSimModelRegistry::RegisterMember` + `USceneComponent`)不需要 GEditor,可以在 headless 跑 → 標準 automation leg 2 執行。

4. **ExpectedUeTests 不變**: SC8/SC9 是擴充既有 `FArchSimScenarioFixtureTest` 的 sub-check,不是新的 `IMPLEMENT_SIMPLE_AUTOMATION_TEST`,所以 `$ExpectedUeTests` 不需 bump。

## Verification evidence (verbatim)

**UE build:**
```
Result: Succeeded
Total execution time: 14.54 seconds
Build.bat exit: 0
```

**Fixture headless test (ArchSim.Gameplay.ScenarioFixture):**
```
SC8: Member[0] I=0 J=1  Member[1] I=2 J=3
SC8 [VERIFIED]: node pairs are distinct — AS-36 fix active.
SC9: SetActorLocation(-100.0,0.0,100.0) → GetActorLocation=(-100.0,0.0,100.0)
SC9 [VERIFIED]: SetActorLocation → GetActorLocation correct after USceneComponent root graft.
Test Completed. Result={成功}
**** TEST COMPLETE. EXIT CODE: 0 ****
```

**Leg 6 PIE smoke SC2b(fix 後 commandlet PIE):**
```
SC1 [VERIFIED]: SpawnDefaultPortalFrame() succeeded in PIE.
SC2: Model.Nodes.Num() = 4 (expected 4) / Model.Members.Num() = 3 (expected 3)
SC2b: Node[0] Pos=(-1000.0,0.0,0.0) mm Fixed=1
SC2b: Node[1] Pos=(1000.0,0.0,0.0) mm Fixed=1
SC2b: Node[2] Pos=(-1000.0,0.0,2000.0) mm Fixed=0
SC2b: Node[3] Pos=(1000.0,0.0,2000.0) mm Fixed=0
SC2b: Member[0] I=0 J=2 ... active=1  ← ColA 相異!
SC2b: Member[1] I=1 J=3 ... active=1  ← ColB 相異!
SC2b: Member[2] I=2 J=3 ... active=1  ← Beam
SC4 [VERIFIED]: AFrameUtilizationHeatmapActor found in PIE world after solve.
```

**6-leg gate(完整):**
```
[1/6] standalone: ALL PASS (failures=0) (exit 0)
[2/6] UE automation: 149 tests run, exit code 0 (expected >= 149)
[3/6] OpenSees compare: PASS (exit 0)
[4/6] linear deep audit: PASS failures=0 checks=104 (exit 0)
[5/6] CLI round-trip: ALL PASS (failures=0) (exit 0)
[6/6] PIE smoke: PASS (exit 0; screenshot=33177 bytes)
      Result={成功} Name={PortalFrameSmoke} Path={ArchSim.PIE.PortalFrameSmoke}
GATE: PASS
```

## Gotchas / discoveries

1. **Screenshot 大小從 15497 → 33177 bytes**: 修前 HeatmapActor 沒有 spawn(bSingular),截圖只有空世界;修後包含可見幾何體。
2. **`FlushAndStartSession failed: invalid model: no nodes`** 仍在 PIE log 出現:是 debounce 設計預期(第一次 solve 在 model 完整前觸發,150ms 後第二次成功),不是 bug。
3. SC8 之前時間戳的 TestNotNull error 屬 FrameCore.UE.* 批次 CDO 階段另一問題,非本 test 範圍。
4. **user-driven PIE 理論上也一直是壞的**: production bug 而非 commandlet-only。所有透過 `PlaceKSetMember` 的路徑(K1/K2/K4/portal frame)全部受影響,fix 全修。

## Self-grading (honest)

- Root cause 數值 trace: **[VERIFIED]** — commandlet log 完全對應計算
- Production fix(USceneComponent root graft): **[VERIFIED]** — build 成功,PIE SC2b 相異 node pair,HeatmapActor spawn 確認
- SC8 node-pair uniqueness regression: **[VERIFIED]**
- SC9 actor-location regression: **[VERIFIED]**
- SC4 HeatmapActor spawn: **[VERIFIED]**
- 6-leg gate: **[VERIFIED]** — GATE: PASS, exit 0

## 誠實揭露

(i) Fix 影響所有 K-set placement 路徑(K1/K2/K4/SpawnDefaultPortalFrame)— 行為只有「正確」和「之前錯誤」的差別。
(ii) S-06 user-driven P10/P11 宣稱需人工 re-verify(backlog 註記)。
(iii) user-driven PIE 確認一直是壞的:production bug 並非 commandlet-only,v0.5.1 以前所有多柱 user-driven PIE session 都受影響。

## ESCALATE?
- None

---

## Adversarial review (iteration 1) 2026-07-02T0100Z

**Verdict:** NITS(6 findings:1 HIGH doc-precision / 2 MEDIUM / 3 LOW;無 BLOCKER)

**Reviewer evidence(摘要;完整輸出見 dispatch 系統記錄):**

| # | severity | issue | 處置 |
|---|---|---|---|
| 1 | HIGH | Gotcha #3 對 backup log 中 SC8 TestNotNull error 的說明語焉不詳(該 error 屬完整批次 pre-pass CDO 階段,非 ScenarioFixture 00.41.20 本身執行;test 自身 Result={成功}) | 本 log 此處補精確說明(即此行);release notes 沿用精確版 |
| 2 | MEDIUM | SC8 `bPairsDiffer` OR 邏輯單獨看太寬鬆(I 異 J 同仍過);但同 test 的 `Node.Num()==4` assert 已提供「完全相異」強保證 | → AS-38(LOW backlog):comment 說明 node-count assert 才是強保證 |
| 3 | MEDIUM | 「未申報 touched」`docs/ARCHITECTURE_INDEX.md`(+37/-10)+ `docs/logs/S-07/manager.md`(+213) | **主對話查證:誤歸因。** 兩檔 diff 內容 = S-07 close 的 v0.5.1 release 記錄(描述 commit `4567c40` 的文字不可能在該 commit 內),為上 sprint 遺留之未 commit 髒檔,非 subagent 所改。轉 Phase 4 隨本輪 release 收 commit(獨立 docs commit 或揭露後併入) |
| 4 | LOW | LOC 申報偏差(widget +31 宣稱 vs +41 diff;test +136 vs +250 diff 含空行) | log-only 註記;不影響正確性 |
| 5 | LOW | 報告內 `L220-223` 行號因 fix 插入已位移(現 L240-243) | log-only 註記 |
| 6 | LOW | SC9 只驗 UE 引擎 SetActorLocation round-trip,未走 PlaceKSetMember production path(test comment 已誠實揭露此侷限) | → AS-38 併入:SC9 可強化為直接呼叫 PlaceKSetMember |

Reviewer 另列 missed edge cases:(a) 兩柱 <1mm 間距 dedup(FindOrAddNode 設計容差,記錄即可);(b) `check(Root)` shipping build strip 後 `NewObject` null 無保護 → **AS-38 併入**(改 if-guard + log + destroy);(c) SC8 cleanup 無 ForceGarbageCollection(2 actors 風險低,記錄)。

鐵則 compliance(reviewer 以 git diff 驗證):FROZEN ×2 = 0 行 CONFIRMED;never-touch CONFIRMED;越界禁令(PIE smoke DEFINE / run_pie_gate / run_gate)0 改動 CONFIRMED;[VERIFIED] 全數有 log/檔案 oracle(149 tests、SC8/SC9/SC2b/SC4 log 行、screenshot 33177 bytes 實檔)CONFIRMED。Adversarial focus 6 維全 cover。

**NITS logged:**
- AS-38(new backlog, LOW):PlaceKSetMember `check(Root)` → shipping-safe null guard;SC8 comment 補 node-count 強保證說明;SC9 強化為走 PlaceKSetMember production path。

**Decision:** Accept with backlog item opened。Advance to Phase 4(release-hardening)。S-07 遺留兩髒檔一併交 Phase 4 處理。

---

## Phase 4: Release-hardening 2026-07-02T0115Z

**Target tag:** v0.5.2(annotated,local;publish 待 user)
**Commit:** `2fb0f4e`
**Files committed:** 10 files / +1180 / -13(widget fix + fixture test + S-07 遺留 docs ×2 + S-08 logs ×4 + RELEASE/HANDOFF_v0.5.2)

### Release-hardening 摘要

- Phase 1 七 agent 審計以 /work Phase 3 adversarial review 取代(per-unit 模式)。
- Sanitize sweep:agent log 內 2 行 `C:\Users\<user>\...` username 洩漏 → 改 `~/.claude/...`;commit 全集合 grep(wmc02/AppData/tmp\claude/token patterns)0 hit。
- Cross-doc 數字一致性:149 UE tests / 104 audit / F1..F71 / screenshot 33177 bytes 在 RELEASE / HANDOFF / agent log / run_gate.ps1 全一致。
- FROZEN integrity:staged 集合無任何 FrameCore / LevelCore 路徑。
- Gate:6-leg PASS(unit 完成時);post-gate delta docs-only,依此免重跑(揭露於 commit message)。
- S-07 遺留 docs 收編 + housekeeping 揭露(RELEASE_v0.5.2.md § Housekeeping)。

### Publish commands (user runs these)

```powershell
git push origin main
git push origin v0.5.2
gh release create v0.5.2 --title "v0.5.2 — PlaceKSetMember node-pair degeneration fix (AS-36)" --notes-file docs/RELEASE_v0.5.2.md
```

(本 section 寫於 release commit 之後,將隨下一輪 commit 收錄 — 專案既有 cadence,見 HANDOFF_v0.5.2 教訓 #2。)
