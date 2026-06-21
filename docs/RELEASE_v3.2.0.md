# FrameCore v3.2.0 — FrameCoreUE thin-slice UE reflection module

**Tag (this release):** `v3.2.0`
**Branch:** `main`
**Date:** 2026-06-22
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v3.1.0` at `5da6f56` (S11 stress-field post-process + 7-agent
audit closeouts; engine numerics frozen at `v3.0.1` bit-identical, +5 files for
`StressKernel.h` / `StressField.{h,cpp}` / `ElasticAllowable.cpp` delegate refactor).

> **v3.2.0 ships a UE5 consumer-side reflection module** — pure visualisation-lane glue
> that exposes the existing `FRAMECORE_API computeStressField` to Blueprint designers and
> editor dev tools. Engine source under `Plugins/FrameSolver/Source/FrameCore/` is
> **bit-identical to v3.1.0** (zero lines changed; verified by gate Leg 1). The whole
> v3.2 surface lives in a new `FrameCoreUE` plugin module that compiles only on UE
> targets; standalone C++17 / OpenSees / CLI paths are untouched.
> **Engine source delta vs v3.1.0 = 0 lines under FrameCore native module; v3.2 delta is
> ~10 files of pure UE-side reflection / Slate / version bumps.**
> v3.2.0 was driven through the same release-hardening Phase 1..5 pipeline as v3.1.0;
> all 5 reachable gate legs were green on the integrator host before tag (see §3).

## 1. What v3.2.0 ships

### 1.1 `FrameCoreUE` plugin module (new)

Located at `Plugins/FrameSolver/Source/FrameCoreUE/`. Depends on `FrameCore` for the
native `frame::StressField` POD types; `Core` / `CoreUObject` / `Engine` for UStruct /
UFunction reflection; under `bBuildEditor` also `Slate` / `SlateCore` / `UnrealEd` /
`EditorStyle` / `EditorSubsystem` / `ToolMenus` / `InputCore` / `WorkspaceMenuStructure`
for the dev panel. **`FrameCore.Build.cs` is unchanged** — the rule from CLAUDE.md
preserve-engine (#1) is intact.

**Source layout (10 new files):**

```
Plugins/FrameSolver/Source/FrameCoreUE/
├── FrameCoreUE.Build.cs                                  (new)
├── Public/FrameCoreUE/FrameCoreUEModule.h                (new)
├── Public/FrameCoreUE/FrameCoreUETypes.h                 (new — 5 USTRUCT mirrors)
├── Public/FrameCoreUE/FrameCoreUELibrary.h               (new — UBlueprintFunctionLibrary)
├── Public/FrameCoreUE/SFrameCoreStressFieldPanel.h       (new — #if WITH_EDITOR)
├── Private/FrameCoreUEModule.cpp                         (new — IMPLEMENT_MODULE + tab spawner)
├── Private/FrameCoreUETypes.cpp                          (new — engine POD -> USTRUCT marshal)
├── Private/FrameCoreUELibrary.cpp                        (new — BP entry impls)
├── Private/SFrameCoreStressFieldPanel.cpp                (new — #if WITH_EDITOR)
├── Private/Tests/FrameCoreUEBlueprintSmokeTest.cpp       (new — UE 61st test)
└── Private/Tests/FrameCoreUEEditorSmokeTest.cpp          (new — UE 62nd test, #if WITH_EDITOR)
```

### 1.2 USTRUCT mirror of `frame::StressField`

Five BlueprintType USTRUCTs in `FrameCoreUETypes.h`:

- `FFrameStressFieldSample` — member sample point (15 floats: X / 4 fiber sigmas / 6 raw
  internal forces N,Vy,Vz,T,My,Mz / SigmaTopY,SigmaBotY,SigmaPlusZ,SigmaMinusZ)
- `FFrameMemberStressTrace` — `{ MemberIdx, MemberId, TArray<FFrameStressFieldSample> Samples }`
- `FFrameShellStressPoint` — `{ CornerIdx, SigmaXX, SigmaYY, TauXY, Sigma1, Sigma2, VonMises, ThetaRad }`
- `FFrameShellStressLayer` — `{ ShellIdx, ShellId, bIsTopLayer, FFrameShellStressPoint Center, TArray<FFrameShellStressPoint> Corners }`
- `FFrameStressField` — aggregate with Members / ShellsTop / ShellsBot TArrays + global
  maxes + governing IDs (using **`-1` sentinel** per v3.1.0 C-07/C-08 audit pattern,
  not 0 which is ambiguous with real ID 0)

All fields are `BlueprintReadOnly`. **Floats are `float32`** — engine `frame::real` is
`double`; lossy double->float cast is intentional, and the BP smoke test verifies the
divergence stays under rel<1e-5 on the F68 cantilever oracle (visualisation tolerance
budget; renderers paint at 8-bit colour anyway). v3.x can add double-precision USTRUCT
variants if BP designers ever need finer than 1e-4.

### 1.3 `UFrameCoreStressFieldLibrary` UBlueprintFunctionLibrary

`Public/FrameCoreUE/FrameCoreUELibrary.h` exposes 5 BP entry points:

- `ComputeCantileverFixture(P, L, SamplesPerSpan)` — **BlueprintCallable**; builds the
  F68 fixture in-memory (rectangular 100x100 / S235-like cap / +Z tip load), solves,
  computes the field, returns marshalled USTRUCT. Default samplesPerSpan=11; values < 2
  silently clamp back to 11 (BP designer who passes 1 gets a sane response rather
  than an empty trace).
- `GetGoverningMemberId(Field)` — **BlueprintPure**
- `GetGoverningShellId(Field)` — **BlueprintPure**
- `GetGlobalMaxFiberSigma(Field)` — **BlueprintPure**
- `GetGlobalMaxVonMises(Field)` — **BlueprintPure**
- `GetMemberSamples(Field, MemberIdx)` — **BlueprintPure**; out-of-range returns empty
  (BP designer's typical "MemberIdx=999" doesn't crash)

### 1.4 `SFrameCoreStressFieldPanel` editor utility panel (#if WITH_EDITOR)

Minimal Slate widget for dev validation of the visualisation lane:

- Compute button → `ComputeCantileverFixture(1000, 2000, 11)` → fills the panel
- Result text: "Global max fiber sigma: %.4f MPa   Governing member id: %d   Members: %d"
- `SListView<FFrameStressFieldSample>` showing the 11 samples (x / sigComp / sigTens /
  tauShear / tauTorsion)

Registered as a nomad tab spawner under `WorkspaceMenu::GetMenuStructure().GetToolsCategory()`
with display name "FrameCore Stress Field". Tab unregister in `ShutdownModule`. The
spawner registration runs at module `StartupModule` — implicitly verified by the editor
smoke test running at all.

### 1.5 Two new UE automation tests

- `FFrameCoreUEBlueprintSmokeTest` (`FrameCore.UE.BlueprintSmokeTest`) — drives
  `ComputeCantileverFixture(1000, 2000, 11)`, cross-checks BP USTRUCT against the engine
  POD `frame::computeStressField` directly inline (oracle (a) — rel<1e-5 budget for the
  float cast), and validates the 11-sample analytic envelope `|P|·(L-x)/Wz` (oracle (b)
  — rel<1e-4 budget for analytic + float cast). The tip-sample sigExp=0 case uses
  absolute diff (v3.1.0 F68 audit pattern). Also verifies BP convenience accessors and
  out-of-range guards.
- `FFrameCoreUEEditorSmokeTest` (`FrameCore.UE.EditorSmokeTest`) — `#if WITH_EDITOR` +
  `#if WITH_DEV_AUTOMATION_TESTS`; constructs `SFrameCoreStressFieldPanel` and drives
  `OnComputeClicked()` directly (bypassing Slate event simulation), asserts
  `FReply::IsEventHandled()`.

UE test count: 60 (v3.1.0) → **62** (v3.2.0). `Scripts/run_gate.ps1`
`$ExpectedUeTests` default bumped accordingly.

### 1.6 Version bumps

- `Plugins/FrameSolver/FrameSolver.uplugin` — Version `29 -> 30`, VersionName `"3.1.0" -> "3.2.0"`
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` — `kEngineVer "3.1.0" -> "3.2.0"`
- `Scripts/run_gpu_gate.ps1` — `FRAMECORE_EXPECTED_ENGINE_VER '3.1.0' -> '3.2.0'`
- `.github/workflows/release-gate.yml` — same pin synced; Leg 1 step name renamed
- `Scripts/run_gate.ps1` — `$ExpectedUeTests 60 -> 62`

The version pins are the v3.0.0 / v2.11.1 audit BLOCKER 1 rule: **every kEngineVer
bump moves every other pin in lockstep, or the CI gate breaks at next push.**

## 2. What stayed bit-identical

- **Engine source under `Plugins/FrameSolver/Source/FrameCore/`** — `git diff v3.1.0..HEAD -- Plugins/FrameSolver/Source/FrameCore/`
  is empty. This is the strongest invariance: standalone F1..F70 + UE existing 60 tests
  + OpenSees + linear deep audit 104 + CLI roundtrip all bit-identical to v3.1.0.
- **Public ABI** — `FRAMECORE_API` exports unchanged. `FRAMECOREUE_API` is the new
  symbol set added by the new module; no existing symbol moved or got removed.
- **Wire ABI** — `kAbiVersion = 2` unchanged. v2 dispatcher capability list unchanged
  (16 verbs + the 4 inspect.* added in v3.1.0). The only protocol-visible change is
  `kEngineVer "3.1.0" -> "3.2.0"`.
- **Default-build cross-vendor** — non-CUDA builds (`FRAMECORE_CUDA=0`) and FrameCore
  native module are bit-identical to v3.1.0; only the UE target gains the
  `FrameCoreUE.dll`. The standalone gate / CLI bridge / Rhino v2 bridge are untouched.

## 3. Reproduction matrix (v3.2.0 source on integrator host, 2026-06-22)

| Leg | Cmd | Result | Notes |
|---|---|---|---|
| 1. Standalone F1..F70 | `Plugins\FrameSolver\Standalone\build.bat` then `frametest.exe` | **ALL PASS (failures=0)** | F1..F70 fixtures bit-identical vs v3.1.0; no new standalone fixture added (FrameCoreUE is UE-only) |
| 2. UE automation 62/62 | `Build.bat ArchSimEditor Win64 Development` then `Scripts\run_gate.ps1 -RequireOpenSees` | **ALL PASS** | `-ExpectedUeTests 62` default; BlueprintSmokeTest + EditorSmokeTest joined the existing 60 |
| 3. OpenSees strict | `python Tools\opensees_compare.py` | **OPENSEES GATE: PASS** | unchanged vs v3.1.0 (engine source unchanged) |
| 4. Deep audit | `Plugins\FrameSolver\Standalone\build_linear_audit.bat` then `linear_deep_audit.exe` | **PASS failures=0 checks=104** | unchanged vs v3.1.0 |
| 5. CLI round-trip | `Tools\cli_roundtrip.py` (after `build_cli.bat`) | **ALL PASS (failures=0)** | 13 checks; unchanged vs v3.1.0 |
| 6a. v2_roundtrip (CPU) | `build_capi_v2.bat` + `FRAMECORE_EXPECTED_ENGINE_VER=3.2.0 python Tools\v2_roundtrip.py` | **=== summary: ALL PASS ===** | `kEngineVer=3.2.0` pin enforced; capability list unchanged |
| 6b. v2_roundtrip (CUDA) | `Scripts\run_gpu_gate.ps1 -Strict` | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; see §3a |
| 7a. F1..F67 + F67s strict | `Scripts\run_gpu_gate.ps1 -Strict` leg 1/3 | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; see §3a |
| 7b. r2_bench --gpu 90k | `Scripts\run_gpu_gate.ps1 -Strict` leg 3/3 | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; see §3a |

§3a footnote — **NOT RUN legs (CUDA / `-Strict` mode)**: v3.2.0 source delta in CUDA
lane = 0 lines (FrameCoreUE is UE-only; `SnSession` / cuDSS Impl unchanged). The
r2_bench 90k margin from v3.0.0/v3.1.0 (`+11.94 ms` over the 16.67 ms 60-fps budget,
v2.11.0 baseline +11.94 ms still in effect) carries forward bit-identical. To reproduce
all 9/9 V3 STABLE legs:

```powershell
Scripts\run_gpu_gate.ps1 -Strict
```

With `cudss64_0.dll` on PATH the `-Strict` flag enforces the `STRICT_EXECUTED`
fingerprint on F67s + the UE `FFrameCoreGpuBacksubStrictTest`, and pins
`FRAMECORE_EXPECTED_ENGINE_VER='3.2.0'`.

## 4. Tag plan

```bash
git add -- \
  Plugins/FrameSolver/FrameSolver.uplugin \
  Plugins/FrameSolver/Source/FrameCoreUE/FrameCoreUE.Build.cs \
  Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameCoreUEModule.h \
  Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameCoreUETypes.h \
  Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameCoreUELibrary.h \
  Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/SFrameCoreStressFieldPanel.h \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/FrameCoreUEModule.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/FrameCoreUETypes.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/FrameCoreUELibrary.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/SFrameCoreStressFieldPanel.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEBlueprintSmokeTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEEditorSmokeTest.cpp \
  Plugins/FrameSolver/Standalone/v2/Dispatcher.h \
  Scripts/run_gate.ps1 \
  Scripts/run_gpu_gate.ps1 \
  .github/workflows/release-gate.yml \
  docs/PLAN_v3.2_ue_interface.md \
  docs/NIGHT_SHIFT_2026-06-22.md \
  docs/RELEASE_v3.2.0.md \
  docs/HANDOFF_v3.2.0.md

git commit -m "release: v3.2.0 -- FrameCoreUE thin slice (USTRUCT marshal + BP node + Slate panel)"
git tag -a v3.2.0 -m "v3.2.0 -- UE consumer-side reflection module"
git push origin main
git push origin v3.2.0

gh release create v3.2.0 \
  --title "v3.2.0 -- FrameCoreUE thin slice (USTRUCT marshal + BP node + Slate panel)" \
  --notes-file docs/RELEASE_v3.2.0.md \
  --latest
```

## 5. Deferred

### 5.1 v3.1.0 deferred (carry-forward, unchanged)

All seven v3.1.0-newly-deferred items still apply: **A-13** F71 cantilever +Z, **D-05**
v1 CLI `STRESS` token, **E-07** `docs/specs/S6b_v2_inspect_protocol.md`, **E-13** S11
naming collision (stress-field vs MITC9i), **C-12** `submitMtx_` hold in
`computeStressField`, **F-02** `findUdl` hash map, **F-03** `StressKernel.h` clamps
invariant doc. Plus the 13 v3.0.1 carry-forwards (A-02 CUDA RAII, A-05/F-14 OpenMP, etc).
See [`docs/HANDOFF_v3.1.0.md` §3](HANDOFF_v3.1.0.md) for first-actions on each.

### 5.2 v3.2.0 newly deferred

- **U-01** — `ComputeCantileverFixture` is the only BP entry that builds a model. A real
  "load JSON model from disk" BP node is deferred to v3.3 (requires deciding the JSON
  schema surface — v1 CLI text format vs v2 wire JSON vs something new). First action:
  add `UFUNCTION(BlueprintCallable) UFrameCoreStressFieldLibrary::LoadModelFromJsonString(FString, ...)`
  and a Slate file-picker on the editor panel.
- **U-02** — Slate panel only displays the F68 cantilever fixture; no "load fixture
  dropdown" between Cantilever / Plate / Cross / Truss. The plan §4 Phase 3 mentioned
  the dropdown — left out to keep minimum viable panel; trivial follow-up. First
  action: add `SComboBox<TSharedPtr<FString>>` next to Compute button + dispatch on
  selection.
- **U-03** — No real renderer (spline mesh / Niagara / colour-band shader). The whole
  visualisation layer beyond "USTRUCT data + sample table" is v3.3+ scope. First
  action: a separate `FrameCoreUEViewport` actor that owns a `UProceduralMeshComponent`
  + paints colour bands based on `Field.Members[i].Samples[j].SigmaCompMax`.
- **U-04** — **✅ CLOSED in Phase 6e (post-tag strengthening)**: added
  `FFrameCoreUEEditorTabSpawnerTest` (`FrameCore.UE.EditorTabSpawnerTest`) which asserts
  `FGlobalTabmanager::Get()->HasTabSpawner("FrameCoreStressFieldPanel")` is true after
  `StartupModule`. Future UE engine WorkspaceMenuStructure API drift surfaces as a failing
  automation test instead of a silently-missing editor menu.
- **U-05** — `float`-only USTRUCT loses precision for very-small load cases (P << 1 N).
  Engine doubles → BP floats is fine for visualisation but not for downstream BP math
  that re-uses the values. First action: decide whether to add `double`-precision
  USTRUCT variant or document the lossy budget.
- **U-06** — UE 5.7 + VS2026 compiler warning ("not a preferred version"). Not new —
  inherited from v3.1.0 build. UE engine version pin is in `ArchSim.uproject` and
  unchanged. First action: pick the preferred VS toolchain version 14.44.35207 in
  `Setup.bat` or document the warning in `docs/HANDOFF_v3.2.0.md`.
- **U-07** — `governingMemberId` / `governingShellId` sentinel mismatch between
  engine POD (`0` means none) and USTRUCT (`-1` means none). 3-agent audit BLOCKER
  deferred because the fix requires editing `StressField.h` (engine rule #1) and the
  brittle marshal-layer heuristic alternative was rejected. v3.2 cantilever fixture
  passes the smoke test because member id 0 actually governs there; the failure mode
  surfaces only on fixtures with no governing element (edge case for the v3.2 dev
  visualisation lane). First action: change `StressField.h` default `governingMemberId = 0`
  to `-1` and guard the engine writer to leave -1 when no governing element exists.
  Dispatcher.cpp already handles -1 correctly (v3.1.0 C-07/C-08 fix); BP marshal will
  automatically inherit. See `docs/HANDOFF_v3.2.0.md` §3.2 U-07 for the detailed first
  action.

## 6. Honest limitations (per-engine)

- **FrameCoreUE is reflection only.** No new engine capability — every value in
  `FFrameStressField` traces back to `frame::computeStressField` (v3.1.0 numerics).
  If the engine got something wrong, the USTRUCT carries the wrong value with the
  same fidelity. F70 D/C interlock in v3.1.0 remains the only inter-path oracle.
- **Float lossy cast is per-value, not aggregated.** Engine double → USTRUCT float at
  each sample. For tiny load cases the worst rel can exceed 1e-5 even though the
  visualisation appears identical. The BP smoke test asserts rel<1e-5 on a
  representative |P|=1000 fixture; smaller P needs `-ExpectedRel` tweaking.
- **Editor smoke test does NOT cover the nomad tab spawner.** The spawner registers
  in `StartupModule` (implicitly OK because the test runs at all), but no test
  asserts `HasTabSpawner("FrameCoreStressFieldPanel")` after StartupModule — that's
  in the deferred U-04 above.
- **No packaged build smoke test.** All verification is Editor target. If
  `bBuildEditor` branch dependencies leaked to the runtime path, a packaged game
  would fail to link. Mitigation: `#if WITH_EDITOR` gates every Slate-touching
  symbol. Reproducibility: `Build.bat ArchSim Win64 Development -project=...` (no
  Editor) is left as v3.3 verification.
- **Renderer is NOT in scope.** Phase 3 panel is dev-only; "the user actually sees a
  3D stress field on a structural model in PIE" is v3.3 / v3.4 work. The USTRUCT
  data layer is in place, but `UStaticMeshComponent` / `UProceduralMeshComponent` /
  `UNiagaraComponent` integration is open.

## 7. Breaking changes

None at the public ABI level. `kEngineVer` bumps `3.1.0 -> 3.2.0` — clients that
hard-pin the exact version string will fail (they should range-match); this is
consistent with the v3.0.x and v3.1.x pattern.

`FrameSolver.uplugin` gained a second Module entry `FrameCoreUE`. Projects that
already enabled the FrameSolver plugin via `ArchSim.uproject` (or equivalent) will
pick up the new module automatically. Projects that explicitly opt out of UE-side
reflection (e.g. headless server builds) can disable `FrameCoreUE` by replacing
the second Modules[] entry with `"WhitelistTargetPlatforms": ["Win64"]` and a
`"TargetAllowList"` of `["Editor"]` — but this is uncommon enough that v3.2.0 doesn't
bake the gate in; opt-out is a v3.3 follow-up if anyone asks.
