# RELEASE v3.5.1 — deferred-items closeout + first verified 5-leg gate

> Tag: `v3.5.1` · Anchor: post v3.5.0 (visual + game-ready surface)
> Bundle: same composition as v3.5.0.

## TL;DR

v3.5.1 closes the v3.5.0 deferred queue **and** is the first v3.5 series release where
the **full 5-leg CPU gate was actually run end-to-end** on the integrator host. v3.5.0
shipped with Legs 2-6 "Projected" pending Z-01; v3.5.1 lifts the disclaimer.

Engine source delta vs v3.5.0 = **0 lines under `Plugins/FrameSolver/Source/FrameCore/`**.
v3.5.1 is pure UE-side refactor + small fixes.

## What changed vs v3.5.0

### PMC-DUP-01 — shared PMC helpers (net −130 LOC)

`Plugins/FrameSolver/Source/FrameCoreUE/Private/FramePMCHelpers.h` consolidates the
`MemberLocalAxes` + `CornerOffset` + `kRings = 11` helpers that v3.5.0 duplicated across
6 PMC-box renderer TUs. Behaviour bit-identical (helper bodies are the v3.5.0
byte-identical copies); future tweaks to the near-vertical threshold or ring count
propagate to every renderer instead of risking silent divergence.

Touched: 6 actor `.cpp` files (DeformedShape / Heatmap / ModalShape / DynCollapseReplay /
ResponseSpectrum / RealTimeDynamic) + 1 new header.

### TEST-DUP-01 — shared test helpers

`Private/Tests/FrameCoreUETestHelpers.h` gains `GetSpawnWorld()` and `TipCenter()`
inline helpers used by the v3.5 renderer-actor tests. 7 of the 8 new v3.5 test files
adopt them via `using FrameCoreUETestHelpers::...`; the InteractiveSubsystem test uses
its own `GetSubsystem()` pattern (game-instance subsystem; world-spawn helper
not applicable).

### U-13 — long-session float-precision modular phase reduction

`AFrameModalShapeActor::Tick` and `AFrameResponseSpectrumActor::Tick` now `FMath::Fmod`
`CurrentPhase` by `1 / FreqHz` (or `1 / EnvelopeHz`) once it exceeds 1e5 s. Removes the
silent freeze-after-~193-days float saturation cliff for digital-twin / kiosk scenarios
that never reload the level.

### U-14 — fragment debris cap

`AFrameFragmentClusterActor::MaxDebrisActors` UPROPERTY (default 1024) limits
`SpawnedDebris.Num()` growth across repeated `SpawnFragmentDebris` calls without
`ClearDebris`. Prevents unbounded GC-root accumulation.

### Build-time fixes surfaced by the first real UE build

The v3.5.0 audit ran read-only; v3.5.1's actual `Build.bat ArchSimEditor` exposed three
items the read-only audit missed:

- **UFunction TObjectPtr restriction**: `AFrameFragmentClusterActor::GetSpawnedDebris`
  returning `const TArray<TObjectPtr<AStaticMeshActor>>&` was rejected by UHT. Split
  into a BP-facing `GetSpawnedDebrisArray()` that materialises a raw-pointer copy +
  a C++-only `GetSpawnedDebris()` for internal callers.
- **PhysicsEngine/BodySetup.h drags Chaos into the TU**: removing the unused include
  in `FrameFragmentClusterActor.cpp` fixed cascading `Chaos/Box.h` / `Chaos/Convex.h`
  incomplete-type errors.
- **GENERATED_BODY conflicts with explicit `= delete` copy ctor** (Audit D-05 over-engineering):
  reverted the explicit deletion; UObject convention already covers this.
- **TUniquePtr\<incomplete\>** in UHT auto-generated `FVTableHelper` ctor: changed
  `Cached` from `TUniquePtr<FCachedModel>` to raw `FCachedModel*` with manual `delete`,
  matching the existing `Session` raw-pointer pattern.

## 5-leg gate (CPU) — VERIFIED

Run on the integrator host this release session (Windows 11, MSVC vs2026 preview, UE 5.7,
`framecore-direct` conda env):

| Leg | Command | Result |
|---|---|---|
| 1 Standalone | `build.bat` | F1..F71 **ALL PASS** (failures=0) |
| 2 UE automation | `Build.bat ArchSimEditor` (Succeeded) + `run_gate.ps1` | **120 / 120 PASS** (exit code 0; FrameCore.UE.* family all green incl. 22 v3.5 visual-surface + 3 InteractiveSubsystem tests via `NewObject` fallback when headless commandlet has no GameInstance) |
| 3 OpenSees | `Tools/opensees_compare.py --relaxed` | **PASS** (shallow-arch von Mises snap-through ours-vs-OS 6.42e-3) |
| 4 Deep audit | `linear_deep_audit.exe` | **PASS failures=0 checks=104** |
| 5 CLI round-trip | `cli_roundtrip.py` | **ALL PASS** (failures=0; EIGEN zero-mass / daemon / C-API / COROT planar+3D / ARCL shallow-arch) |
| 6 v2 round-trip | `build_capi_v2.bat` + `v2_roundtrip.py` | **PASS** (`kEngineVer="3.5.1"` pin enforced; 23 capabilities; wire ABI unchanged from v3.4.0/v3.5.0) |

## Wire / version contract

- `kEngineVer` **3.5.0 → 3.5.1** (`Plugins/FrameSolver/Standalone/v2/Dispatcher.h`).
- `FrameSolver.uplugin` Version **33 → 34**, VersionName **3.5.0 → 3.5.1**.
- `FRAMECORE_EXPECTED_ENGINE_VER` **3.5.0 → 3.5.1** (`run_gpu_gate.ps1` + `release-gate.yml`).
- `run_gate.ps1 $ExpectedUeTests` **unchanged at 120** (no new tests; helper refactors).
- **Breaking changes: none.** UE-side refactor + small fixes only.

## Deferred to v3.6 (carried forward from v3.5.0 deferred list)

- **U-08** (showcase map + BP examples): the Python builder in
  `Tools/build_v3_5_showcase_map.py` works; running it requires the UE Editor and a
  designer to assemble the BP examples on top.
- **U-09** (Chaos POD destruction): `UGeometryCollectionComponent` migration; v3.5
  ships StaticMesh debris thin slice.
- **U-10** (influence-line polarity audit), **U-11** (cubic Hermite member-axis
  interpolation), **U-12** (`FFrameModelPatch` incremental load patch), **U-15**
  (CI-calibrated PerfBaseline threshold).

See [`docs/HANDOFF_v3.5.1.md`](HANDOFF_v3.5.1.md) for next-cycle pickup.
