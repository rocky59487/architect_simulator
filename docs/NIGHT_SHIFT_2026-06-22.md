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

### Phase 6 post-release strengthening (user's pre-sleep reminder: don't end too early)

User asked for continuous strengthening + scenario testing after v3.2.0 release. Spawned
Phase 6a-6d as post-release work; all four passed cleanly.

#### Phase 6a — 3 new BP marshal scenarios (62 → 65 UE tests)

Added three marshal-layer tests in `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/`,
each builds the fixture inline (no FrameCore-private header dep) and verifies
`FrameCoreUE::ToBlueprint` against the engine POD:

1. **`FFrameCoreUEMarshalSSBeamTest`** — F2 simply-supported beam under UDL (2 members,
   3 nodes). Verifies 2-member-trace USTRUCT shape; governing member id >= 0; BP
   sigmaCompMax matches POD rel<1e-5 at worst sample; UDL-induced Vy field marshals
   (non-zero internal force); `governingShellId == -1` for member-only model.
2. **`FFrameCoreUEMarshalShellPlateTest`** — F69 clamped 2x2 plate (4 shells, n=2,
   side=1000mm, t=10mm, q=0.01 MPa). Verifies `ShellsTop` + `ShellsBot` both
   populate with 4 layers; each layer has Center + 4 Corners; `bIsTopLayer` differentiates
   correctly; ShellId preserved from engine; VonMises marshal rel<1e-5; `governingShellId >= 0`;
   `governingMemberId == -1` for shell-only model.
3. **`FFrameCoreUEMarshalMultiMemberTest`** — 3-segment cantilever with USER-SET member
   IDs 100/200/300 (instead of default 0/1/2). Verifies MemberId field carries user IDs
   (NOT array index), MemberIdx tracks array index separately, `GoverningMemberId == 100`
   (the root member, real user ID — proves the engine writes the real ID, not the
   ambiguous-with-sentinel `0`).

**Build + automation:** rebuild incremental 7.79s; UE automation 65/65 EXIT 0; all 3 new
tests `Test Completed. Result={成功}`.

**Lessons:** Marshal layer is well-covered now — single-member / multi-member / shell-only
/ user-set IDs all verified. Test gap remaining: a model where NO member governs (engine
0-sentinel scenario) is the U-07 BLOCKER fixture that needs the engine fix first.

#### Phase 6b — Packaged build smoke (FrameCoreUE non-editor compile path)

3-agent audit B-MEDIUM finding was "FrameCoreUE.uplugin Type should be 'Editor' not
'Runtime'". I rejected as false positive because USTRUCT + UBlueprintFunctionLibrary need
to be in packaged builds. **Phase 6b proves the rejection was correct.**

Built `ArchSim` (non-editor) target with `Build.bat ArchSim Win64 Development -project=...`:
**Build exit 0 in 57.74s; `ArchSim.exe` + `ArchSim.lib` + `ArchSim.exp` produced**.

The packaged build links FrameCoreUE.dll cleanly, which means:
- USTRUCT + library compile into packaged-game (BP runtime can use them)
- `#if WITH_EDITOR` regions (Slate panel + tab spawner + WorkspaceMenuStructure dep) are
  properly compile-elided
- `Type="Runtime"` is the **correct** classification (the audit finding's premise — "code is
  almost entirely #if WITH_EDITOR" — was wrong; only the panel + spawner are editor-only)

Saved log: `Saved/Logs/phase6b_packaged_build.log`.

#### Phase 6c — Stability stress (3x repeat 五腿 gate)

Ran `Scripts/run_gate.ps1 -RequireOpenSees` three times back-to-back:

| Iteration | Exit | Wall-clock (s) |
|---|---|---|
| 1 | 0 (PASS) | 353.9 |
| 2 | 0 (PASS) | 352.0 |
| 3 | 0 (PASS) | 351.4 |

**All 3 iterations PASS** with **65/65 UE tests** every time. Timing drift = 2.5s = **0.7%**
(very stable; no JIT/cache warm-up artifacts). No flaky tests detected.

This is a strong end-state validation: v3.2.0 + Phase 6a strengthening is deterministic and
production-stable.

Saved logs: `Saved/Logs/phase6c_stress_{1,2,3}.log`.

#### Phase 6d — Apply safer NIGHT_SHIFT §5 sweep findings (doc topology)

Applied 6 doc-only changes from the 14 deferred idle-time sweep findings — the doc topology
ones that don't touch build/CI/source:

- `docs/README.md` stage table — added S11 (v3.1.0 stress field) + FrameCoreUE (v3.2.0)
  rows; updated the `S5_S11_skeletons.md` history entry to note S11's reuse
- `docs/README.md` external bridges section — updated "v2.8.1 audit confirmed 16 capabilities"
  to "v3.2.0 audit confirmed 23 capabilities incl. inspect.stress_field"
- `docs/ARCHITECTURE.md` §5b — added a paragraph cross-referencing v3.2.0 FrameCoreUE as the
  consumer-side module; lists USTRUCT mirror + library + Slate panel + 5 marshal tests
- `Plugins/FrameSolver/Standalone/build_capi_v2.bat` — modernised the comment block (B2 stub
  → B3 wire migration historical, now stable; adds v3.2.0 dispatcher state)
- `docs/specs/S5_S11_skeletons.md` — added 1-line disambiguation banner at top clarifying
  that v3.1.0 reused "S11" for stress-field post-process, while this skeleton's §S11 is the
  original MITC9i higher-order shell idea (deferred E-13 carry-forward)

Remaining 8 sweep findings still deferred (build .bat UE_5.7 fallback / HANDOFF absolute
paths / `E:/project/CLAUDE.md` out-of-tree anchor / gate-logs policy / run_gate.ps1 FAIL
echo / 2 NITs / U-07 engine BLOCKER) — all need user triage or are engine-source rule #1
blocked.

#### Phase 6e — U-04 closeout + robustness test (65 → 67 UE tests)

Two more tests added to deepen coverage:

1. **`FFrameCoreUEEditorTabSpawnerTest`** (`FrameCore.UE.EditorTabSpawnerTest`) — closes
   v3.2.0 deferred **U-04**. Asserts
   `FGlobalTabmanager::Get()->HasTabSpawner("FrameCoreStressFieldPanel")` is true after
   StartupModule. UE 5.7 build PASS. Future WorkspaceMenuStructure API drift surfaces as a
   failing automation test instead of a silent missing menu. `HANDOFF_v3.2.0.md` §3.2 U-04
   and `RELEASE_v3.2.0.md` §5.2 U-04 both marked CLOSED.
2. **`FFrameCoreUERobustnessTest`** (`FrameCore.UE.RobustnessTest`) — three sub-assertions:
   - (a) Negative-input contract — `ComputeCantileverFixture` with
     `SamplesPerSpan` = -3 / 0 / 1 silently clamps to 11 (the Phase 5 C-MEDIUM audit fix
     in `FrameCoreUELibrary.cpp`); `SamplesPerSpan = 21` passes through.
   - (b) Marshal scaling — 20-segment cantilever produces 20 USTRUCT traces, each
     11-sample, MemberId preserved 0..19, governingMemberId = 0 (root).
   - (c) Memory stability — 100 `ComputeCantileverFixture` repeat calls produce
     bit-exact USTRUCT every iteration (caught a state-leak between calls if it existed).

Rebuild incremental 6.64s; automation **67/67 EXIT 0**; both new tests `Result={成功}`.

`$ExpectedUeTests` bumped 65 → 67; non-cuDSS recommendation bumped 63 → 65.

#### Phase 6f — theta range + zero-load edge tests + perf baseline (67 → 69 UE tests)

Two more focused edge tests:

1. **`FFrameCoreUEThetaRangeTest`** (`FrameCore.UE.ThetaRangeTest`) — sweeps 40 shell
   sample points (4 shells × 5 points × 2 layers) on the 2x2 clamped plate fixture and
   asserts every `FFrameShellStressPoint.ThetaRad ∈ (-π/2, π/2]` (v3.1.0 audit A-09 fix
   carry-forward invariant; atan2-based principal axis cannot return -π/2 since that
   maps to +π/2). Catches float roundoff at boundary too (eps tolerance 1e-5).
2. **`FFrameCoreUEZeroLoadTest`** (`FrameCore.UE.ZeroLoadTest`) — `ComputeCantileverFixture(P=0, L=2000, 11)`
   verifies all 11 sample sigmas are exactly 0 (zero load → zero internal forces →
   zero analytic stress from `frame::computeStressField`), no NaN anywhere, global
   maxes 0, shell sentinels stay -1. This documents the BP-friendly zero-load contract.

Rebuild incremental 2.73s; UE automation **69/69 EXIT 0**; both new tests `Result={成功}`.
`$ExpectedUeTests` bumped 67 → 69; non-cuDSS recommendation 65 → 67.

**Compile error fresh durable lesson:** First attempt failed because `auto check = [&](...)` 
lambda name collided with UE's `check()` assertion macro (CoreMinimal.h). Compiler tried to
expand `check(bp.ShellsTop[s].Center)` as the macro, evaluating the argument as `bool`,
hence "`!` cannot be applied to const FFrameShellStressPoint". Renamed to `checkThetaRange`
and the build flew through. Adds to CLAUDE.md 踩雷 catalog alongside `IN`/`OUT` (Windows SAL
macros), now `check` (UE assertion macro).

#### Phase 6f perf baseline (`frame_perf.exe`)

Standalone XXL benchmark run for the v3.2.0 post-tag baseline reference:

```
case=XXL-24st-12x9 nx=12 ny=9 stories=24 repeat=5 warmup=1 dry=0
nodes=3250 members=9816 dof=19500 freeDof=18720 loads=3122
buildMs    = 0.24-0.32 ms (matrix assembly)
solveMs    = 1672-1828 ms median ~1714 ms (LDLT solve on 19500 freeDof)
checksum   = 14006.6399593 (deterministic across re-runs)
```

This is the engine-native LDLT path (the supernodal lane fires on much larger DOF counts).
v3.2.0 source delta in engine numerics is 0 lines vs v3.1.0; this baseline is therefore the
same v3.1.0 number, recorded here for handoff completeness.

#### Phase 6g — final 5-leg + v2_roundtrip combo (production-ready at 69/69)

After all Phase 6 strengthening landed, ran one more full verification combo:

- **5-leg gate** `GATE: PASS` — standalone F1..F70 / UE **69/69** / OpenSees / audit 104
  / CLI 13 all green; saved `Saved/Logs/phase6_final_gate.log`
- **v2_roundtrip CPU** `=== summary: ALL PASS ===` — capabilities list intact (still 23
  including `inspect.stress_field`), `kEngineVer=3.2.0` pin enforced

This is the post-Phase-6 production-ready snapshot: 69 UE tests + 5-leg + v2 dispatcher
all green at HEAD `2e96104` (Phase 6f commit; Phase 6g is just verification).

#### Phase 6h — FrameCoreUE Quick-Start doc for new contributors

Wrote `docs/FrameCoreUE_QuickStart.md` covering:
- What FrameCoreUE is (USTRUCT mirrors + library + editor panel)
- 3 use cases (Blueprint graph / Editor panel / native C++ module)
- Precision budget table (engine 1e-12 → USTRUCT 1e-5 cast → analytic 1e-4 → near-zero
  abs diff)
- What is NOT in v3.2.0 (U-01 / U-02 / U-03 / U-05 / U-07 deferred)
- Verification matrix snapshot (69 UE tests + 5 legs + v2 roundtrip all green)

Linked from `docs/README.md` stage table and `docs/HANDOFF_v3.2.0.md` follow-up section
so new contributors hit it before the formal RELEASE notes.

### Net Phase 6 outcome

UE test count: v3.2.0 tag shipped at 62 → +Phase 6a (3 marshal) → 65 → +Phase 6e (spawner
+ robustness) → 67 → +Phase 6f (theta + zero-load) → **69 UE tests**, all green. Plus the
v3.2 deferred **U-04** moved from "deferred to v3.3" to "CLOSED in v3.2 post-tag" (live
TabSpawner sanity test). Coverage extends to: single/multi-member traces, shell layers,
shell principal angles, user-set member IDs, negative-input contracts, zero-load edge,
20-member scaling, 100x repeat memory stability, packaged-build compile path, nomad tab
spawner registration, theta range invariant. Stability stress 3x clean (drift 0.7%). 6 doc
topology cleanups applied from the broader idle-time sweep. 1 durable lesson added
(`check` lambda vs UE macro collision).

### Repo-wide light hygiene sweep (1 general-purpose agent)

Plan §6 沒事做時優先序 #5 "release-hardening skill 做 deep audit"。Spawn 1 general-purpose
subagent doing light repo-wide hygiene sweep on the **non-v3.2 areas** (engine source / docs /
build / CI / scripts / scratch hygiene), since v3.2 source delta was already audited in Phase 5.
Agent returned **17 findings** across 8 categories (0 BLOCKER for v3.2.0 itself; all are
follow-up hygiene for the next contributor).

**Findings by category:**

| Severity | Cat | File:Line | Title |
|---|---|---|---|
| HIGH | stale_version | `docs/ARCHITECTURE.md:297,310` | UE test count frozen at 57 → 62 (v3.2.0); $ExpectedUeTests = 57 → 62 |
| HIGH | stale_version | `.github/workflows/release-gate.yml:6` | header references `docs/HANDOFF_v2.11.1.md` → should be `docs/HANDOFF_v3.2.0.md` |
| HIGH | tech_debt | `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressField.h:78` | U-07 sentinel mismatch carry-forward (engine 0 vs USTRUCT -1) — already documented in HANDOFF_v3.2.0 §3.2 |
| MED | scratch_contamination | `docs/gate-logs/v3.0.1/` | Orphan v3.0.1 gate logs in repo; no v3.1.0/v3.2.0 dirs (CI artifacts only; 90-day retention) |
| MED | doc_topology | `docs/README.md:33` | Stage table missing S11 + FrameCoreUE rows |
| MED | doc_topology | `docs/README.md:65` | "Rhino bridge v2 status" stale at v2.8.1 (16 caps); v3.1+ added inspect.stress_field |
| MED | doc_topology | `docs/ARCHITECTURE.md:241` | §5b describes computeStressField but no FrameCoreUE consumer cross-ref |
| MED | ci_hygiene | `.github/workflows/release-gate.yml:3` | "9-leg verification matrix" comment misleads — CI runs 5 CPU legs only |
| MED | hardcoded_path | `Plugins/FrameSolver/Standalone/build*.bat:7` | All 8 .bat files have hardcoded UE_5.7 fallback path (3rd tier); UE 5.8+ would silent-fail |
| MED | hardcoded_path | `docs/HANDOFF_v3.2.0.md:80` | "E:\project\UE_5.7\..." absolute path in Build.bat command sample; should be parameterised |
| LOW | stale_version | `docs/VERIFICATION.md:296` | "3 of the 57-test gate count" → 62 |
| LOW | doc_topology | `docs/README.md:65` | Release-notes table ends at v2.8.1; v2.9-v3.2.0 missing |
| LOW | claude_md | `E:/project/CLAUDE.md:4` | Project CLAUDE.md HEAD anchor stale at v2.11.1 (parent-of-repo file; out-of-tree) |
| LOW | checkout_hygiene | `docs/gate-logs/` | No v3.1.0 / v3.2.0 subdirs — CI artifact-only evidence |
| LOW | ci_hygiene | `Scripts/run_gate.ps1:109` | OpenSees FAIL arm doesn't echo $OsOut tail for context (only -RequireOpenSees path does) |
| NIT | stale_version | `Plugins/FrameSolver/Standalone/build_capi_v2.bat:2` | "B2 stub / B3 links" historical migration comment — now stable v3.x |
| NIT | doc_topology | `docs/README.md:45` | `specs/S5_S11_skeletons.md` note says "S11 仍是骨架" — shipped v3.1.0 has formal spec |

### Disposition

**Applied (3 in-place fixes — doc-only, parallel to Phase 5 audit pattern):**

- `docs/ARCHITECTURE.md:297,310` UE count 57 → 62 + add v3.2.0 FrameCoreUE test note
- `.github/workflows/release-gate.yml:3-6` header comment HANDOFF v2.11.1 → v3.2.0 + "9-leg
  verification matrix" → "standard 5-leg gate + v2 CPU dispatcher roundtrip"
- `docs/VERIFICATION.md:296` "3 of the 57-test gate count" → "62-test gate count (v3.2.0)"

These three are pure documentation drift introduced when v3.2 audit's scope was narrowed to
the source delta. They follow the same pattern as the Phase 5 README/VERIFICATION §1 bumps
already shipped in v3.2.0 (so applying them is "carry-along" not "out-of-scope").

**Deferred (14 findings):**

The remaining 14 findings would benefit from user triage before applying — they involve
either (a) gate-logs policy decisions, (b) doc reorganisation choices, (c) build system
backwards-compat trade-offs, or (d) the rule-#1-protected U-07 BLOCKER which was already
documented in HANDOFF_v3.2.0.md §3.2. The disposition table above lists each finding's file
and line for trivial location-by-location triage.

The U-07 BLOCKER carry-forward note is the most important one: when v3.3 work begins, the
engine source change (`StressField.h` default 0 → -1) should be one of the first items, and
will require an engine-side smoke test addition (a fixture with no governing element, e.g.,
all-zero loads) to verify the new sentinel propagates correctly through both the dispatcher
JSON path and the FrameCoreUE USTRUCT marshal.

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
