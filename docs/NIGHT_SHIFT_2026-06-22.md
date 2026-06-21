# Night shift log — 2026-06-22 (PLAN v3.2 UE 介面 thin slice)

> Live log,每 phase 完成 / 卡住 / 浮現 unilateral decision 都 append。
> User 醒來看 §1 summary + §3 per-phase status 就能 1 分鐘掌握全貌。
> Plan 沒寫的不可逆改動 → 在 §4 紀錄等簽,**不 unilateral push main**。

---

## 1. Summary (TL;DR — 持續 update)

| 階段 | 狀態 | wall-clock | 備註 |
|---|---|---|---|
| Plan drafted + signed off | ⏳ pending sign-off | — | `docs/PLAN_v3.2_ue_interface.md` written, awaiting commit & user review |
| Phase 0 — pre-flight 五腿 gate | ✅ PASS | ~6 min | 五腿全綠 (F1..F70 / UE 60/60 / OpenSees / audit 104 / CLI) |
| Phase 1 — FrameCoreUE module shell | ✅ PASS | ~10 min code + 36 s build | New module compiled clean; UnrealEditor-FrameCoreUE.dll linked |
| Phase 2 — Blueprint node + smoke test | ✅ PASS | ~10 min code + 5 s build + 5 s automation | 61/61 UE tests; BP marshal rel<1e-5 vs POD oracle; analytic rel<1e-4 |
| Phase 3 — Slate editor utility panel | ✅ PASS | ~15 min code + 10 s build + 5 s automation | 62/62 UE tests; Slate panel constructs + OnComputeClicked drives path without crash |
| Phase 4 — 五腿 gate + bump ExpectedUeTests | ✅ PASS | ~7 min | 五腿 62/62 + v2_roundtrip CPU ALL PASS (kEngineVer still 3.1.0) |
| Phase 5 — release-hardening + tag v3.2.0 | ✅ PASS | ~60 min | All 4 in-place audit fixes applied + 1 deferred to v3.3 U-07; post-fix 五腿 still green; tag v3.2.0 pushed; binary bundle (9.2 MB) attached to GitHub release |

**狀態圖例:** ⏸️ not started · 🚧 in progress · ✅ PASS · ❌ NEGATIVE · ⏭️ DEFERRED · ⏳ pending sign-off

---

## 2. Base anchor (start of session)

- HEAD: `0f32648` ("ci: quote release-gate.yml Leg 1 step name") on `main`, 1 commit ahead of tag `v3.1.0` (`5da6f56`)
- work tree: clean
- last 五腿 verified: 2026-06-21 (RELEASE_v3.1.0.md §3 reproduction matrix);CUDA legs NOT RUN
- v3.1.0 source delta verified at integrator host:
  - standalone F1..F70 ALL PASS
  - UE 60/60 ALL PASS (built in v3.1.0 cycle)
  - OpenSees PASS
  - audit 104 PASS
  - CLI roundtrip 13 PASS

---

## 3. Phase status log (append-only)

### Plan drafted

- **2026-06-21 night** — user invoked PLAN flow; A/B/C answers captured:
  A=Thin slice 三層都最薄, B=Direct link FrameCore.dll, C=4/4 不可逆改動全勾
- Plan written to `docs/PLAN_v3.2_ue_interface.md`
- §3 allow-list clarifications:
  - C-2: 實際只動 **新檔** `FrameCoreUE.Build.cs`,`FrameCore.Build.cs` 不動(守鐵則 #1)
  - C-3: 實際只動 `.uplugin` 加 module entry,`ArchSim.uproject` 不動(uproject 是 plugin-level
    enable,plugin → modules 是 .uplugin schema)
- Awaiting commit + push + user sign-off。在 user sign-off 前不進 Phase 0。

### Phase 0 — pre-flight 五腿 gate

- **2026-06-22 night** — sign-off received, ran `Scripts\run_gate.ps1 -RequireOpenSees` with
  `PATH = $env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH` + `UE_ENGINE_ROOT=E:\project\UE_5.7`
- Five legs:
  - [1/5] standalone: **ALL PASS (failures=0)** — F1..F70 全綠
  - [2/5] UE automation: **60 tests run, exit 0** — 跑現有 v3.1.0 binary
  - [3/5] OpenSees: **PASS** — `python` resolved to `WindowsApps\PythonSoftwareFoundation.Python.3.12`
    (openseespy installed there, NOT in framecore-direct conda env which is libs-only)
  - [4/5] linear deep audit: **PASS failures=0 checks=104**
  - [5/5] CLI round-trip: **ALL PASS (failures=0)**
- Verdict: `GATE: PASS` exit code 0
- Log saved to `Saved\Logs\phase0_gate.log`
- **Lesson:** `framecore-direct` conda env has zero python binary (it's a build-time libs-only env:
  OpenBLAS / METIS / cuDSS). OpenSees leg uses system Python (Windows Store 3.12) which has
  openseespy. Plan §3 Phase 0 風險「OpenSees env 切換失敗」實際上不存在 — env 不需切換,只需 PATH 加 Library\bin
  讓 native dll 能找到。durable lesson 進 §6。
- Proceeding to Phase 1.

### Phase 1 — FrameCoreUE module shell

- **2026-06-22 night** — created new module:
  - `Plugins/FrameSolver/Source/FrameCoreUE/FrameCoreUE.Build.cs` — depend Core/CoreUObject/Engine/FrameCore; bBuildEditor branch adds Slate/SlateCore/UnrealEd/EditorStyle/EditorSubsystem/ToolMenus/InputCore (Phase 3 will use)
  - `Public/FrameCoreUE/FrameCoreUEModule.h` + `Private/FrameCoreUEModule.cpp` — custom `FFrameCoreUEModule` IModuleInterface, IMPLEMENT_MODULE
  - `Public/FrameCoreUE/FrameCoreUETypes.h` — 5 USTRUCT(BlueprintType): FFrameStressFieldSample / FFrameMemberStressTrace / FFrameShellStressPoint / FFrameShellStressLayer / FFrameStressField; all BlueprintReadOnly; -1 sentinel for governing IDs (v3.1.0 C-07/C-08 pattern)
  - `Private/FrameCoreUETypes.cpp` — `FrameCoreUE::ToBlueprint(frame::StressField)` marshal helper; static MarshalSample/MarshalTrace/MarshalPoint/MarshalLayer; lossy double->float cast
  - `Public/FrameCoreUE/FrameCoreUELibrary.h` + `Private/FrameCoreUELibrary.cpp` — `UFrameCoreStressFieldLibrary` UBlueprintFunctionLibrary; ComputeCantileverFixture (BP demo entry) + GetGoverningMemberId/ShellId/GlobalMaxFiberSigma/GlobalMaxVonMises/GetMemberSamples (BP pure accessors)
- Updated `Plugins/FrameSolver/FrameSolver.uplugin` — Modules[] gained second entry "FrameCoreUE" Runtime Default
- Engine rule #1 honoured: zero edits under `Plugins/FrameSolver/Source/FrameCore/` (verified via `git status`)
- **Build:** `Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development` → exit 0 in 35.95 s; `[10/11] Link UnrealEditor-FrameCoreUE.dll` + `[11/11] WriteMetadata ArchSimEditor.target`; Result: Succeeded
- Warnings (carried over from FrameCore CUDA path, not new): cusparse deprecated types (csrsv2Info_t / csric02Info_t / bsric02Info_t / cusparseSolvePolicy_t / etc). These are NVIDIA cuSPARSE header deprecation notices in the conda env's cuDSS bundle, unchanged from v3.1.0 build. Not a v3.2 regression.
- **Cantilever fixture decision:** `Private/FrameTestFixtures.h` is FrameCore-internal — FrameCoreUE re-implements `BuildCantileverFixture(P, L)` inline in `Private/FrameCoreUELibrary.cpp` to avoid Private-header coupling. The fixture mirrors `fixtures::cantileverTipLoad` exactly (rectangular 100x100 / S235-like / +Z tip load), so the F68 oracle remains the verifier.
- Proceeding to Phase 2 (BP smoke test).

### Phase 2 — Blueprint node + smoke test

- **2026-06-22 night** — BP Library was already implemented in Phase 1 (ComputeCantileverFixture
  + GetGoverningMemberId/ShellId/GlobalMax(FiberSigma|VonMises) + GetMemberSamples). Phase 2
  added the smoke test and bumped `$ExpectedUeTests`.
- Added `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEBlueprintSmokeTest.cpp`
  with two oracles:
  - (a) engine POD cross-check — call `frame::computeStressField` on the same fixture
    inline in the test (own local `BuildCantileverFixtureLocal`, no FrameCoreUELibrary
    reach-in), compare `BP.Samples[0].SigmaCompMax` vs `POD.samples[0].sigmaCompMax`
    at **rel<1e-5** (only divergence is the engine-side double->USTRUCT float cast)
  - (b) analytic |P|·(L-x)/Wz over 11 samples at **rel<1e-4** (analytic + float cast budget);
    tip-sample sigExp=0 case uses absolute diff (v3.1.0 F68 audit pattern)
- Bumped `Scripts/run_gate.ps1` `[int]$ExpectedUeTests = 60 -> 61` (v3.2 entry +1); comment
  block bumped accordingly. Test count guard `-ge 61` now enforced.
- Build: incremental ~5 s (`[Adaptive Build] Excluded from FrameCoreUE unity file: ...`
  confirmed taunt-rail #4 working as advertised — new .cpp gets a fresh TU, no anon-namespace
  collision risk).
- UE automation: **Total tests run: 61, EXIT CODE: 0**.
  `Test Completed. Result={成功} Name={BlueprintSmokeTest} Path={FrameCore.UE.BlueprintSmokeTest}`
- Marshal layer therefore validated: USTRUCT round-trips POD without semantic drift inside
  the visualisation budget.
- Proceeding to Phase 3 (Slate editor utility panel).

### Phase 3 — Slate editor utility panel

- **2026-06-22 night** — landed minimal Slate panel + nomad tab spawner under `#if WITH_EDITOR`:
  - `Public/FrameCoreUE/SFrameCoreStressFieldPanel.h` + `Private/SFrameCoreStressFieldPanel.cpp`
    — `SCompoundWidget` subclass. UI = Compute button + result text block + sample
    `SListView<FFrameStressFieldSample>`. OnComputeClicked calls
    `UFrameCoreStressFieldLibrary::ComputeCantileverFixture(1000, 2000, 11)` and re-fills
    the list. `OnComputeClicked()` is public so the editor smoke test can drive it
    directly without simulating Slate click events.
  - `Private/FrameCoreUEModule.cpp` — `StartupModule` registers a nomad tab spawner under
    `WorkspaceMenu::GetMenuStructure().GetToolsCategory()` with tab name "FrameCore Stress Field".
    `ShutdownModule` unregisters. Both gated by `#if WITH_EDITOR` so the packaged-game path
    is untouched.
  - `FrameCoreUE.Build.cs` — `bBuildEditor` branch already had Slate/SlateCore/UnrealEd/
    EditorStyle/EditorSubsystem/ToolMenus/InputCore; added `"WorkspaceMenuStructure"` for
    `GetMenuStructure().GetToolsCategory()`.
- Added `Private/Tests/FrameCoreUEEditorSmokeTest.cpp` — `#if WITH_EDITOR` + `#if WITH_DEV_AUTOMATION_TESTS`
  guard; constructs the panel and drives `OnComputeClicked` directly, asserts FReply
  is handled. Does NOT exercise the nomad tab spawner (that needs a real editor session;
  the spawner registration is implicitly validated by the test running at all, since
  `StartupModule` already executed).
- Bumped `Scripts/run_gate.ps1` `$ExpectedUeTests 61 -> 62`; comment block updated.
- Build: incremental ~10 s; adaptive build properly excluded the new SFrameCoreStressFieldPanel.cpp
  + FrameCoreUEEditorSmokeTest.cpp from FrameCoreUE unity (taunt-rail #4 still well-behaved).
- UE automation: **Total tests run: 62, EXIT CODE: 0**.
  `Test Completed. Result={成功} Name={EditorSmokeTest} Path={FrameCore.UE.EditorSmokeTest}`
  `Test Completed. Result={成功} Name={BlueprintSmokeTest} Path={FrameCore.UE.BlueprintSmokeTest}`
- Phase 3 PASS. Proceeding to Phase 4 (full 五腿 gate).

### Phase 4 — 五腿 gate + bump ExpectedUeTests

- **2026-06-22 night** — full `run_gate.ps1 -RequireOpenSees` + v2_roundtrip CPU:
  - [1/5] standalone: **ALL PASS** (failures=0) — F1..F70 bit-identical to Phase 0 (zero engine source touched, verified by git diff)
  - [2/5] UE: **62 tests, exit 0** — 60 base + Phase 2 BP smoke + Phase 3 editor smoke
  - [3/5] OpenSees: **PASS**
  - [4/5] linear deep audit: **PASS** failures=0 checks=104
  - [5/5] CLI roundtrip: **ALL PASS** (failures=0)
  - `GATE: PASS` exit 0
- v2_roundtrip CPU (`build_capi_v2.bat` + `Tools/v2_roundtrip.py` with FRAMECORE_EXPECTED_ENGINE_VER=3.1.0):
  - capabilities list intact: `inspect.stress_field` present alongside the rest of v3.1.0 verbs
  - **`=== summary: ALL PASS ===`** exit 0
- This proves Phase 1-3 added a pure UE-side reflection module and did NOT touch v2 dispatcher
  schema. `kEngineVer` still `3.1.0` at this point — Phase 5 bumps it to `3.2.0`.
- Proceeding to Phase 5 (release-hardening + tag v3.2.0).

### Phase 5 — release-hardening + tag v3.2.0

- **2026-06-22 night** — Phase 5 work product so far:
  - Version bumps applied to 4 pin sites: `.uplugin Version 29->30, VersionName 3.1.0->3.2.0`;
    `Dispatcher.h kEngineVer 3.1.0->3.2.0` + comment block; `run_gpu_gate.ps1` env-var pin;
    `release-gate.yml` env-var pin + Leg 1 step name rename.
  - Final verify post-bump (background task busm1o4kz):
    - Standalone build + run: ALL PASS (F70 globalMaxVonMises bit-exact rel=0)
    - frame_capi_v2.dll rebuilt with kEngineVer 3.2.0 baked in
    - v2_roundtrip CPU with FRAMECORE_EXPECTED_ENGINE_VER=3.2.0: ALL PASS, capabilities list unchanged
  - Drafted `docs/RELEASE_v3.2.0.md` (7 sections including limitations, deferred U-01..U-06,
    repro matrix) and `docs/HANDOFF_v3.2.0.md` (5 sections + first-action sketches).
- **3-agent adversarial audit** (general-purpose subagents in parallel; A=correctness, B=build,
  C=docs). Returned **8 findings** (2 BLOCKER + 2 HIGH + 3 MEDIUM + 1 LOW). v3.1.0 audit
  pattern: cross-confirmation across agents = high signal; orphan single-agent findings
  weaker. Cross-confirmed: stale UE test count (B-LOW + C-HIGH on README/VERIFICATION).
- **Closeout disposition:**
  - **APPLIED (in-place fixes)**:
    - C-BLOCKER (README.md status block stale v3.1.0)
    - C-HIGH + B-LOW (VERIFICATION.md + README.md UE count stale 60 -> 62)
    - A-MEDIUM (smoke test assertion robustness: `>= 0` AND `== 0` for value specificity)
    - C-MEDIUM (FrameCoreUELibrary.cpp clamp comment "empty trace" -> "clamps to 11")
  - **DEFERRED to v3.3 (rule #1 violation if applied now)**:
    - A-BLOCKER (sentinel mismatch engine 0 vs USTRUCT -1) -> v3.3 U-07; HANDOFF docs U-07
      added; doc comment added to `FrameCoreUETypes.h` to flag the convention discrepancy.
      The fix requires editing `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressField.h`
      to change engine default 0 -> -1, which is engine source (rule #1) and out of v3.2 scope.
      For the v3.2 smoke test fixture, member id 0 actually IS the governing member, so the
      assertion currently passes for the right reason on this fixture — the bug surfaces only
      on fixtures with no governing element, which is an edge case for the visualisation lane.
  - **REJECTED (false positives)**:
    - B-MEDIUM (uplugin Type "Editor" not "Runtime"): the USTRUCT + UBlueprintFunctionLibrary
      are runtime BP exposure (not `#if WITH_EDITOR`); they must compile into packaged-game
      builds for BP runtime to call. Only the Slate panel + tab spawner are editor-only,
      and they're `#if WITH_EDITOR` compile-elided. Type="Runtime" is correct.
    - C-HIGH (HANDOFF cross-links to non-existent docs/HANDOFF.md and HANDOFF_v2.11.1.md):
      verified by `Glob "docs/**/HANDOFF*.md"` — both files DO exist
      (`docs/HANDOFF.md` and `docs/HANDOFF_v2.11.1.md`). Agent C's claim was wrong.

---

## 4. Out-of-plan decisions awaiting sign-off (append-only)

> Plan §3 allow-list 之外浮現的不可逆改動 → 此處紀錄,**不執行**,等 user 醒來簽核。

(空 — 目前無)

---

## 5. Idle-time work (append-only)

> Phase 結束剩餘 budget 時做的 research / docs / stability 工作。每項 1-2 句,連結到產出檔。

(空 — 目前無)

---

## 6. Lessons / surprises (append-only durable)

> 夜班撞到的 durable 教訓。下次 session HANDOFF 會吸收。

1. **`framecore-direct` conda env 不含 Python.** 它是 build-time native libs-only env
   (OpenBLAS / METIS / cuDSS DLLs)。`Get-Command python` resolves to Windows Store 3.12
   binary which has `openseespy` installed there. For five-leg gate: just prepend
   `$envRoot\Library\bin` to PATH; no `conda activate`. Plan §0 risk「OpenSees env
   切換失敗」實際不存在。Update HANDOFF for future contributors.

2. **`run_gate.ps1` 並不 build UE** — `$ExpectedUeTests` count guard 是漏編 test 的最後一道
   防線。v3.2.0 加 2 個 UE test → bump 60→62。任何加新 UE test commit 前必先 incremental
   rebuild,否則 gate `-ge` count guard 會 short-fall。

3. **UE adaptive non-unity build 在加新 .cpp 時自動工作** — `[Adaptive Build] Excluded from
   FrameCoreUE unity file: ...` 是好現象,新 .cpp 各自 TU 編譯,匿名 namespace 不衝突。
   v3.2 加了 8 個 .cpp 全跑 adaptive,沒問題。但未來若大量加 .cpp,unity 變得很小可能
   影響 build time;v3.3 加 renderer / actor / component 時要監控。

4. **3-agent audit cross-confirmation 對「真信號」與「false positive」非常清晰** — 跨 A/B/C 三
   agent 都飆同 finding(stale UE count = B-LOW + C-HIGH)就是高信心 — 我直接 apply。
   單一 agent flag 的 finding(C-HIGH HANDOFF cross-link)我先用 `Glob` 驗證實證 — 結果
   FALSE POSITIVE,doc 是存在的。Cross-check finding 是 release-hardening 核心,別跳。

5. **rule #1 對 v3.2 是真實 binding** — A-1 BLOCKER (sentinel mismatch 引擎 0 vs USTRUCT -1)
   真實但 fix 需要動 `StressField.h`。違反鐵則 #1 比帶一個 edge-case bug 更嚴重 — defer 是
   正確選擇,並加 durable comment 在 `FrameCoreUETypes.h` + HANDOFF U-07 first-action。
   未來 release-hardening 看到「BLOCKER 須動 engine source」時也應同樣 defer 而非硬修。

6. **plan 預估 11.5-17.5 hr 實際 ~3-4 hr 完成** — 整個夜班(Phase 0 預檢 6 min + Phase 1 11 min
   + Phase 2 11 min + Phase 3 16 min + Phase 4 14 min + Phase 5 ~60 min including audit + fix
   + rebuild + tag + release)= ~120 min from sign-off。Plan 預估含「最壞 case Phase 3 卡住」
   risk budget;實際 Slate 沒卡。教訓:plan budget 不要砍太細,實際 throughput 在 thin slice
   UE 改動上比想像快。

### Phase 5 — release-hardening + tag v3.2.0 (final entry)

- **2026-06-22 00:00-01:00** — 上述 §3 Phase 5 開始記錄,本附錄為 closeout summary:
  - Audit closeouts: 4 in-place fixes + 1 defer (A-1 sentinel) + 2 rejects (false positives)
  - Post-audit-fix rebuild: incremental 6.32 s (only smoke test .cpp recompile due to
    assertion change; other audit fixes are doc / comment only)
  - Post-audit-fix gate: **GATE: PASS** five-leg + UE 62/62 + audit 104 + CLI 13 +
    OpenSees PASS
  - Commit: `5e5a68f` (21 files changed, +1495 / -53)
  - Tag: `v3.2.0` annotated, pushed to origin
  - Binary bundle: `dist/framecore-v3.2.0-win64.zip` 9.2 MB (frame_capi.dll + frame_capi_v2.dll +
    frame_cli.exe + frametest.exe + openblas.dll + README.txt)
  - **GitHub release**: <https://github.com/rocky59487/architect_simulator/releases/tag/v3.2.0>
    --latest, --notes-file docs/RELEASE_v3.2.0.md
- All 6 plan phases ✅ PASS. v3.2.0 ready for user review.
