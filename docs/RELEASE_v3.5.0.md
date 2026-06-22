# RELEASE v3.5.0 — visual + game-ready surface

> Tag: `v3.5.0` · Anchor: post v3.4.0 (Karamba3D-parity numerical surface)
> Source: this commit. Bundle: `framecore-v3.5.0-win64.zip` (composed identically to
> v3.4.0; CUDA legs unchanged).

## TL;DR

v3.5.0 closes the engine-result-vs-UE-visual gap. After v3.5, every numerical result the
v3.4 engine surface produces has a matching UE5 actor that renders it visually, plus a
`UGameInstanceSubsystem` that wraps `frame::ReSolveSession` for interactive 60-fps re-analysis.

**Engine source delta vs v3.4.0 = 0 lines under `Plugins/FrameSolver/Source/FrameCore/`.**
v3.5 is pure UE consumer-side work — `Plugins/FrameSolver/Source/FrameCoreUE/` only.
CLAUDE.md 鐵則 #1 fully honoured.

## What ships

### 8 new BP-callable actors

| Actor | Consumer | Phase |
|---|---|---|
| `AFrameDeformedShapeActor` | `FFrameSolveResult.Displacements` + `MemberGeometry` | 1 |
| `AFrameUtilizationHeatmapActor` | `FFrameSolveResult.MemberUtilization` + `ShellUtilization` | 2 |
| `AFrameModalShapeActor` | `FFrameModalResult.Modes[ModeIndex].Shape` | 3 |
| `AFrameDynCollapseReplayActor` | `FFrameDynCollapseResult.Frames` | 4 |
| `AFrameFragmentClusterActor` | `FFrameDynCollapseResult.Events[*].Detached` | 5 |
| `AFrameInfluenceLineActor` | `FFrameInfluenceLine` | 6 |
| `AFrameResponseSpectrumActor` | `FFrameResponseSpectrumResult.PeakDisplacements` | 8 |
| `AFrameRealTimeDynamicActor` | `FFrameModalTimeHistory.Steps` | 8 |

### 1 new game-instance subsystem

`UFrameInteractiveSubsystem` (Phase 7) wraps a long-lived `frame::ReSolveSession`. BP API:

- `StartSession(Def, Opts, ReOpts, OutError)` — allocate session, factor baseline.
- `ApplyPatchAndResolve(FFrameModelPatch, OutResult)` — toggle members/shells, S1 ReSolve,
  marshal result.
- `Rebaseline()` — force Tier-3 rebaseline (clears ladder rank).
- `ResolveCurrent(OutResult)` — resolve without patching (for perf baselines).
- `EndSession()` — release.

### 2 new shared USTRUCT types

In `FrameCoreUE/FrameCoreUEVisualTypes.h`:

- `FFrameShellGeometry` — per-shell world-space corners + node-index mapping for the
  heatmap actor.
- `FFrameModelPatch` — Phase 7 subsystem patch payload (member/shell activate/deactivate
  lists).

Also: `FFrameMemberGeometry` gained two additive fields (`EndINodeIdx`, `EndJNodeIdx`,
default -1). This is forward-compatible — v3.3 `AFrameCoreStressFieldActor` doesn't read
them, so legacy callers stay bit-identical.

## 5-leg gate (CPU) — projected, pending Z-01 fresh-clone run

**Honest disclosure:** the 5-leg gate was **NOT RUN end-to-end** in the
release-hardening session that produced this tag. Engine source delta vs v3.4.0
is 0 lines under `Plugins/FrameSolver/Source/FrameCore/` (CLAUDE.md 鐵則 #1),
so Leg 1 is **structurally bit-identical** with v3.4.0 — its v3.4.0 ALL PASS
evidence carries forward unchanged. Legs 2-6 are **projected**: the new code
is UE-side consumer-only and the version-pin contract holds, so a fresh
UE Editor build + `Scripts\run_gate.ps1 -RequireOpenSees` on the integrator
host should yield the results below. Promotion from "expected" to "verified"
requires the Z-01 first-action documented in
[`HANDOFF_v3.5.0.md`](HANDOFF_v3.5.0.md#z-01-first-action--verify-gate-on-a-clean-checkout).

Reference environment: Windows 11, MSVC vs2026 preview, UE 5.7,
`framecore-direct` conda env on PATH.

| Leg | Command | Status | Expected result |
|---|---|---|---|
| 1 Standalone | `build.bat` | **CARRIES FORWARD** (engine delta 0) | F1..F71 ALL PASS (bit-identical with v3.4.0) |
| 2 UE automation | `Build.bat ArchSimEditor` + `run_gate.ps1` automation | **PROJECTED — needs Z-01 run** | **120 / 120** with cuDSS (**118 / 118** without; `-ExpectedUeTests 118`) |
| 3 OpenSees | `Tools/opensees_compare.py --strict` | **PROJECTED** | PASS (engine numerics unchanged) |
| 4 Deep audit | `linear_deep_audit.exe` | **PROJECTED** | **104 / 104** PASS |
| 5 CLI round-trip | `cli_roundtrip.py` | **PROJECTED** | **13 / 13** PASS |
| 6 v2 round-trip | `build_capi_v2.bat` + `v2_roundtrip.py` | **PROJECTED** | ALL PASS (kEngineVer=3.5.0 pin enforced; 23 capabilities; wire ABI unchanged from v3.4.0) |

## 3 CUDA legs (carried forward)

v3.5 engine source delta in the CUDA path = 0 lines. The v3.0.0 / v3.1.0 GPU evidence
remains the authoritative baseline:

- `r2_bench --gpu 90k` margin +11.939 ms (vs 16.67 ms / 60 fps budget).
- `F67s STRICT_EXECUTED` fingerprint in UE log under `FRAMECORE_GPU_STRICT=1`.

Run `Scripts\run_gpu_gate.ps1 -Strict` on a cuDSS host to refresh.

## Wire / version contract

- `kEngineVer` **3.4.0 → 3.5.0** (`Plugins/FrameSolver/Standalone/v2/Dispatcher.h`).
- `FrameSolver.uplugin` Version **32 → 33**, VersionName **3.4.0 → 3.5.0**.
- `FRAMECORE_EXPECTED_ENGINE_VER` **3.4.0 → 3.5.0** in `Scripts/run_gpu_gate.ps1` +
  `.github/workflows/release-gate.yml`.
- `run_gate.ps1 $ExpectedUeTests` **98 → 120**.
- v2 dispatcher capability list **unchanged** (23 capabilities). v3.5 is UE-side, not
  dispatcher-side; client wire ABI is identical to v3.4.0.
- **Breaking changes: none.** v3.5 is purely additive UE consumer surface; v3.4 clients
  need only bump `$ExpectedUeTests` from 98 to 120 if they run the gate locally.
  Existing `AFrameCoreStressFieldActor` (v3.3) is unaffected by the additive
  `FFrameMemberGeometry.EndINodeIdx/EndJNodeIdx` field pair.

## Honest boundaries (v3.5 inherits and adds)

Carried from v3.4 (engine):
- D/C utilization is **elastic only**; not an RC ultimate-state check.
- Dynamic collapse is **LSP-level sequential linear** (±30% conservatism, per literature).
- Modal / buckling / response spectrum are all **linear**.
- Plastic hinges are **event-to-event** (no unloading / reversal).
- No fiber section / pushover.
- Shell elements are **flat-facet MITC4** (curvature error O(1/N²)).

v3.5-specific:
- **Chaos POD destruction is a v3.6 deliverable.** Phase 5 (`AFrameFragmentClusterActor`)
  ships a **thin slice**: each fragment cluster spawns an `AStaticMeshActor` with physics
  enabled (mass = `Cluster.Mass`, initial linear velocity = `Cluster.Vel`). Full Chaos
  `UGeometryCollectionComponent` integration is deferred to v3.6 because UE 5.7's Chaos
  destruction API has rough edges and the StaticMesh path delivers the same end-user
  effect ("a chunk falls when a member detaches") without the plugin-dep drift.
- **DeflectionScale is a visual amplification** (default 100x). Real displacements are
  typically sub-millimetre; without amplification the deformed shape looks identical to
  undeformed.
- **Real-time interactive 60 fps depends on DOF and patch size.** The 10K-DOF / 16.7 ms
  baseline is the engine R2 lane benchmark; the UE wrapper test (Phase 7
  `PerfBaseline`) only asserts the wrapper itself doesn't impose pathological marshalling
  overhead.
- **Replay actor interpolation is linear** between Newmark frames (not cubic Hermite).
  Adequate at default dt < 1/30 s; high-frequency artifacts possible at coarser dt.
- **Modal shape animation timing is artificial.** `cos(2π * f * t)` uses the real engine
  frequency, but Amplitude is BP-tuned and unrelated to actual modal energy.
- **Phase 9 (showcase map + BP examples) is deferred to v3.5.1.** A Python builder script
  is provided in `Tools/build_v3_5_showcase_map.py` — run it from inside the UE Editor's
  Python console to populate a starter `Content/Maps/FrameCoreShowcase.umap` with one of
  every renderer actor.

## File changes (counts)

| Area | Files added | Files modified | Engine source delta |
|---|---|---|---|
| FrameCoreUE Public/ | 10 new headers (8 renderer-actor headers `Frame{DeformedShape,UtilizationHeatmap,ModalShape,DynCollapseReplay,FragmentCluster,InfluenceLine,ResponseSpectrum,RealTimeDynamic}Actor.h` + `FrameInteractiveSubsystem.h` (subsystem) + `FrameCoreUEVisualTypes.h` (shared types)) + 1 modified (`FrameCoreUETypes.h` +2 fields on `FFrameMemberGeometry`) | — | 0 lines under `FrameCore/` |
| FrameCoreUE Private/ | 9 new `.cpp` (one per actor + subsystem) | — | — |
| FrameCoreUE Tests/ | 7 new test files (22 individual tests) | — | — |
| Tools/ | `build_v3_5_showcase_map.py` (Python builder for Phase 9) | — | — |
| Scripts/ | — | `run_gate.ps1` ($ExpectedUeTests), `run_gpu_gate.ps1` (version pin) | — |
| Plugin manifest | — | `FrameSolver.uplugin` (Version 32→33, VersionName 3.4.0→3.5.0) | — |
| v2 dispatcher | — | `Dispatcher.h` (kEngineVer 3.4.0→3.5.0) | — |
| CI | — | `.github/workflows/release-gate.yml` (version pin) | — |
| Docs | `RELEASE_v3.5.0.md`, `HANDOFF_v3.5.0.md` | `README.md`, `docs/VERIFICATION.md`, `docs/ARCHITECTURE.md` | — |

## Release-hardening audit (7-agent adversarial sweep)

Seven parallel read-only audits (A: numerics + API · B: oracle / claim cross-check ·
C: actor PMC geometry · D: lifetime + memory · E: docs + comment cartography · F: code
quality + optimization · G: reproducibility + privacy) ran against the v3.5.0 candidate.
Findings closed before this tag:

- **A-02 HIGH**: `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam` const-ref struct param →
  by-value `FFrameDynCollapseEvent` (UHT marshal contract).
- **A-04 MED**: `FFrameShellGeometry.CornerNodeIndices` documented as v3.6-reserved.
- **A-05 HIGH + D-02 + D-10**: `AFrameDynCollapseReplayActor` end-of-loop events were
  silently dropped when `bLoop` wrapped — `DispatchEventsIn` now fires the pre-wrap
  window before fmod, then the post-wrap window; dead `PrevTime` member removed.
- **C-05 BLOCKER**: `AFrameInfluenceLineActor` ribbon normal now flips with `sign(Height)`
  so negative-influence segments shade correctly under directional light.
- **C-09 HIGH**: `AFrameResponseSpectrumActor` envelope changed from `cos(...)` to
  `0.5 * (1 + cos(...))` so SRSS / CQC peak displacements (direction-less scalars)
  never animate through reverse peaks — physically meaningless oscillation removed.
- **C-03 MED**: `ensureMsgf` on `FFrameDynCollapseFrame.UFlat.Num() % 6 == 0` surfaces
  truncated frame payloads.
- **D-01 HIGH + F-06 MED**: `UFrameInteractiveSubsystem::StartSession` wraps the
  engine `ReSolveSession` ctor in try/catch so an allocation failure does not leave
  `Cached` non-null with `Session` null; explicit `= delete` on copy/move ctors.
- **D-04 MED + D-06 MED**: `AFrameModalShapeActor::TimeScale` now `ClampMin = 0.0`;
  `AFrameDynCollapseReplayActor::PlaybackSpeed` runtime-clamped at the Tick use-site
  so BP bypass of the UI ClampMin can't drive `CurrentTime` negative.
- **D-12 LOW**: `UFrameInteractiveSubsystem` test now `AddError` on subsystem-null path
  so a missing GameInstance is a failure, not a silent pass.
- **F-04 + F-05 MED + F-12 LOW**: `AFrameUtilizationHeatmapActor::BuildHeatmap`
  precomputes `TMap<MemberIdx, Risk>` / `TMap<ShellIdx, Risk>` once instead of doing
  O(M·N) linear scans per member — matters for `UFrameInteractiveSubsystem`'s
  60 fps re-heatmap path; `SatGuard` hoisted out of the per-section loop.
- **F-09 LOW**: `AFrameDeformedShapeActor` `BaseColor` hoisted out of the ring loop
  (per-actor consistency with the other PMC actors).
- **E-01 + E-06 BLOCKER**: release-notes gate table no longer claims "Verified on
  integrator host" for legs that were not actually run; status column distinguishes
  "Carries forward" (Leg 1, engine source delta 0) from "Projected" (Legs 2-6) with
  the Z-01 fresh-clone-run instruction in HANDOFF.
- **E-02 + E-03 + E-04 + G-04 BLOCKER**: stale `v3.3.0` / 98 / 96 / 72 references
  in README.md and ARCHITECTURE.md replaced with v3.5.0 numbers.
- **E-05 + G-05 HIGH**: `docs/specs/UE5_engine_surface_map.md` status header bumped
  to "landed v3.4.0".
- **E-07 HIGH**: `CLAUDE.md` v3.4.0 paragraph's "v3.5 留 ..." references annotated
  "**landed v3.5**".
- **E-08 + E-10 + E-11 + E-12 + E-15 MED**: VERIFICATION projected-status footnote,
  RELEASE header-count corrected (9→10), Phase 5 + Phase 7 spec annotations linking
  to thin-slice + CI-friendly threshold context, HANDOFF link anchor added.
- **E-13 + G-01 + G-02 HIGH**: hardcoded `E:/project/ArchSim` paths in
  `Tools/build_v3_5_showcase_map.py`, `HANDOFF_v3.5.0.md` Z-01 PowerShell block, and
  HANDOFF U-08 invocation replaced with `<path-to-ArchSim-clone>` placeholder + env-var
  pattern matching prior HANDOFF (v3.1.0) style.
- **G-03 MED**: `build/` (release-bundle staging area) added to `.gitignore`.
- **B-02 MED + B-03 LOW**: VERIFICATION.md stray "not 116" → "not 96"; `run_gate.ps1`
  $ExpectedUeTests comment now flags Phase 9 deferred / Phase 10 no-new-tests.

Deferred to v3.5.1 / v3.6 (cross-confirmed in HANDOFF):

- **PMC-DUP-01 (F-01/F-02/F-03)**: extract shared `MemberLocalAxes` + corner helper +
  `kFramePMCNRings` constexpr into `Private/FramePMCHelpers.h` (net −130 LOC).
  Pure refactor, held out of v3.5 release scope per release-hardening discipline.
- **TEST-DUP-01 (F-07/F-08)**: shared `Private/Tests/FrameCoreUEVisualTestHelpers.h`
  for `GetSpawnWorld()` / `TipCenter()` (net −40 LOC).
- **U-08, U-09, U-10, U-11**: full HANDOFF U-## list.
- **U-12 (E-14)**: `FFrameModelPatch` incremental-nodal-load patch field
  (v3.6 schema extension).
- **U-13 (D-03)**: long-session float-precision modular phase reduction in
  `AFrameModalShapeActor` / `AFrameResponseSpectrumActor` (`FMath::Fmod` periodic
  reduction); only matters at multi-day continuous play.
- **U-14 (D-07)**: `AFrameFragmentClusterActor::SpawnedDebris` cap to avoid unbounded
  growth on repeated `SpawnFragmentDebris` without `ClearDebris`.
- **U-15 (F-14)**: tighten `PerfBaseline` test threshold from 200 ms to CI-calibrated
  10–20 ms once CI hardware data is available.

## Next-cycle pickup (see [`HANDOFF_v3.5.0.md`](HANDOFF_v3.5.0.md))

- **U-08** v3.5.1: BP examples + showcase map authoring (1 hr human-in-Editor work).
- **U-09** v3.6: Full Chaos POD destruction (`UGeometryCollectionComponent`).
- **U-10** v3.6: `FFrameInfluenceLine.ReactionAtPosition` magnitude polarity audit
  (engine returns reactionAt unit positive Z load; visual ramp signs may need flip).
- **U-11** v3.6: Cubic Hermite member-axis interpolation in `AFrameDeformedShapeActor`
  (requires marshal of per-end rotation vectors).
