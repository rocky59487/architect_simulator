# UE5 Engine Surface Map — v3.4 spec

> Status: **design locked** 2026-06-22 (HEAD `v3.3.0`). Implementation pending.
> Companion spec: [`UE5_visual_surface_map.md`](UE5_visual_surface_map.md) (v3.5).
> Cross-link bridge: [`docs/HANDOFF_v3.4_v3.5_design.md`](../HANDOFF_v3.4_v3.5_design.md).

## Purpose

v3.4 closes the engine-API-vs-UE-BP gap. After v3.4, every numerical capability
the C++ engine exposes has a matching `UFUNCTION(BlueprintCallable)` entry and a
matching `USTRUCT(BlueprintType)` payload, so a Blueprint designer can:

1. **Build a `FrameModel` from BP** (nodes, members, materials, sections, loads)
   without writing JSON.
2. **Run any analysis** (`SolveLinear`, `SolvePDelta`, `SolveTensionOnly`,
   `SolveSizeOpt`, `SolveBESO`, `SolveCorotational`, `SolveArcLength`,
   `SolveDynCollapse`, `AnalysisModal`, `AnalysisBuckling`,
   `ResponseSpectrum`, `RealTimeDynamic`, `ReanalysisSolve` (S1 ReSolve)) and
   read its **full result** as USTRUCT (not just stress field).
3. **Tune any opt-in flag** (`useDKQPlate`, `useQM6Membrane`,
   `shellGeometricStiffness`, `shellCorotational`, `warpTolerance`,
   `useWarpingCorrection`, `CollapseOptions::nmInteraction`, `irSteps`,
   `useSnSession`, `useGpuBacksub`) from BP.

v3.4 is the **numerical surface**. Renderers (deformed shape actor, D/C heat-map
actor, dyn-collapse replay actor, Chaos POD integration) all land in v3.5; v3.4
ships zero new actors beyond v3.3's `AFrameCoreStressFieldActor`.

## Non-goals (v3.4)

- Visual renderers — deferred to v3.5.
- Chaos POD integration — deferred to v3.5.
- Real-time interactive `UFrameInteractiveSubsystem` — deferred to v3.5 (the
  Phase 7 in this doc only exposes `ReanalysisSolve` as a stateless library
  function; subsystem lifetime + GameInstance integration is v3.5 work).
- BP-side `ShellPressure` if the engine struct semantics differ across modes —
  may slip to v3.5 if validation surfaces an inconsistency.
- New analyses — every capability listed below already exists in the C++ engine
  (S1–S10 + v3 surface line + v3.1 stress field). v3.4 does **not** add new
  algorithms; it ONLY adds the UE surface.

## Engine source delta budget

**Target: 0 lines under `Plugins/FrameSolver/Source/FrameCore/`**. Every engine
capability v3.4 surfaces already has a `FRAMECORE_API` entry point. If during
implementation a capability turns out to need a missing facade
(`FRAMECORE_API` header annotation), the additive `FRAMECORE_API` on existing
function is acceptable and counts as "trivial engine delta"; rewriting any
algorithm or struct under `FrameCore/` is **out of scope** and a v3.4 abort
signal.

CUDA path delta: 0 lines (the GPU lane is already gated by `FRAMECORE_CUDA`;
v3.4 surfaces existing `useGpuBacksub` via `FFrameSolveOptions::bUseGpuBacksub`,
no new GPU code).

## Wire / version contract

- `kEngineVer` "3.3.0" → "3.4.0" (minor bump; capabilities added, no schema
  break vs v3.3).
- v2 dispatcher capability list grows from 23 → 23 (no new dispatcher methods;
  v3.4 work is UE-side, not dispatcher-side). Wire ABI unchanged.
- `FrameSolver.uplugin` Version 31 → 32, VersionName 3.3.0 → 3.4.0.
- `FRAMECORE_EXPECTED_ENGINE_VER` '3.3.0' → '3.4.0' in `run_gpu_gate.ps1` +
  `.github/workflows/release-gate.yml`.
- `run_gate.ps1 $ExpectedUeTests` 72 → ~95 (each analysis adds 2-3 UE tests).

## Phase tree

### Phase 0 — Spec + task tree (this doc + handoff bridge)

- This file.
- `docs/specs/UE5_visual_surface_map.md` (v3.5 spec).
- `docs/HANDOFF_v3.4_v3.5_design.md` (cross-link bridge).
- Estimated: 1 hr (already done as part of v3.3 wrap-up; no Phase-0 commit
  needed at v3.4 implementation start since the spec already lives in main).

### Phase 1 — Input USTRUCT (FrameModel BP-side build)

Mirror every `frame::*` POD type from `Plugins/FrameSolver/Source/FrameCore/
Public/FrameCore/*.h` as a `USTRUCT(BlueprintType)` under
`FrameCoreUE/FrameCoreUETypes.h` (or a new
`FrameCoreUEModelTypes.h` if the file grows too large; split is acceptable as
long as both ship from `Public/FrameCoreUE/`).

Required structs:

| Engine POD | USTRUCT | Notes |
|---|---|---|
| `frame::Node` | `FFrameNode` | id (int32), x/y/z (double or float — match `FFrameStressFieldSample` precision choice), fixed (bool[6]) → `TArray<bool>` with len-6 guard, prescribed (real[6]) |
| `frame::Member` | `FFrameMember` | id, i/j (NodeId), matIdx, secIdx, refVec (FVector), tensionOnly (bool), active (bool), release (bool[12]) |
| `frame::Material` | `FFrameMaterial` | E, G, rho, nu, fy, cap.comp/tens/shear |
| `frame::Section` | `FFrameSection` | A, Iy, Iz, J, cy, cz, Asy, Asz, Zy, Zz, shape (enum rectangular/circular) |
| `frame::ShellQuad` | `FFrameShellQuad` | id, n[4] (NodeId), matIdx, t, active |
| `frame::NodalLoad` | `FFrameNodalLoad` | node (NodeId), comp (real[6]) |
| `frame::MemberUDL` | `FFrameMemberUDL` | member (MemberId), w_local (FVector) |
| `frame::ShellPressure` | `FFrameShellPressure` | shell (ShellId), p (real) |
| `frame::SolveOptions` | `FFrameSolveOptions` | every opt-in flag (`bUseDKQPlate`, `bUseQM6Membrane`, `bShellGeometricStiffness`, `WarpTolerance`, `bUseWarpingCorrection`, `bUseSupernodalPrimary`, `bUseSnSession`, `bUseGpuBacksub`, `IrSteps`, `IrTol`, `bEnableReleases`, `bSkipForceRecovery`, `bParallelRhs`) |

Plus the analysis-specific options:

| Engine struct | USTRUCT | Used by |
|---|---|---|
| `frame::PDeltaOptions` | `FFramePDeltaOptions` | `SolvePDelta` |
| `frame::CollapseOptions` | `FFrameCollapseOptions` | sequential-linear collapse |
| `frame::CorotationalOptions` | `FFrameCorotationalOptions` | `SolveCorotational` |
| `frame::ArcLengthOptions` | `FFrameArcLengthOptions` | `SolveArcLength` |
| `frame::DynCollapseOptions` | `FFrameDynCollapseOptions` | `SolveDynCollapse` (callbacks: skip — `onFrameEmitted`/`isCancelled` are v3.5 InteractiveSubsystem-bound) |
| `frame::SizeOptOptions` | `FFrameSizeOptOptions` | `SolveSizeOpt` |
| `frame::BESOOptions` | `FFrameBESOOptions` | `SolveBESO` |
| `frame::ReSolveOptions` | `FFrameReSolveOptions` | `ReanalysisSolve` |

Then add `UFrameModelBuilder` (UBlueprintFunctionLibrary):

- `static UFrameModel* CreateModel(const FFrameModelDef& Def)` where
  `FFrameModelDef` aggregates the arrays above. **Or** if we want immutability +
  validation, expose builder fluent helpers: `AddNode / AddMember / AddMaterial
  / AddSection / AddShell / AddNodalLoad / AddMemberUDL / AddShellPressure /
  Build`. Decide at Phase-1 implementation start which pattern reads better in
  a BP graph; the immutable aggregate is simpler and aligns with the existing
  `ComputeFromJsonModel` shape.
- `static bool ValidateModel(const FFrameModelDef& Def, FString& OutError)`.
- `static FFrameModelDef LoadModelFromJson(const FString& JsonPath, FString&
  OutError)` — refactors v3.3's `ComputeFromJsonModel` JSON parser out into a
  reusable BP-callable that returns the structural data, not just the stress
  field. v3.3's `ComputeFromJsonModel` keeps its signature for backward compat
  and internally calls `LoadModelFromJson` + `SolveLinear` + `computeStressField`.

`UFrameMaterialLibrary` (UBlueprintFunctionLibrary, static accessors):

- `static FFrameMaterial GetS235() / GetS275() / GetS355() / GetS460()`
  (structural steel grades, units MPa).
- `static FFrameMaterial GetConcreteC30() / GetConcreteC40() / GetConcreteC50()`
  (concrete grades; nu = 0.2, rho = 2400 kg/m^3).
- `static FFrameMaterial GetAluminum6061()` (light alloy demo).
- `static FFrameMaterial MakeCustomMaterial(float E, float G, float rho, float
  nu, float fy, FFrameCapacity Cap)`.

`UFrameSectionLibrary` (UBlueprintFunctionLibrary):

- `static FFrameSection MakeRectangular(float Width, float Depth)`.
- `static FFrameSection MakeCircular(float Diameter)`.
- `static FFrameSection MakeIBeam(float Height, float Flange, float Web,
  float Tw, float Tf)` (computes A/Iy/Iz/J/cy/cz analytically; matches engine
  Section constructors).
- `static FFrameSection MakeHSS(float Width, float Depth, float Thickness)`.
- `static FFrameSection MakeCircularHollow(float Outer, float Inner)`.

Tests (`FrameCoreUE.ModelBuilder.*`):

1. **BuildAndValidate**: build cantilever from BP → validate → assert ok.
2. **InvalidMemberRef**: build a member referencing a non-existent node → assert
   validate returns false with diagnostic.
3. **LibraryPresets**: every material/section preset returns finite positive
   values + `MakeRectangular(100, 100)` matches engine `Section::Rectangular(100,
   100)` bit-exact (rel < 1e-12).

Estimated Phase 1 cost: 8 hr.

### Phase 2 — Output USTRUCT (SolveResult marshal)

Mirror `frame::SolveResult` and its sub-types:

| Engine POD | USTRUCT |
|---|---|
| `frame::SolveResult` (top level) | `FFrameSolveResult` |
| disp (real[nNodes * 6]) | `TArray<FFrameNodalDisplacement>` (per-node {Ux, Uy, Uz, Rx, Ry, Rz}) |
| reactions (real[nNodes * 6], NaN at free DOFs) | `TArray<FFrameNodalReaction>` |
| memberForces (per member: endI/endJ MemberEndForces) | `TArray<FFrameMemberInternalForces>` (per member: I/J with N/Vy/Vz/T/My/Mz) |
| memberUtilization (per member: DemandResult endI/endJ + governing + peak) | `TArray<FFrameMemberUtilization>` |
| shellForces (per shell: Nxx/Nyy/Nxy/Mxx/Myy/Mxy + per-corner Mxx/Myy/Mxy) | `TArray<FFrameShellInternalForces>` |
| shellUtilization (per shell: DemandResult per corner+face) | `TArray<FFrameShellUtilization>` |
| singular (bool) | `bool bSingular` |
| diagnostic (std::string) | `FString Diagnostic` |
| pivotMargin (real) | `float PivotMargin` |
| utilization (DemandSummary top-level) | `FFrameDemandSummary` (maxDC, governingMember, governingShell, safetyFactor) |

Also surface `frame::DemandSummary` and `frame::ShellDemandSummary` independently
so `worstUtilization()` and `worstShellUtilization()` (D/C-screen-only paths
without a full solve) can be called from BP as well.

Marshal layer in `FrameCoreUETypes.cpp`:

```cpp
namespace FrameCoreUE {
    FRAMECOREUE_API FFrameSolveResult ToBlueprint(const frame::SolveResult& R);
    FRAMECOREUE_API FFrameDemandSummary ToBlueprint(const frame::DemandSummary& D);
    // (the existing `FFrameStressField ToBlueprint(frame::StressField)` stays)
}
```

Tests (`FrameCoreUE.Marshal.SolveResult.*`):

1. **CantileverDisplacement**: cantilever F68 fixture → marshal → assert
   `Displacement[1].Uz` matches analytic `P*L^3/(3*E*Iz)` rel < 1e-5 (float).
2. **CantileverReaction**: assert `Reaction[0].Fz == -P` rel < 1e-5.
3. **CantileverMemberForce**: assert `MemberForces[0].EndI.N == 0`, `EndI.Mz ==
   P*L` (within sign convention, abs rel < 1e-5).
4. **PivotMarginFinite**: assert `PivotMargin > 0 && IsFinite(PivotMargin)`.
5. **DemandSummary**: assert `Utilization.MaxDC > 0` for the cantilever.

Estimated Phase 2 cost: 5 hr.

### Phase 3 — Linear Analysis Library

`UFrameAnalysisLibrary` (UBlueprintFunctionLibrary) with the following
BlueprintCallable entries. Each takes a `FFrameModelDef` (or `UFrameModel*` if
the model becomes a UObject) plus an options struct, returns the matching
result USTRUCT.

| BP function | Engine call | Result USTRUCT |
|---|---|---|
| `SolveLinear` | `frame::solve(m, opt)` | `FFrameSolveResult` |
| `LoadCombineEnvelope` | `frame::loadCombineEnvelope(m, combos)` | `FFrameLoadEnvelope` (per-member peak demands across all combos) |
| `InfluenceLine` | engine influence line API (TBD: locate the function — search `Plugins/FrameSolver/Source/FrameCore/Public/` for `influence*` at Phase 3 start) | `FFrameInfluenceLine` |
| `SolveWithPrescribed` | `frame::solve` with `Node::prescribed` non-zero | reuses `FFrameSolveResult` |
| `AnalysisModal` | `frame::analysisModal(m, k)` where k = number of modes | `FFrameModalResult` (k modes: {frequency, period, shape: TArray<FFrameNodalDisplacement>}) |
| `AnalysisBuckling` | `frame::analysisBuckling(m, opt)` | `FFrameBucklingResult` (criticalFactor, knockdownFactor, reportedCriticalFactor, mode shape, diagnostic, singular) |
| `ResponseSpectrum` | `frame::responseSpectrum(m, spectrum, combination)` | `FFrameResponseSpectrumResult` |
| `RealTimeDynamic` | `frame::realTimeDynamic(m, history)` | `FFrameRealTimeDynamicResult` |
| `ReanalysisSolve` | `frame::reanalysisSolve(session, patch)` (S1 ReSolve, stateless library form — full S1 session lifetime is v3.5 InteractiveSubsystem work) | `FFrameSolveResult` |

Tests (`FrameCoreUE.Analysis.*`):

1. **SolveLinear.Cantilever**: matches Phase-2 `CantileverDisplacement` (regression).
2. **AnalysisModal.Cantilever**: first mode frequency matches analytic
   `omega_1 = (beta_1)^2 * sqrt(E*Iz/(rho*A*L^4))` where `beta_1 = 1.875` (rel < 1e-3, courser tolerance because the engine modal solver has its own residual budget).
3. **AnalysisBuckling.Column**: Euler column `P_cr = pi^2 * E * Iy / L^2`,
   `criticalFactor * P_applied == P_cr` rel < 1e-3.
4. **LoadCombineEnvelope.SSBeam**: two load combos (1.0DL + 1.0LL vs 1.2DL +
   1.6LL), assert envelope picks per-member max demand bit-exact.
5. **ReanalysisSolve.Cantilever**: solve linear → apply small section A patch →
   reanalysis_solve → assert result matches a fresh solve with the patched
   model rel < 1e-9.

Estimated Phase 3 cost: 10 hr.

### Phase 4 — Nonlinear Analysis Library

| BP function | Engine call | Result USTRUCT |
|---|---|---|
| `SolvePDelta` | `frame::solvePDelta(m, opt)` | `FFramePDeltaResult` (converged, iterations, lastIncrement, finalState, diverged) |
| `SolveTensionOnly` | `frame::solveTensionOnly(m, opt)` | `FFrameTensionOnlyResult` (converged, cycled, finalState, iterations, slack vector) |
| `SolveSizeOpt` | `frame::solveSizeOpt(m, opt)` | `FFrameSizeOptResult` (areas array, converged, cycled, finalAreas, finalDC, invalidDemand, iterations, singular, sizeOptSingular, weightVolume) |
| `SolveBESO` | `frame::solveBESO(m, opt)` | `FFrameBESOResult` (density field, iteration history, converged, mechanism flag, compliance) |
| `SolveCorotational` | `frame::runCorotational(m, opt)` | `FFrameCorotationalResult` (converged, diverged, finalState, lastResidual, loadStepsCompleted, totalIterations) |
| `SolveArcLength` | `frame::runArcLength(m, opt)` | `FFrameArcLengthResult` (converged, diverged, finalState, pathDisp, pathLambda, totalIterations) |
| `SolveDynCollapse` | `frame::runDynamicCollapse(m, opt)` | `FFrameDynCollapseResult` (frames, events, outcome, fragments, endTime, nFrames, nEvents) |

DynCollapse-specific marshal sub-types:

| Engine | USTRUCT |
|---|---|
| `frame::DynCollapseFrame` (t, u, v, kineticEnergy) | `FFrameDynCollapseFrame` (Time, Displacement: TArray<FFrameNodalDisplacement>, Velocity, KineticEnergy) |
| `frame::CollapseEvent` (time, elementId, mode, kind) | `FFrameDynCollapseEvent` (Time, ElementId, Mode (enum), Kind) |
| `frame::CollapseOutcome` (enum) | `EFrameDynCollapseOutcome` (Stable, Collapsed, Invalid, Cancelled) |
| `frame::FragmentCluster` (memberIds, shellIds, centroid, mass) | `FFrameFragmentCluster` (MemberIds, ShellIds, Centroid (FVector), Mass) |

NOTE: Live streaming of `DynCollapseFrame` mid-run (the `onFrameEmitted` + `isCancelled` callback channel from v2.7) is **deferred to v3.5 Phase 4** where it belongs to the replay actor + interactive subsystem. v3.4's `SolveDynCollapse` is the blocking, post-run-history form.

Tests (`FrameCoreUE.Nonlinear.*`):

1. **SolvePDelta.Column**: vertical column with axial+lateral load, assert
   `lastIncrement` < tolerance, `iterations` < 10, deflection > linear deflection
   (P-Delta amplification).
2. **SolveTensionOnly.Truss**: 6-bar truss with one compression-only bar, assert
   `slack[that_bar] == 1` and remaining members carry the redistributed load.
3. **SolveSizeOpt.Cantilever**: 1-bar cantilever, assert `finalDC` <= 1.0 and
   `finalAreas[0]` > initial area for an under-sized starting guess.
4. **SolveBESO.Cantilever**: 8-element ground structure, assert >= 1 element
   gets density 1.0 and `mechanism` flag false.
5. **SolveCorotational.Cantilever**: large-deflection cantilever, assert
   `loadStepsCompleted == requested`, `converged == true`.
6. **SolveArcLength.SnapThrough**: classic snap-through arch fixture (S9c F-fixture
   in standalone), assert `pathLambda` traces the snap-through curve.
7. **SolveDynCollapse.Cantilever**: cantilever with tip-load release at t=0,
   assert `nFrames > 0`, first frame `t == 0`, last frame `t == endTime`,
   `outcome == Stable` (no failure events).

Estimated Phase 4 cost: 10 hr (DynCollapse marshal is the biggest single item).

### Phase 5 — Shell + curve-surface opt-ins

Expose the opt-in flags through `FFrameSolveOptions` (Phase 1 already adds the
struct; Phase 5 wires them through `SolveLinear` + adds parity tests):

- `bUseDKQPlate`: switches `MITC4 → DKQ` plate element.
- `bUseQM6Membrane`: adds QM6 incompatible-modes bubble to MITC4 membrane.
- `bShellGeometricStiffness`: turns on shell `K_sigma` for buckling.
- `WarpTolerance` (float, default = 0.05): relaxes `FrameModel::validate` for
  warped quads.
- `bUseWarpingCorrection`: enables MacNeal/Sabir warped-quad correction.
- `bShellCorotational` (under `FFrameCorotationalOptions`): unlocks shell
  co-rotational large-deflection.

Tests (`FrameCoreUE.Shell.*` — most fixtures already exist on the standalone
side; the UE tests are thin mirrors):

1. **DKQ vs MITC4 parity**: simple plate fixture under uniform load, assert
   midspan deflection matches MITC4 within bend-only tolerance.
2. **QM6 membrane test**: in-plane shear plate, QM6 must reduce membrane locking
   relative to MITC4.
3. **Shell K_sigma**: SS plate buckling fixture, `criticalFactor` matches
   analytic `N_cr = 4*pi^2*D/a^2` (F57 standalone equivalent).
4. **Warped quad admit**: a warped facet that pre-v3.0 rejected now admits with
   default `WarpTolerance = 0.05`, solve runs without error.
5. **Shell co-rotational rotation invariance**: rotated cantilever shell, assert
   rotation-invariant tip deflection (F58b standalone mirror).
6. **NM interaction opt-in**: `CollapseOptions::nmInteraction = true` for a
   cantilever with combined N+M, assert plastic hinge forms earlier than the
   `Mp_full` case.

Estimated Phase 5 cost: 3 hr.

### Phase 6 — Version bumps + audit + release

- `FrameSolver.uplugin` Version 31 → 32, VersionName "3.3.0" → "3.4.0".
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` `kEngineVer` "3.3.0" → "3.4.0".
- `Scripts/run_gate.ps1` `$ExpectedUeTests` 72 → ~95 (Phase 1 adds 3, Phase 2
  adds 5, Phase 3 adds 5, Phase 4 adds 7, Phase 5 adds 6 → +26 = 98; fine-tune
  on actual test count).
- `Scripts/run_gpu_gate.ps1` + `.github/workflows/release-gate.yml`
  `FRAMECORE_EXPECTED_ENGINE_VER` '3.3.0' → '3.4.0'.
- `README.md` v3.4 status block; bump UE count; mention Karamba3D-parity
  surface achievement.
- `docs/VERIFICATION.md` UE count 72 → 98 (or actual); mention new analysis
  test families.
- `docs/ARCHITECTURE.md` UE count.
- `docs/HANDOFF_v3.4.0.md` + `docs/RELEASE_v3.4.0.md` (new).
- `docs/specs/UE5_engine_surface_map.md` (this file): mark as "landed v3.4.0"
  at top.
- `E:/project/CLAUDE.md` line 13 anchor v3.3.0 → v3.4.0.
- 3-agent audit pass (correctness + build/gate/version + docs/migration).
- Closeout findings.
- Final 5-leg gate green.
- Commit with explicit `git add` per Phase 5 commit pattern.
- `git tag v3.4.0` + push + `gh release create` with bundle zip (same composition
  as v3.3.0 bundle).

Estimated Phase 6 cost: 5 hr (audit lane is the biggest sub-item).

## Risks

- **Engine API discoverability**: some analyses' entry points (e.g.,
  `influence line`, `responseSpectrum`) might live in less-documented
  `Plugins/FrameSolver/Source/FrameCore/Public/` corners. The first Phase-3
  task is a grep sweep to confirm every BP function in the table has a real
  `FRAMECORE_API` entry; if any is missing, add the `FRAMECORE_API` annotation
  (still a "trivial engine delta", counted against the 0-line target).
- **UStruct nesting depth**: `FFrameSolveResult` nests several `TArray<F...>`
  layers. UE's BP `Make/Break Struct` ergonomics decline past ~3 nesting
  levels; mitigate by exposing `UFrameAnalysisLibrary` flat accessors
  (`GetDisplacementForNode`, `GetMemberForceForId`, etc.) alongside the raw
  struct.
- **DynCollapse memory**: a 1000-frame run with 50K-DOF model produces 50K * 6
  * 1000 floats = 300 MB per `FFrameDynCollapseFrame.Displacement` array. v3.4
  ships the full blocking-mode result; v3.5 streams. Document the memory
  footprint in the v3.4 RELEASE notes' "Honest boundaries" section.
- **Test count guard**: every Phase adds tests. The `$ExpectedUeTests` rule
  (CLAUDE.md 踩雷 #2) means each test addition must be paired with a
  `run_gate.ps1` bump in the same commit (or Phase 6 sweeps them all). Prefer
  Phase 6 sweep to keep per-Phase commits clean.

## Done definition (v3.4 ships)

1. Every BP function in the Phase-3 + Phase-4 tables exists and is BP-callable.
2. Every USTRUCT in the Phase-1 + Phase-2 + Phase-4 tables exists and is
   BlueprintReadWrite (or BlueprintReadOnly where appropriate).
3. UE test count moves 72 → ~98, all pass.
4. 5-leg gate green; v2_roundtrip CPU green with `kEngineVer 3.4.0` pin.
5. Engine source delta ≤ 10 lines (additive `FRAMECORE_API` only); CUDA path
   delta = 0.
6. RELEASE notes' "Honest boundaries" section lists the engine limitations
   that v3.4 does not lift (D/C is elastic; dyn collapse is LSP-level; modal/
   buckling are linear; etc.).
7. 3-agent audit clean.

## Honest boundaries (carried forward from engine; v3.4 surfaces but does NOT change)

- D/C utilization is **elastic only**; not an RC ultimate-state check.
- Dynamic collapse is **LSP-level sequential linear** (each event is linear;
  literature cites ±30% conservatism).
- Modal / buckling / response spectrum are all **linear**.
- Plastic hinges are **event-to-event** (no unloading / reversal).
- No fiber section / pushover (deliberately excluded by user).
- Shell elements are **flat-facet MITC4** (curvature error O(1/N^2)).
- Co-rotational shells are **small-strain** (linear `kl_` black box).

These are documented at each `UFUNCTION` doc-comment so a BP designer sees
them in the editor tooltip, not just in the spec.
