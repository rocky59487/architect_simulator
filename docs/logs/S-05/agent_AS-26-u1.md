# Agent log — AS-26-u1: `UArchSimModelRegistry` ClassWithin verify + `ArchSimPieHarness` `NewObject` outer mirror

## Dispatch 2026-06-27T02:15 (iteration 1)

**Plan reference:** [`docs/logs/S-05/plan_2026-06-27T0200.md`](plan_2026-06-27T0200.md) § "AS-26-u1"
**Scope contract:** [`docs/logs/S-05/scope_2026-06-27T0145.md`](scope_2026-06-27T0145.md)
**Domain skills loaded:** ue5-engineer (primary) + cpp-engineer (secondary)
**Budget:** 30-45 min / 100K tokens / 25 steps / 20 min wall timeout
**Baseline:** Sprint S-05; tag `v0.3.1` @ commit `994be68`; branch `main`
**Round:** 1 of 4 parallel (with AS-27-u1, AS-25-u1, SPIKE-UE5.8-eval); no file collision verified at plan time

### Pre-flight reads (main thread, before composing prompt)

- `docs/ARCHITECTURE_INDEX.md` § 2 (class map) — `UArchSimModelRegistry` row confirmed; § 7 backlog row `AS-26` confirmed MEDIUM with first-action pointer; § 6 test inventory shows `ArchSim.Integration.PieHarnessSmoke` exists at v0.3.0
- `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h:33-34` — `UCLASS()` (no explicit `Within=` specifier) on `UArchSimModelRegistry : public UGameInstanceSubsystem`; implicit `Within=UGameInstance` inherited from base
- `Source/ArchSim/Private/Tests/ArchSimPieHarness.cpp:60-82` — `GetOrCreateModelRegistry()` Level-3 fallback at line 81 confirmed: `return NewObject<UArchSimModelRegistry>();` (no outer; the AS-26 target)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` — AS-24 precedent reference (read by subagent during work)
- ue5-engineer SUBAGENT_PREFIX.md + cpp-engineer SUBAGENT_PREFIX.md — loaded for injection

### Composed prompt (verbatim)

```
你是 Architect Simulator ue5 + cpp engineer。Repo root: E:\project\ArchSim
語言:中文回報(技術識別字保留英文)。

=========================================================================
鐵則 (違反 = REJECT, main thread will re-prompt or escalate)
=========================================================================

1. **[FROZEN since v4.0.0]** Plugins/FrameSolver/Source/FrameCore/
   — engine source frozen. CLAUDE.md amendment required to touch.
   觸碰此路徑 = ESCALATE (do not attempt).

2. **[FROZEN since v2.2+1]** Plugins/LevelSim/Source/LevelCore/
   — LevelSim core frozen. Same rule as above.

3. 不准動: .gitignore / ArchSim.uproject / Plugins/LevelSim/* / build artifacts

4. NEVER `git add -A` or `git add .`. Explicit per-file only.

5. 不要 commit (主對話統一收 commit 在 Phase 4 release-hardening).

6. 5-leg gate must be green: Scripts\run_gate.ps1 -RequireOpenSees
   (current $ExpectedUeTests=145 cuDSS / 143 non-cuDSS as of v0.3.1)

7. Honest verify: every claim has an oracle.
   `[VERIFIED]` (oracle agrees) vs `[NEW CODE]` (claim, not yet verified)
   honest grading required in your report.

=========================================================================
Top-tier discipline (違反 = REJECT)
=========================================================================

- **NO STUBS**: 不准留 "TODO: handle X later" / "stub for AS-NN" 這類佔位.
  做就做完整 unit; 若 genuinely blocked, 在 final report 末尾寫 `## ESCALATE`
  section + 你做到哪、卡在哪、需要什麼 input.

- **NO HALF-FINISH**: 沒寫 ESCALATE 就回 "我做到這裡停下" = REJECT.
  Always either fully done OR fully ESCALATE'd.

- **READ BEFORE WRITE**: 先讀 docs/ARCHITECTURE_INDEX.md § 2, 6, 7, 9
  + already-existing tests/source, 避免重做 architecture-index 已收錄的東西.

- **PIN ACTUAL BEHAVIOR**: 若 spec 跟 production 行為不符, 不准改 production
  配 spec; 反而在 test 寫真實行為 + 在報告標 spec correction.

- **EDGE CASES**: 每個新 function/class 至少 cover 3 edge cases.
  本 unit 是既有 function 1-line 修改 + 註解 — edge cases 已由 existing test
  覆蓋, 但你必須 verify existing tests 仍 PASS.

- **COMMENTS explain WHY not WHAT**: 解釋意圖, 不是覆述程式碼.
  本 unit 必須加 AS-24-style WHY comment 解釋為何顯式 outer.

=========================================================================
Architecture index pointer
=========================================================================

**先讀** docs/ARCHITECTURE_INDEX.md (約 360 行, 10 節).
特別注意:
  - §2 ArchSim class map — `UArchSimModelRegistry` 是 `UGameInstanceSubsystem`
  - §6 UE test inventory — count 145 (cuDSS) / 143 (non-cuDSS)
  - §7 backlog status — AS-26 是你的本輪任務,row 在表中
  - §9 iron rules — cross-check 上面 cite

若你打算新增 class / file / API, 先確認 architecture index 沒收錄等價物.
本 unit 不新增 class / file / API; 只動 1 行 + 加 comment.

=========================================================================
Baseline
=========================================================================

Sprint: S-05
Current tag: v0.3.1 (commit 994be68)
Branch: main
Recent commits since last tag (showing v0.3.0..v0.3.1):
  994be68 release: v0.3.1 -- S-04 patch (carryover cleanup + cosmetic NITs)
  e763fa9 feat(S-04): PHASE5-NITS-u1 -- 7 cosmetic NITs bundle
  2883d40 feat(S-04): AS-24-u1 -- FrameCoreUE NewObject GetTransientPackage outer (3-site)
  4b6f094 feat(S-04): AS-20-u1 -- LogTemp -> LogArchSim umbrella sweep

=========================================================================
Domain expertise (injected from your assigned domain skills)
=========================================================================

[INJECTED: ~/.claude/skills/domain/ue5-engineer/SUBAGENT_PREFIX.md verbatim]
[INJECTED: ~/.claude/skills/domain/cpp-engineer/SUBAGENT_PREFIX.md verbatim]
(Main thread confirms both prefixes' §0 "絕對禁線" overlaps with iron rules
 above; intentional re-emphasis at domain layer.)

=========================================================================
本輪任務: AS-26-u1 — `UArchSimModelRegistry` ClassWithin verify + `ArchSimPieHarness` NewObject outer mirror
=========================================================================

**Context:** HANDOFF_v0.3.0.md §4 AS-24 first action mentioned
"ArchSimPieHarness::GetOrCreateModelRegistry() should also adopt the same
pattern" but S-04 AS-24-u1 scope explicitly excluded it. AS-24 fixed 3
`FrameCoreUE` test sites with `NewObject<UFrameInteractiveSubsystem>(GetTransientPackage())`
+ WHY comment. AS-26 mirrors this at `ArchSimPieHarness.cpp:81`.

**Hypothesis to verify FIRST:** `UArchSimModelRegistry` inherits from
`UGameInstanceSubsystem` which has implicit `Within=UGameInstance`. Cite UE5.7
engine source proving this (path + line) BEFORE making the fix. If the
hypothesis is WRONG (e.g. UGameInstanceSubsystem does NOT have implicit
`Within=UGameInstance`), STOP and ESCALATE — the AS-26 fix predicate fails.

**The fix (after verification):**
1. Edit `Source/ArchSim/Private/Tests/ArchSimPieHarness.cpp:81`:
   ```cpp
   return NewObject<UArchSimModelRegistry>();
   ```
   →
   ```cpp
   // AS-26: GetTransientPackage() outer for ClassWithin(UGameInstance) consistency.
   // UArchSimModelRegistry inherits from UGameInstanceSubsystem which has implicit
   // Within=UGameInstance. Without an explicit outer, the NewObject default outer is
   // already GetTransientPackage() (per UObjectGlobals.h:1918 in UE5.7), so this
   // change is technically equivalent to the no-arg call. The value of this fix is
   // intent-documentation + parity with the AS-24 fix at FrameCoreUE 3 test sites
   // (commit 2883d40). See HANDOFF_v0.3.0.md §4 AS-24 / S-04 manager.md Round 1.
   return NewObject<UArchSimModelRegistry>(GetTransientPackage());
   ```
   (Adapt comment phrasing to AS-24 precedent; keep WHY focus.)

2. NO other source change. NO test change.

3. Run UE incremental build + isolated `ArchSim.Integration.PieHarnessSmoke`
   test (verify still PASS) + full 5-leg gate (verify still 145/143 PASS).

**Verify hypothesis evidence required in report:** Cite the UE5.7 engine source
path:line that proves `UGameInstanceSubsystem` has implicit `Within=UGameInstance`,
AND cite the UE5.7 `UObjectGlobals.h:1918` line showing
`NewObject<T>(UObject* Outer = GetTransientPackageAsObject())` (matching AS-24
subagent's discovery in S-04 manager.md Round 1 AS-24-u1 KEY REVIEW POINT).

Files you are likely to touch:
- `Source/ArchSim/Private/Tests/ArchSimPieHarness.cpp` (PRODUCTION — 1 line + comment block)
- `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` (READ-only)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp` (READ-only — AS-24 precedent reference)
- UE5.7 engine source under `$env:UE_ENGINE_ROOT\Engine\Source\Runtime\CoreUObject\` (READ-only — ClassWithin / NewObject default outer verification)
- `Scripts/run_gate.ps1` (READ — confirm `$ExpectedUeTests` line; should be unchanged)

Estimated budget: 0.75h / 100K tokens / 25 tool calls / 20min wall.
**Do NOT exceed budget without ESCALATE.** If at ~80% budget (20 steps / 16min)
and not done, ESCALATE early.

ESCALATE triggers specific to this unit:
1. ClassWithin verification shows `UArchSimModelRegistry` does NOT actually
   have implicit `Within=UGameInstance` (hypothesis fails) — bring evidence
   + propose 2 alternative fixes.
2. Test behaviour changes after the fix (PieHarnessSmoke sub-check failure
   OR 5-leg gate count drift).
3. UE5.7 `UObjectGlobals.h` doesn't show `GetTransientPackageAsObject()` as
   the default outer (AS-24 precedent doesn't hold) — propose explicit
   defensive verification.

Adversarial-focus dimensions (Phase 3 will check):
- ClassWithin inheritance verification: cite UE5.7 engine source path:line
- `NewObject` default outer is `GetTransientPackage()`: cite UE5.7
  `UObjectGlobals.h:1918` (or current equivalent line)
- WHY comment style matches AS-24 pattern at FrameCoreUE precedent
  (compare side-by-side)
- 5-leg gate count unchanged (145 cuDSS / 143 non-cuDSS)
- No collateral edit (only line 81 + comment block above)
- Isolated `ArchSim.Integration.PieHarnessSmoke` still 8 sub-checks PASS

=========================================================================
Verification (run ALL of these, 真實 build + 真實 test, 不准 mock / fake)
=========================================================================

1. UE editor incremental build:
   & "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
       ArchSimEditor Win64 Development `
       -project="E:\project\ArchSim\ArchSim.uproject" -waitmutex
   Expect: Result: Succeeded, <X> seconds

2. 隔離 PieHarnessSmoke test:
   & "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
       "E:\project\ArchSim\ArchSim.uproject" `
       -ExecCmds="Automation RunTests ArchSim.Integration.PieHarnessSmoke; Quit" `
       -unattended -nullrhi -log
   Expect: Result={成功} + EXIT CODE: 0 (8 sub-checks all green)

3. 完整 5-leg gate:
   .\Scripts\run_gate.ps1 -RequireOpenSees
   Expect: GATE: PASS (UE 145 tests green; non-cuDSS: 143)

(Powershell: 確保 `$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"`
 已在 session 中設好 — `framecore-direct` conda env libs-only,OpenBLAS/METIS/cuDSS
 DLL 在 `Library/bin/` 下;openseespy 在 system Python (Windows Store 3.12)。)

=========================================================================
Reporting format (markdown; do NOT truncate; section order LITERAL)
=========================================================================

## Status
✅ DONE / ⚠️ PARTIAL (with ## ESCALATE) / ❌ FAIL (with ## ESCALATE)
[one-line summary]

## Files touched
| Path | LOC delta | Production / Test / Config / Docs | New? |
|---|---|---|---|

## Design decisions (non-obvious only)
- 你做了哪些非顯然的設計選擇, 為什麼
- 若無 = "Standard mirror of AS-24 pattern at one site; no notable decisions"

## ClassWithin verification evidence (REQUIRED)
- UE5.7 engine source path:line proving `UGameInstanceSubsystem` has implicit
  `Within=UGameInstance`
- UE5.7 `UObjectGlobals.h:1918` (or current equivalent) showing default outer
  of `NewObject<T>()` IS `GetTransientPackage()`-equivalent
- Confirmation of AS-24 precedent (cite `FrameCoreUEInteractiveSubsystemTest.cpp` line)

## Verification evidence (verbatim)
- UE build: <時間> + exit code (note: build should be near-instant since edit
  is 1 line in a test file)
- PieHarnessSmoke isolated: Saved/Logs/ArchSim.log 對應 Result={成功} 那行
  + 8 sub-assertion count
- 5-leg gate: 從 [1/5] 到 GATE: PASS 末段 (UE 145 tests confirmed green)

## Gotchas / discoveries
- 過程中發現任何 UE5.7 API quirk, AS-24 precedent edge case
- 若無 = "None"

## Self-grading (honest)
For each claim:
- "ClassWithin verification confirms hypothesis" — [VERIFIED] / [NEW CODE]
- "5-leg gate PASS at 145" — [VERIFIED] (gate output cited)
- "PieHarnessSmoke isolated PASS 8/8" — [VERIFIED] (test output cited)
- "WHY comment matches AS-24 pattern" — [NEW CODE] / [VERIFIED] (side-by-side
  diff with FrameCoreUEInteractiveSubsystemTest.cpp comment)

## ESCALATE? (寫此 section 即使是空的)
- 具體 blocker (file:line) — 若無 blocker, 寫 "None"

=========================================================================
你 ABSOLUTELY NOT 做
=========================================================================

- 不准動 FROZEN paths (鐵則 1, 2)
- 不准動 `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` (READ-only)
- 不准改 PieHarness.cpp 除了 line 81 + comment block
- 不准動 `Scripts/run_gate.ps1`(`$ExpectedUeTests` 應該不變)
- 不准動 docs/ARCHITECTURE_INDEX.md(該行已在 S-04 backlog table 列出;此 unit ship 後 Phase 5 才會 tick)
- 不准動其他 plugin source(ALS / SPUD / Prefabricator / SUQS)
- 不准跨 unit(AS-25 / AS-27 / SPIKE-UE5.8 是別的 subagent 的事)
- 不准 commit (Phase 4 統一收)
- 不准虛報 [VERIFIED]
- 不准超過 budget 不 ESCALATE

開工.
```

## Agent return 2026-06-27T02:21 (iteration 1)

**Status:** ✅ DONE
**Wall time:** ~10m 22s (621,949 ms)
**Token usage:** 90,470 of 100K budget (within)
**Tool calls:** 19 of 25 budget (within)

### Full report (verbatim from agent)

```
5-leg gate PASS,UE 145 tests green,全部 leg 通過。

---

## Status
DONE — ArchSimPieHarness.cpp:81 加 GetTransientPackage() outer + 9 行 AS-26 WHY comment block,鏡像 AS-24 pattern。

## Files touched
| Path | LOC delta | Type | New? |
|---|---|---|---|
| Source/ArchSim/Private/Tests/ArchSimPieHarness.cpp | +10 (1 line change + 9 line comment) | PRODUCTION | No |

## Design decisions (non-obvious only)
Standard mirror of AS-24 pattern at one site。`GetTransientPackage()` outer 在語意上等同於 no-arg call(UE5.7 UObjectGlobals.h:1919 的 default 就是 GetTransientPackageAsObject()),ClassWithin runtime warning 是 expected baseline behavior(AS-24 fix 的 FrameInteractiveSubsystem 也有相同 warning),不是 regression。AS-24 fix 的目的是在 ensure() cascades to fatal 的隔離 single-test runs 中防止 crash,GetTransientPackage() 明確傳入可消除 ensure() 問題;本 AS-26 fix 主要是 intent-documentation + parity。

## ClassWithin verification evidence (REQUIRED)
- UE5.7 ClassWithin: E:\project\UE_5.7\Engine\Source\Runtime\Engine\Public\Subsystems\GameInstanceSubsystem.h:15 — UCLASS(Abstract, Within = GameInstance, MinimalAPI) [VERIFIED]
- UE5.7 NewObject default outer: E:\project\UE_5.7\Engine\Source\Runtime\CoreUObject\Public\UObject\UObjectGlobals.h:1919 — T* NewObject(UObject* Outer = GetTransientPackageAsObject()) [VERIFIED]
- AS-24 precedent: Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEInteractiveSubsystemTest.cpp:49-52 — AS-24 WHY comment + return NewObject<UFrameInteractiveSubsystem>(GetTransientPackage()) [VERIFIED]

## Verification evidence (verbatim)
- UE build: Total execution time: 2.32 seconds + Result: Succeeded;4 actions (Compile ArchSimPieHarness.cpp / Link lib / Link dll / WriteMetadata)
- PieHarnessSmoke isolated: Saved/Logs/ArchSim.log:1209 — Result={成功} + EXIT CODE: 0;8 sub-check outputs 可見([2] GetOrFindGameInstance null / [4] IsRegistryFromRealGI=false / [5] SpawnActor 成功/銷毀 / [8] World->GetGameInstance()=null)
- 5-leg gate: [1/5] standalone: ALL PASS / [2/5] UE automation: 145 tests / [3/5] OpenSees: PASS / [4/5] deep audit: PASS checks=104 / [5/5] CLI: ALL PASS → GATE: PASS (standalone OK, UE 145 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK)

## Gotchas / discoveries
GetTransientPackage() outer 不消除 LogUObjectGlobals: ClassWithin warning(FrameInteractiveSubsystem AS-24 fix 後也同樣出現)— warning 是 UE ClassWithin runtime check 對非 GameInstance outer 發出,和 outer 是 null 還是 TransientPackage 無關。AS-24 comment 說明的「suppresses ensure()」是指 isolated fatal crash 路徑,而非 warning 本身。PieHarnessSmoke 仍 PASS,warning 是 expected。

## Self-grading (honest)
- "ClassWithin 假設正確" — [VERIFIED](GameInstanceSubsystem.h:15 Within=GameInstance)
- "5-leg gate PASS at 145" — [VERIFIED](gate output 引用:UE 145 tests green)
- "PieHarnessSmoke isolated PASS 8/8" — [VERIFIED](Result={成功} + 8 sub-check outputs 可見 + EXIT CODE: 0)
- "WHY comment matches AS-24 pattern" — [VERIFIED](side-by-side 讀 FrameCoreUEInteractiveSubsystemTest.cpp:49-52,本 comment 格式一致)

## ESCALATE?
None。所有 hypothesis 正確,gate 全綠,無 collateral edit。
```

### Main-thread observations (for Phase 3 review consumption)

- Subagent self-reported ✅ DONE with full 5-leg gate output cited
- ESCALATE: None
- **KEY VALIDATIONS for review:**
  - GameInstanceSubsystem.h:15 confirms `Within = GameInstance` is EXPLICIT specifier (not implicit as plan hypothesized) — this is honest correction
  - UObjectGlobals.h:1919 confirms `NewObject<T>(UObject* Outer = GetTransientPackageAsObject())` default
  - 5-leg gate output cites all 5 legs PASS at 145 UE tests
  - PieHarnessSmoke isolated PASS with 8 sub-checks visible
- **Honest gotcha disclosure:** subagent found that `GetTransientPackage()` outer does NOT eliminate the `LogUObjectGlobals: ClassWithin` warning — same warning appears for AS-24's `FrameInteractiveSubsystem` fix. This means the AS-26 fix is purely intent-documentation + parity (no behavioural warning suppression). Aligns with AS-24's "no-op equivalent" framing (S-04 lesson #2).
- Wall time 10m 22s is high but within timeout (20min). Most time in 5-leg gate run (~5-7min typical).

Chaining to Phase 3 review.

## Adversarial review (iteration 1) 2026-06-27T02:23

**Verdict:** NITS

**Reviewer findings:**

| # | severity | file:line | issue | evidence | recommended action |
|---|---|---|---|---|---|
| 1 | NIT | `ArchSimPieHarness.cpp:83` | comment 寫 `implicit Within=UGameInstance` 但 UE5.7 source 是 explicit UCLASS macro `Within = GameInstance` | `GameInstanceSubsystem.h:15` Read 確認:`UCLASS(Abstract, Within = GameInstance, MinimalAPI)` | Inline fix: comment 改 `implicit` → `explicit UCLASS macro` 並補完整 macro `(Abstract, Within = GameInstance, MinimalAPI)` |
| 2 | NIT | `ArchSimPieHarness.cpp:84` | comment cite UCLASS 省略 `Abstract,` 與 `MinimalAPI`,可接受簡寫但不完整 | Same evidence as #1 | Inline fix bundled with #1 |
| 3 | NIT | comment 長度 9 行 vs AS-24 precedent 3 行 (FrameCoreUEInteractiveSubsystemTest.cpp:49-51) | 風格密度差 3x;task spec 要求「AS-24-style」 | Side-by-side Read confirmed | 接受 extended form 作 design rationale 擴充(plan 含 HANDOFF cite + UObjectGlobals.h 引用);不強壓 |
| 4 | NIT | subagent files-touched report | report 只列 ArchSimPieHarness.cpp 但 working tree 同時有 AS-27 的 DriverLoopTest.cpp + ARCH_INDEX 修改 | `git status` 三 M | 屬 parallel-dispatch 正常 working-tree 共存(非 collateral);Phase 4 per-unit explicit `git add` 解決 |

**Reviewer's exhaustive-check declaration:**
- Read 4 files: `GameInstanceSubsystem.h` L10-25 / `UObjectGlobals.h` L1912-1926 / `ArchSimPieHarness.cpp` L55-116 / `FrameCoreUEInteractiveSubsystemTest.cpp` L44-62
- grep'd 3 patterns: `NewObject` in PieHarness.cpp (collateral check) / `implicit` (wording check) / `AS-24` in precedent
- Cross-checked 6 claims: UCLASS macro content / NewObject template signature / FROZEN paths diff / never-touch diff / `$ExpectedUeTests=145` / 1-line code + 9-line comment diff
- Rationale: 核心 code change 正確;UE engine cite 全 Read 驗實;FROZEN/never-touch 0 違反;5-leg gate count 不變。 實質 NIT 是 comment 用 `implicit` 但 UE source 是 `explicit` UCLASS macro — 文件精確度 NIT,非 code BLOCKER。 NITS 評級適當。

**鐵則 compliance:** FROZEN CONFIRMED 0 / Never-touch CONFIRMED 0 / No stub CONFIRMED / [VERIFIED] oracle CONFIRMED (with comment精確度 NIT).

### Phase 3 closeout (inline NIT fixes applied 2026-06-27T02:24)

Per S-04 lesson #3 (reviewer-found unit's-own-scope leak → inline fix before Phase 4):

1. **Findings #1 + #2 inline fix** (main thread Edit to `ArchSimPieHarness.cpp:82-89`): WHY comment 改:
   - `implicit Within=UGameInstance` → `explicit UCLASS macro UCLASS(Abstract, Within = GameInstance, MinimalAPI)`
   - 完整 macro 補全 (Abstract / Within / MinimalAPI)
   - UObjectGlobals.h:1919 cite 加 default arg `GetTransientPackageAsObject()`
2. **Finding #3 (comment density)**: 接受 extended form (9 lines).Plan content 包含 HANDOFF cite + 2 個 UE engine cite,合理。 No edit.
3. **Finding #4 (working tree co-existence)**: 屬 parallel-dispatch 正常 — Phase 4 per-unit explicit `git add` discipline (S-04 lesson #7) 解決。 No edit. Documented for context.

**Decision:** Accept with NITS inlined for #1 + #2. No new backlog AS-XX (NITs are unit-scope inline-fixed). Chain to Phase 4 for AS-26-u1 feature commit.


