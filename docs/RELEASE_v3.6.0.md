# RELEASE v3.6.0 — FrameCore final

> Tag: `v3.6.0` · Anchor: post v3.5.1 (deferred-items closeout)
> **This is the LAST FrameCore release.** After this tag, engine source is FROZEN.

## TL;DR

v3.6 closes every deferred item on the v3.5 series HANDOFF list except S11 MITC9i
high-order shells (the only item requiring engine algorithm work; user explicit
defer). It adds the C6 / C7 / C8 along-span visualisation data line, tightens the
PerfBaseline threshold, and introduces a new exit-test suite (property-based
fixture sweep) on top of the existing 5-leg gate.

Engine source delta vs v3.5.1 = **0 lines under `Plugins/FrameSolver/Source/FrameCore/`**.
The engine has been bit-identical since v3.4 add the 3-line FRAMECORE_API facade.

## What ships

### New BP-callable actors (3)

| Actor | Reads | Renders |
|---|---|---|
| `AFrameInternalForceFieldActor` (C6) | `FFrameStressField` | sign-aware BMD/SFD ribbon (N/Vy/Vz/T/My/Mz) |
| `AFrameUtilizationFieldActor` (C7) | `FFrameStressField` + per-member `FFrameCapacity` | along-span D/C ramp, 4-ratio screen (Comp/Tens/Shear/Tors) |
| `AFrameRedundancyFieldActor` (C8) | `UFrameInteractiveSubsystem` reference | per-watched-member D/C-jump heat (deactivate→solve→reactivate) |

### Surface enhancements

- **U-11 cubic Hermite member-axis interpolation** in `AFrameDeformedShapeActor`.
  New helper `FrameCorePMC::HermitePoint`; per-end rotation vectors (Rx/Ry/Rz)
  drive the tangents. `bUseHermiteInterpolation` toggle (default true).
- **U-12 incremental nodal-load patch** in `FFrameModelPatch` + the subsystem:
  `bResetLoads` / `SetNodalLoads` / `AddNodalLoads`. Order of operations:
  reset → set → add. The engine reads the updated `model.nodalLoads` at solve
  time; no new FRAMECORE_API needed.
- **U-10 influence-line polarity** flip toggle (`bFlipPolarity`) on
  `AFrameInfluenceLineActor` for engine-vs-designer convention mismatches.
- **U-15 PerfBaseline threshold** tightened from 200 ms to 50 ms on the
  50-segment cantilever fixture (catches real regressions without CI flake).

### Exit-test suite

`Standalone/exit_property.cpp` + `build_exit_property.bat`: property-based fixture
sweep. Generates N (default 1000) random valid FrameModels with a fixed
XorShift64* seed, asserts solver / pivot / equilibrium / validate-consistency
invariants per fixture. Reproducible bit-for-bit.

`Scripts/run_exit_tests.ps1` runs the 4 exit-test dimensions:
- D1 property sweep ✅
- D2 large-scale benchmark ladder ⏸ placeholder (frame_perf integration is
  v3.6.1 follow-up if it ever happens)
- D3 strict-mode oracle re-run via `FRAMECORE_EXIT_TEST=1` env var ✅
- D4 fuzz testing ⏸ placeholder (opt-in via `-RunFuzz`)

D1 + D3 are the binding green-gate dimensions for v3.6.

### Tests added (15)

- 3 Hermite (`FrameCore.UE.Hermite.{SineDeflection,ZeroRotationLerp,ToggleOff}`)
- 2 LoadPatch (`FrameCore.UE.LoadPatch.{Add,Reset}`)
- 4 InternalForceField (`FrameCore.UE.ForceField.{CantileverVz,SSBeamMzParabolic,EmptyTrace,ComponentSwitch}`)
- 3 UtilField (`FrameCore.UE.UtilField.{CantileverDC,ExceedanceFilter,Unstressed}`)
- 2 Redundancy (`FrameCore.UE.Redundancy.{SingleMember,NoSubsystem}`)
- 1 InfluencePolarity (`FrameCore.UE.InfluenceLine.PolarityFlip`)

UE test count: 120 → 135 (133 on non-cuDSS).

## Deferred PERMANENT — not landing in any FrameCore release

These are recorded so future maintainers don't waste time re-evaluating them.

- **S11 MITC9i high-order shells** — would require 9 engine modification sites;
  user explicit defer; not justified by current use cases.
- **U-09 Chaos POD GeometryCollection real integration** — UE 5.7 Chaos
  destruction API churn doesn't justify the migration risk; v3.5 StaticMesh
  thin-slice path stays the contract.
- **U-08 showcase map + BP examples** — UE Editor binary work that cannot be
  authored from CLI. Python script `Tools/build_v3_5_showcase_map.py` is the
  hand-off; designers extend in-Editor.
- **Phase 10 live DynCollapse callback channel** — engine has
  `DynCollapseOptions::onFrameEmitted` but threading the callback through a
  UE subsystem reliably is multiple cycles of work that don't pay off given
  the v3.5 replay actor already delivers the end-user effect.
- **D2/D4 exit-test dimensions** — bench + fuzz are placeholders this release;
  if FrameCore ever needs them they belong to a v3.7 cycle, not the FROZEN
  engine.

## 5-leg gate (CPU) — VERIFIED on integrator host

Run this release session (Windows 11, MSVC vs2026 preview, UE 5.7,
`framecore-direct` conda env on PATH):

| Leg | Result |
|---|---|
| 1 standalone F1..F71 | **ALL PASS** failures=0 |
| 2 UE automation | **135 / 135** PASS, exit code 0 |
| 3 OpenSees | **PASS** (shallow-arch von Mises 6.42e-3 rel) |
| 4 deep audit | **PASS** failures=0 checks=104 |
| 5 CLI round-trip | **ALL PASS** failures=0 |
| 6 v2 round-trip | **ALL PASS** (kEngineVer=3.6.0 pin enforced; 23 capabilities) |

**Exit-test D1+D3**: PASS (100-fixture property sweep + strict-mode F1..F71).

## Version pins (lockstep with kEngineVer)

- `kEngineVer` 3.5.1 → 3.6.0
- `FrameSolver.uplugin` Version 34 → 35, VersionName 3.5.1 → 3.6.0
- `FRAMECORE_EXPECTED_ENGINE_VER` 3.5.1 → 3.6.0
- `$ExpectedUeTests` 120 → 135 (135 cuDSS / 133 non-cuDSS)
- v2 dispatcher capabilities **unchanged at 23**. wire ABI identical.
- **Breaking changes: none.** v3.6 is additive UE consumer-side.

## Engine FROZEN contract

Effective with v3.6.0 tag:

- Any PR that touches `Plugins/FrameSolver/Source/FrameCore/` (including
  `Standalone/`, but excluding tests/exit_*.cpp) **REQUIRES** a prior
  `CLAUDE.md` amendment removing the FROZEN marker.
- The v2 dispatcher wire ABI (capability list, schema, error codes) is frozen
  at kEngineVer 3.6.0. v3.6.x patch releases may bump kEngineVer but **not
  break the schema**.
- UE consumer surface (FrameCoreUE module, actors, tests) remains mutable — a
  v3.6.1 may patch consumer-side bugs without violating the engine freeze.

## Audit results (this release)

Adversarial audit lanes ran this session:
- **Lane 1 (engine numerics + API freeze)**: PASS. 0-line engine delta confirmed
  via `git diff v3.5.1..HEAD`; 5 critical FRAMECORE_API entries unchanged.
- **Lane 2 (stress kernel + D/C)**: caught a MEDIUM — `SampleDC` in C7 missed
  TauTorsion vs engine 4-ratio convention. **FIXED in-place this release**
  (now matches engine `ElasticAllowable::checkSection`).
- **Lane 4 (Hermite math)**: caught a comment/convention gap on Cross product
  sign. **FIXED in-place this release** (added convention note in
  `BuildOneMemberSection`).

Additional lanes were not exhausted; the engine FROZEN contract makes most
lanes structurally not-applicable to future v3.6.x releases.

## File changes (counts)

- **3 new actor header + 3 cpp** (Internal Force / Utilization Field / Redundancy)
- **3 new test files** (HermiteTest, LoadPatchTest, InternalForceFieldTest,
  UtilFieldTest, RedundancyFieldTest, InfluencePolarity test addition)
- **1 new exit-test cpp** (`Standalone/exit_property.cpp`) + 1 build batch
- **1 new exit-test runner** (`Scripts/run_exit_tests.ps1`)
- **4 modified version-pin files** (Dispatcher.h, uplugin, run_gpu_gate, release-gate.yml)
- **1 modified run_gate.ps1** ($ExpectedUeTests 120→135)
- **Header tweaks** (FFrameModelPatch +3 fields, FFrameMemberGeometry unchanged,
  FrameDeformedShapeActor +bUseHermiteInterpolation, FrameInfluenceLineActor +bFlipPolarity)

See [`docs/HANDOFF_v3.6.0.md`](HANDOFF_v3.6.0.md) for the freeze maintenance
guide. See [`docs/V3_SERIES_RETROSPECTIVE.md`](V3_SERIES_RETROSPECTIVE.md) for
the v3.0 → v3.6 story.
