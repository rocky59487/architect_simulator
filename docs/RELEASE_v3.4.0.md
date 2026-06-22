# Release v3.4.0 — Karamba3D-parity numerical surface for UE5 Blueprint

**Tag:** `v3.4.0` (2026-06-22)
**Type:** Feature — UE BP surface expansion + 3-line additive engine facade
**Source delta vs v3.3.0:** 3 engine lines (`FrameModel.h` `FRAMECORE_API` annotations) +
17 new UE-only files under `Plugins/FrameSolver/Source/FrameCoreUE/`.

## TL;DR

v3.4 closes the engine-API-vs-UE-Blueprint gap. After this release, every numerical
capability the C++ engine exposes via `FRAMECORE_API` has a matching
`UFUNCTION(BlueprintCallable)` entry through **`UFrameAnalysisLibrary`**, every C++
result POD has a matching `USTRUCT(BlueprintType)` mirror, and every option struct is
BP-editable. A Blueprint designer can now build a model, tune any opt-in flag, run any
analysis (linear / modal / buckling / response spectrum / load envelope / influence line
/ reanalysis / P-Delta / tension-only / size-opt / BESO / co-rotational / arc-length /
dynamic-collapse), and read the full result without writing a line of C++.

This is a **non-breaking** release on the wire side. The v2 dispatcher schema is
identical to v3.3.0 (no new dispatcher methods, 23 capabilities unchanged); `kEngineVer`
bumps to `"3.4.0"` only as a UE-side consumer-surface marker so clients can pin /
capability-gate against the matching `FrameCoreUE.dll`. Pre-v3.4 clients that don't
touch UE Blueprint are unaffected.

## What changed

### v3.4 Karamba3D-parity numerical surface

**17 input USTRUCT** under `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/`:

```
FFrameCapacity + FFrameMaterial + FFrameSection + FFrameNode + FFrameMember +
FFrameShellQuad + FFrameNodalLoad + FFrameMemberUDL + FFrameShellPressure +
FFrameSolveOptions + FFramePDeltaOptions + FFrameTensionOnlyOptions +
FFrameCollapseOptions + FFrameCorotationalOptions + FFrameDynCollapseOptions +
FFrameSizeOptOptions (+ FFrameSizeOptLoadCase) + FFrameBESOOptions +
FFrameReanalysisOptions + FFrameModelDef (aggregate)
+ enums EFrameSectionShape, EFrameReleasePreset
```

**9 output USTRUCT** for `SolveResult` + sub-types:

```
FFrameNodalDisplacement + FFrameNodalReaction (NaN-at-free-DOF collapsed to 0 with
  bHasConstrainedDof flag) +
FFrameMemberEndForces + FFrameMemberInternalForces +
FFrameDemandResult + FFrameMemberUtilization (ElasticAllowable per-member D/C) +
FFrameShellInternalForces (per-corner MxxC/MyyC/MxyC TArrays) +
FFrameShellUtilization (checkShellSurface per-shell) +
FFrameDemandSummary (worstUtilization + worstShellUtilization governing idx resolved)
+ FFrameSolveResult (top-level)
+ enum EFrameFailMode
```

**13 analysis-specific USTRUCT** for Phase 3+4 result POD mirrors:

```
FFrameModalOptions + FFrameModeShape + FFrameModalResult
FFrameBucklingOptions + FFrameBucklingResult
EFrameSpectrumCombo + FFrameSpectrum + FFrameResponseSpectrumResult
FFrameModalDynamicsOptions + FFrameModalTimeStep + FFrameModalTimeHistory
FFrameLoadEnvelope + FFrameInfluenceLine
FFramePDeltaResult + FFrameTensionOnlyResult + FFrameSizeOptResult
EFrameBESOStop + FFrameBESOResult
FFrameCorotationalResult (arc-length is opt-in via opts.bUseArcLength)
EFrameDynCollapseOutcome + FFrameFragmentCluster + FFrameCollapseHingeEvent +
FFrameDynCollapseEvent + FFrameDynCollapseFrame + FFrameDynCollapseResult
```

**Library entry points**:

- `UFrameModelBuilder::ValidateModel(Def, OutError)` -- runs `FrameModel::validate`.
- `UFrameModelBuilder::LoadModelFromJson(JsonPath, OutError) -> FFrameModelDef` --
  parses the dispatcher's `model.set` JSON schema subset into a populated USTRUCT.
- `UFrameMaterialLibrary` -- S235/S275/S355/S460 + ConcreteC30/C40/C50 + Aluminum6061
  + `MakeCustomMaterial(E, G, Rho, Nu, Fy, CapComp, CapTens, CapShear)`.
- `UFrameSectionLibrary::MakeRectangular(W, D)` / `MakeCircular(D)` -- delegate engine
  factory so analytic values are bit-identical with `frame::Section::Rectangular/Circular`.
- `UFrameAnalysisLibrary` -- 15 BP entries spanning every linear + nonlinear analysis:
  - **Linear (8)**: SolveLinear, AnalysisModal, AnalysisBuckling, LoadCombineEnvelope,
    InfluenceLine (uses `reactionInfluenceLine`), ResponseSpectrum,
    RealTimeDynamic (modal step-response), ReanalysisSolve (stateless library form).
  - **Nonlinear (7)**: SolvePDelta, SolveTensionOnly, SolveSizeOpt, SolveBESO,
    SolveCorotational, SolveArcLength (thin wrapper forcing `opts.bUseArcLength=true`),
    SolveDynCollapse (blocking post-run; live-stream callbacks deferred to v3.5).

### Engine source delta (3 lines)

```diff
 // Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/FrameModel.h
-    int nodeIndex(NodeId id) const;
-    int memberIndex(MemberId id) const;
-    int shellIndex(int id) const;
+    FRAMECORE_API int nodeIndex(NodeId id) const;
+    FRAMECORE_API int memberIndex(MemberId id) const;
+    FRAMECORE_API int shellIndex(int id) const;
```

These three id->idx helpers were already public per `FrameModel::validate` semantics,
but the symbols never crossed the `UnrealEditor-FrameCore.dll` boundary. The
consumer-side `FrameCoreUE` module marshals `SolveResult` per-node / per-member by
looking up slot indices via these helpers; without the `FRAMECORE_API` facade the
linker emits `LNK2019 unresolved external symbol`. Per the v3.4 spec line 47
allowance, adding `FRAMECORE_API` to existing public-API functions counts as
"trivial engine delta" -- impl unchanged, standalone `F1..F71` is bit-identical
with v3.3.0.

## Verification matrix

| Leg | Result | Reproduce |
|---|---|---|
| 1. Standalone F1..F71 | **ALL PASS** | `Plugins\FrameSolver\Standalone\build.bat` (bit-identical vs v3.3.0; engine impl unchanged). |
| 2. UE automation | **98/98 PASS** (96/96 without cuDSS) | rebuild via `%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project="%CD%\ArchSim.uproject" -waitmutex` then `UnrealEditor-Cmd ArchSim.uproject -ExecCmds="Automation RunTests FrameCore; Quit" -log -unattended -NullRHI -nosplash -stdout -nopause`. Pass `-ExpectedUeTests 96` in non-cuDSS build. |
| 3. OpenSees | **PASS** (`--relaxed`; cross-platform drift mode) | `python Tools\opensees_compare.py --relaxed` (`openseespy` in system Python or the `framecore-direct` conda env). 3D co-rotational cantilever rel<1.22e-9; shallow-arch snap-through limit-load rel<6.4e-3. |
| 4. Deep audit (104 checks) | **`PASS failures=0 checks=104`** | `Plugins\FrameSolver\Standalone\build_linear_audit.bat && Plugins\FrameSolver\Standalone\linear_deep_audit.exe`. |
| 5. CLI round-trip (13 checks) | **ALL PASS** | `Plugins\FrameSolver\Standalone\build_cli.bat && python Tools\cli_roundtrip.py`. |
| 6a. v2_roundtrip CPU | **`=== summary: ALL PASS ===`** | `Plugins\FrameSolver\Standalone\build_capi_v2.bat && python Tools\v2_roundtrip.py` -- wire schema unchanged from v3.3.0 (no new dispatcher methods); `kEngineVer="3.4.0"` pin enforced; 23 capabilities advertised. |
| 6b. v2_roundtrip CUDA | NOT RUN this session | `Scripts\run_gpu_gate.ps1 -Strict` -- CUDA path source delta = 0 lines, v3.0.0 / v3.1.0 GPU evidence carries forward (`r2_bench --gpu 90k margin +11.939 ms`, `F67s STRICT_EXECUTED` fingerprint). |
| One-shot 5-leg | one-shot reproduce | `powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees` (default `$ExpectedUeTests = 98`; pass `-ExpectedUeTests 96` on non-cuDSS box). |

Engine-side honesty: 5 of 6 CPU legs ran green on the integrator's host
(standalone F1..F71 + UE 98/98 + OpenSees + audit 104 + CLI 13 + v2_roundtrip CPU).
The GPU leg (6b) is `NOT RUN` deliberately: v3.4 has zero source delta in the CUDA
path (only 3 FRAMECORE_API additive annotations on FrameModel.h, none of which touch
the cuDSS pipeline), so the v3.0.0 / v3.1.0 GPU evidence carries forward unchanged.
The CI workflow `.github/workflows/release-gate.yml` re-runs legs 1, 4, 5, 6a on
every push to main + tag.

## Karamba3D parity self-assessment

| Karamba3D Tab | v3.4 entry |
|---|---|
| Setup    | `UFrameModelBuilder::ValidateModel` / `LoadModelFromJson` + `FFrameModelDef` |
| Material | `UFrameMaterialLibrary` (8 presets + custom) |
| Section  | `UFrameSectionLibrary::MakeRectangular` / `MakeCircular` |
| Element  | `FFrameMember` / `FFrameShellQuad` USTRUCT |
| Load     | `FFrameNodalLoad` / `FFrameMemberUDL` / `FFrameShellPressure` USTRUCT |
| Analyze (linear)    | `SolveLinear` / `AnalysisModal` / `AnalysisBuckling` / `ResponseSpectrum` / `RealTimeDynamic` |
| Analyze (nonlinear) | `SolvePDelta` / `SolveTensionOnly` / `SolveCorotational` / `SolveArcLength` / `SolveDynCollapse` |
| Inspect  | `FFrameSolveResult` + `FFrameModalResult` + ... (Phase 2/3/4 result USTRUCT) |
| Algorithms | `SolveSizeOpt` (FSD) / `SolveBESO` |
| Interactive | `ReanalysisSolve` (stateless form; v3.5 `UFrameInteractiveSubsystem` stateful) |
| Utility  | `UFrameMaterialLibrary` / `UFrameSectionLibrary` |
| Display  | Deferred to v3.5 visual surface (per-result actors, Chaos POD integration) |

**11 of 12 Karamba3D tabs covered**. Display is deliberately deferred to v3.5
(`docs/specs/UE5_visual_surface_map.md`); v3.4 is the numerical surface only.

## Honest boundaries (engine limits NOT lifted by v3.4)

v3.4 surfaces the engine's existing analyses through Blueprint; it does NOT improve
the engine's underlying numerical scope. Every honest boundary still applies, and the
new BP function documentation links back here:

- **D/C utilization is elastic** -- the `ElasticAllowable` screen the engine ships
  is an allowable-stress D/C ratio, NOT an RC ultimate-strength check. Per-member
  `FFrameMemberUtilization` and per-shell `FFrameShellUtilization` carry the
  elastic D/C only; design-code interaction equations are out of scope.
- **DynamicCollapse is LSP-level sequential linear** -- each event is linear elastic,
  with a scalar `dlf` capturing dynamic amplification. Literature places LSP at ~±30%
  conservative vs full nonlinear dynamics.
- **Modal / Buckling / ResponseSpectrum are all linear** -- linearised eigenproblems
  on the assembled stiffness + mass / geometric stiffness. No nonlinear modal /
  nonlinear post-buckling.
- **Plastic hinges are event-to-event** -- no unloading / reversal; uniaxial Mp;
  optional N-M interaction is the rectangular plastic envelope (EC3 §6.2.9 exact
  for rectangular, conservative for circular), NOT AISC H1.1.
- **No fiber section / pushover** -- deliberately excluded from the engine scope.
- **MITC4 is flat-facet** -- curvature error is `O(1/N^2)` (8 facets/circle gives
  ~7.6% hoop error; 32 facets gives ~0.5%). Mesh refinement remains the user's job.
- **Co-rotational shells are small-strain** -- the linear `kl_` element is treated
  as a black box that rotates rigidly with the facet frame. No finite-membrane-strain
  shell.

These appear in `UFUNCTION` doc comments so a BP designer sees them in the editor
tooltip, not just in the spec.

## Breaking changes

**None.** v3.4 is purely additive on the UE side. Pre-v3.4 clients (Grasshopper, Rhino,
the v2 dispatcher) see no schema changes; `kEngineVer` bumps to `"3.4.0"` for the
consumer-side version-pin contract only.

## Migration (v3.3.0 -> v3.4.0)

- **Wire / CLI / Grasshopper clients**: no change. Re-pin `kEngineVer = "3.4.0"`
  in clients that hard-check the engine version, otherwise nothing.
- **UE Blueprint graphs built on v3.3 BP surface**: no change. v3.3 BP surface
  (`UFrameCoreStressFieldLibrary` + `FFrameStressField*`) is unchanged. v3.4 adds
  parallel `UFrameAnalysisLibrary` + `FFrame*` types that the v3.3 surface didn't
  touch -- mix as desired.
- **Plugin Version pin**: `FrameSolver.uplugin` Version 31 -> 32, VersionName 3.3.0
  -> 3.4.0. UE projects with hard `Version` pins refresh; soft `VersionName` lookups
  continue to work.

## Deferred to v3.5

v3.5 is the **visual surface** -- per-result actors + Chaos POD bridge + interactive
GameInstanceSubsystem. None of these are in scope for v3.4. The handoff lists the
Phase 1 first-actions for v3.5; see [docs/specs/UE5_visual_surface_map.md](specs/UE5_visual_surface_map.md).

- **V35-01** Deformed shape actor (consumes `FFrameSolveResult::Displacements`).
- **V35-02** D/C heat-map actor (consumes `FFrameSolveResult::MemberUtilization` /
  `ShellUtilization`).
- **V35-03** Stress-field renderer port from v3.3 `AFrameCoreStressFieldActor` to the
  v3.4 USTRUCT input pipeline.
- **V35-04** DynCollapse replay actor (consumes `FFrameDynCollapseResult::Frames`;
  live-stream callback via UE delegate replaces v3.4's blocking post-run).
- **V35-05** Chaos POD destruction bridge (consumes `FFrameDynCollapseEvent::Detached`).
- **V35-06** `UFrameInteractiveSubsystem` (stateful S1 `ReSolveSession` BP wrapper).
- **V35-07** v3.4 USTRUCT validation pass (sanity-check the v3.4 schema before
  v3.5 actor consumers depend on it).

Engine-side deferred (v3.6+ minor-patch territory; no commitment):

- **V36-01** `Section::Shape::IBeam / HSS / CircularHollow` (BP-side analytic A/I/J/Z
  works today via `MakeCustomSection`-style fluent helpers, but the engine `Shape`
  enum still only has Rectangular/Circular -- adding new shapes touches
  `ElasticAllowable` biaxial-bending formulae and the stress kernel; deliberate
  defer to a release cycle that has bandwidth for an engine-side change).

## Tag plan

```bash
git tag -a v3.4.0 -m "v3.4.0 -- Karamba3D-parity numerical surface for UE5 Blueprint"
git push origin main
git push origin v3.4.0
gh release create v3.4.0 --title "v3.4.0 -- Karamba3D-parity numerical surface" \
                          --notes-file docs/RELEASE_v3.4.0.md
```

Bundle zip (same composition as v3.3.0):

```
framecore-v3.4.0-win64.zip
├── frame_capi.dll          (v1 C-API; OpenBLAS runtime link)
├── frame_capi_v2.dll       (v2 dispatcher; 23 capabilities)
├── frame_cli.exe           (text bridge)
├── frametest.exe           (standalone F1..F71)
├── openblas.dll            (BLAS runtime)
├── LICENSE
└── README.txt              (release-specific quick start)
```

## Repo URL

[github.com/rocky59487/architect_simulator](https://github.com/rocky59487/architect_simulator)
