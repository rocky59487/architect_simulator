# HANDOFF v3.5.1 → next cycle

> v3.5.1 is the first v3.5 series tag whose **5-leg gate was actually verified
> end-to-end** on the integrator host. v3.5.0's Projected disclosure no longer applies
> to the engine numerics or the v3.5 visual surface.

## What was done

- Closed v3.5.0 deferred items **PMC-DUP-01** (shared `FramePMCHelpers.h`,
  net −130 LOC), **TEST-DUP-01** (shared `FrameCoreUETestHelpers.h` for
  `GetSpawnWorld` + `TipCenter`), **U-13** (modular phase reduction in
  ModalShape + ResponseSpectrum Tick), **U-14** (FragmentCluster
  `MaxDebrisActors` cap).
- Surfaced + fixed 4 UE-build-time issues the v3.5.0 read-only audit could not see
  (UFunction TObjectPtr restriction; PhysicsEngine include dragging Chaos in;
  GENERATED_BODY collision with explicit `= delete`; TUniquePtr\<incomplete\>
  vs UHT vtable helper).
- Re-ran the 5-leg gate; results in [`RELEASE_v3.5.1.md`](RELEASE_v3.5.1.md).

## Still deferred (v3.6 cycle)

- **U-08**: showcase map + BP examples (UE Editor manual work; Python builder ready).
- **U-09**: Chaos POD destruction (`UGeometryCollectionComponent` migration).
- **U-10**: influence line polarity convention audit.
- **U-11**: cubic Hermite member-axis interpolation (needs marshal of per-end
  rotation vectors).
- **U-12**: `FFrameModelPatch` incremental nodal-load patch field (E-14 from v3.5.0
  audit).
- **U-15**: tighten PerfBaseline threshold 200 ms → CI-calibrated 10-20 ms once
  CI hardware data is available.

## Z-01 first action (if v3.5.1 lands anything that needs human inspection)

```powershell
$Root = "<your-ArchSim-clone>"
Set-Location $Root
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```

Expected: same green pattern as v3.5.1 RELEASE table. If anything regresses, the
suspect set is the PMC helper extraction (touched 6 actor TUs).

## Critical version pins (post-v3.5.1)

If a future release breaks Leg 6 with a version-pin failure, sync all four pins
in one commit:
1. `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` — `kEngineVer`
2. `Plugins/FrameSolver/FrameSolver.uplugin` — `Version` + `VersionName`
3. `Scripts/run_gpu_gate.ps1` — `FRAMECORE_EXPECTED_ENGINE_VER`
4. `.github/workflows/release-gate.yml` — `FRAMECORE_EXPECTED_ENGINE_VER`

`$ExpectedUeTests` in `run_gate.ps1` is a fifth pin that only moves when test files
are added or removed; v3.5.1 leaves it at 120.
