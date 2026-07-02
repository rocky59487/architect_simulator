# Agent log — AS-37-u2: AS-37 (a)+(b-1) 文件化 + test-harness sidestep helper

## Dispatch 2026-07-02T0300Z (iteration 1)

**Plan reference:** docs/logs/S-08/plan_2026-07-02T0019Z.md § "AS-37-u2"(條件 unit;user 2026-07-02T0200Z 選 (a)+(b-1) 縮小版)
**Domain skills loaded:** ue5-engineer (primary), cpp-engineer (secondary)
**Budget:** 2h / 200K tokens / **90 tool calls(校準後)** / 25min timeout
**Dispatch mode:** foreground(serial flow)

### Pre-flight reads(main thread)

- AS-37-u1 查證結論全套(同 sprint,agent_AS-37-u1.md):crash 鏈 + commandlet-only + (b-1) 選項定義(~30 LOC test-side helper)
- 既有 harness:`Source/ArchSim/Private/Tests/ArchSimPieHarness.h/.cpp`(AS-13,`GEngine->GetWorldContexts()` pattern)— helper 的自然歸屬地,避免重複 surface
- sidestep 現場:`ArchSimPortalFramePIESmokeTest.cpp`(AS-35-u1 的 test-local `WorldSettings->DefaultGameMode = AGameModeBase::StaticClass()` override;NITS-u1 reorder 後行號已變,agent 需重讀)
- ARCH_INDEX § 7 AS-37 row(將改 closed)
- Baseline:`a016486`(NITS-u1 commit,v0.5.2 之後 1 commit)
- 檔案碰撞:無 in-flight unit;PIE smoke test 檔前兩 unit 已 ship

### Composed prompt

結構同前;unit spec 核心:

1. **(b-1) Helper**:在 `ArchSimPieHarness`(namespace/header 依既有慣例)加 sidestep helper(如 `OverrideGameModeForSafePIE(UWorld*)` 或對既有 override 機制最貼合的簽名),封裝 DefaultGameMode override + WHY 註解(cite AS-37-u1:AssetRegistry `/ALS/` 未及索引 → LoadObject 四連敗 → `AlsCharacterMovementComponent.cpp:894/903` ensures → `AlsCharacter.cpp:526` null deref;commandlet-only;crash log refs)
2. **PortalFrame test 改用 helper**(行為恆等;inline sidestep 移除)
3. **(a) docs**:ARCH_INDEX § 7 AS-37 row → ✅ closed(commandlet-only 結論 + 修正 crash 鏈記載(首發 L894/L903,終崩 L526)+ Dev `-game` 無 pak caveat + helper 指標 + 「未來 PIE test 必用 helper」規約)
4. ExpectedUeTests 149 不變(無新 test class);6-leg gate 全綠收尾

ESCALATE triggers:helper 需要動 production(非 Tests/)才能生效 / override 時序在 helper 化後失效(leg 6 FAIL 且 budget 內無法收斂)。
越界:不動 ALS plugin、不動 SPUD、不實作 (b-2)/(b-3)/(b-4)。

## Agent return 2026-07-02T0325Z (iteration 1)

**Status:** ✅ DONE(self-reported;待 Phase 3 review)
**Wall time:** 22m 17s / 25min|**Token:** 108,825 / 200K|**Tool calls:** 37 / 90 — **全在 budget 內**(校準生效)

### Full report(verbatim 要點)

## Files touched
| Path | LOC delta | 類型 | New? |
|---|---|---|---|
| `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` | +57 | helper 宣告 + `#if` 保護 | No |
| `Source/ArchSim/Private/Tests/ArchSimPieHarness.cpp` | +87 | `OverrideGameModeForSafePIE` 實作 | No |
| `Source/ArchSim/Private/Tests/ArchSimPortalFramePIESmokeTest.cpp` | 淨 -20 | inline sidestep → helper 呼叫 | No |
| `docs/ARCHITECTURE_INDEX.md` | 1 row | AS-37 → ✅ closed | No |

## Helper 設計
- 歸屬 `ArchSimPieHarness` namespace(bootstrap 函式集同抽象層);簽名 `bool OverrideGameModeForSafePIE(FAutomationTestBase* Test = nullptr)`;`#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR` 保護(`FAutomationEditorCommonUtils`/`FAutomationTestBase` 為 editor-only 符號)。
- 時序語意:`RunTest()` 同步、所有 `ADD_LATENT_AUTOMATION_COMMAND` 之前 — 與原 inline 完全相同(CreateNewMap + WorldSettings->DefaultGameMode override 在 PIE latent 排入前完成)。
- inline(原 L370-392,30 行)→ helper 呼叫(4 行 + include);AddError 由 helper 發。

## Docs delta
ARCH_INDEX AS-37 row → ✅ closed S-08 (v0.5.3):commandlet-only(+Dev `-game` caveat)、精確 crash 鏈(L894/L903 首發 + L526 終崩)、(a)+(b-1) 處置、helper 指標、**未來規約:所有 commandlet PIE test(含 AS-08-u2)必須呼叫 OverrideGameModeForSafePIE()**。

## Verification evidence
- UE build:Result: Succeeded
- Leg 6:`Pre-step [VERIFIED]: WorldSettings.DefaultGameMode → AGameModeBase` + SC1/SC4 [VERIFIED] + Result={成功} EXIT CODE 0
- 6-leg gate:[1/6]..[6/6] 全 PASS → `GATE: PASS`(149)

## Gotchas
1. `#if` 保護必要性(editor-only 符號)。
2. **STALE LOG 偽陽性**:standalone run_pie_gate.ps1 一次出現 pre==post(時戳精度)→ exit 1;6-leg gate 內 leg 6 正常 PASS。NITS-u1 reviewer 預言的 `-le` same-timestamp 邊界當天應驗;該腳本屬 NITS-u1 已穩定範圍,本 unit 越界禁令不修,留記錄。
3. include 已收在 `#if` 區段內,不污染非 test build TU。

## Self-grading
Deliverable 1/2/3 全 [VERIFIED](build + leg 6 + 6-leg gate + docs verbatim)。

## ESCALATE?
None

---

## Adversarial review (iteration 1) 2026-07-02T0345Z

**Verdict:** NITS(3 findings + 2 missed edges;無 BLOCKER)

**行為恆等對照(reviewer 逐分支):** CreateNewMap ✅ / WS non-null ✅ / WS null(AddWarning 後繼續)✅ / 時序 ✅;唯 null-world 路徑 helper 少了原 inline 的 `TestNotNull` structured Failure record(最終 test fail 結果不變)→ near-equivalent。

**Findings 與 integrator 處置(Phase 4 small-fix):**
1. NITS:TestNotNull record 消失 → **Phase 4 修**:helper 內補 `if (Test) Test->TestNotNull(...)` 恢復完整 parity(原 inline 本就 TestNotNull + AddError 雙發)。
2. NITS:helper comment 殘留「Caller's TestNotNull will produce the hard failure」誤導性 forward-ref → **Phase 4 修**。
3. NITS:ARCH_INDEX AS-37 row 前瞻寫 v0.5.3(HEAD 尚未 tag)→ 本輪 tag v0.5.3 後自動消解。

**Missed edges(採納):**
- ARCH_INDEX L252 AS-35 row 仍寫「test-local WorldSettings override」→ **Phase 4 修**為 helper 指標。
- `Test = nullptr` 時 null-world 失敗無聲(現有 caller 傳 this,不發生)→ 記錄;helper WHY 註解已含規約,暫不改簽名。

**鐵則:** FROZEN 0 行 / touched 4 檔一致 / 越界(gate scripts / ALS / SPUD)0 diff / no commit(HEAD `a016486`)/ [VERIFIED] oracle(log 2026-07-02 10:32 `OverrideGameModeForSafePIE [VERIFIED]` + Result={成功})— 全 CONFIRMED。

**Decision:** Accept。Phase 4 = v0.5.3 tag(NITS-u1 + AS-37-u2 合併)+ integrator small-fixes(上列 1/2/AS-35 row + **stale-guard same-timestamp 偽陽性修復**(AS-37-u2 gotcha 應驗 NITS-u1 reviewer 預言,gate 路徑不留 flake))。
