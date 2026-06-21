# Handoff — v2.11.0 (2026-06-21)

> Supplements `HANDOFF_v2.10.0.md`. Captures the v2.10 → v2.11 jump: three
> stacking GPU-lane phases (cuSPARSE SpMV reactions / Qf-detect cache /
> CUDA stream + async memcpy) plus UE bCudaEnabled wire-up. Use this when
> picking up v2.12.

## TL;DR

- **HEAD on `main`**: `3d2c559` (UE F67 mirror test) ahead of tag `v2.11.0`
  (`3ae1cad`).
- **60 fps PASS through 200K DOF on production code.** 35× speedup at 90 k
  vs v2.9.0 CPU baseline (162 ms → 4.56 ms LAZY+GPU).
- **8 gates all-green**: standalone F1-F66 / F1-F67, UE 58/58 (incl. new
  GPU test), OpenSees, deep audit 104, CLI, v2_roundtrip (CPU + CUDA),
  GPU GATE.
- **kEngineVer**: 2.11.0; **FrameSolver.uplugin Version**: 25.

## The v2.10 → v2.11 commits

| # | hash | one-liner |
|---|---|---|
| 1 | `5a230f7` | docs: PLAN_v2.11_24hr.md -- two-axis push plan |
| 2 | `d467a28` | Phase 1: GPU SPMV reactions via cuSPARSE |
| 3 | `f1b9ba8` | Phase 3: OpenMP RHS opt-in (parallelRhs flag) |
| 4 | `d633040` | Phase 3': Qf-skip cache -- 60 fps @ 150K production CLEARED |
| 5 | `5064194` | Phase 2: CUDA stream + async memcpy -- 60 fps cleared at 200K production |
| 6 | `3ae1cad` | release: v2.11.0 |
| 7 | `792b810` | feat(UE): Phase 7 FRAMECORE_CUDA detection + DLL preload |
| 8 | `3d2c559` | test(UE): FFrameCoreGpuBacksubTest UE F67 mirror |

GitHub release: <https://github.com/rocky59487/architect_simulator/releases/tag/v2.11.0>

## Production scaling curve (this hardware: RTX 5070 Ti Laptop, 12.8 GB)

| nf | LAZY+GPU | 60 fps | 30 fps | 100 ms | speedup vs v2.9.0 CPU LAZY |
|---|---:|:-:|:-:|:-:|---:|
|  90 k | **4.56 ms** | **PASS +12.1** | PASS | PASS | **12.3×** |
| 150 k | **12.40 ms** | **PASS +4.3** | PASS | PASS | **8.7×** |
| 200 k | **14.80 ms** | **PASS +1.8** | PASS | PASS | **~10×** |
| 300 k | 23.20 ms | -6.5 | PASS +10.1 | PASS | |
| 400 k | 32.80 ms | -16.1 | PASS +0.6 | PASS +67.2 | |
| 600 k | 51.50 ms | -34.8 | -18.1 | PASS +48.5 | |

## v2.12 deferred (high → low priority)

### 1. Move remaining CPU RHS stages to GPU

At 200K LAZY+GPU the breakdown is:

```
rhs   = 2.50 ms (eq=0; rest = VecX::Zero + presc + Ff scatter)
bsub  = 6.40 ms
spmv  = 2.40 ms
scat  = 0.56 ms
tot   = 14.80 ms / 16.67 ms budget
```

`rhs.rest` (2.5 ms) and `scat` (0.6 ms) are CPU operations on host buffers.
Moving them onto the same CUDA stream would:
- Eliminate the host-device round-trip wrappers around them
- Let the GPU run them in parallel with subsequent operations

Predicted savings: 1.5-2 ms at 200 K → 60 fps margin of +3-4 ms (currently
+1.8 ms). At 250-300 K this opens 60 fps headroom that the v2.11 build
doesn't have. Implementation: CUDA kernel for Ff scatter + presc applied
to d_b.

### 2. cuDSS PHASE_REFACTORIZATION for P-Delta refactor — **NEGATIVE on frame-tower**

**Status (updated 2026-06-21, v2.11.1 release-hardening pass):** measured
NEGATIVE on the frame-tower fixture (1.04-1.07× only at 90 k-150 k DOF,
see [`../Research/R2_realtime_150k/RESULTS_round4_step3_refactor_negative.md`](../Research/R2_realtime_150k/RESULTS_round4_step3_refactor_negative.md)).
The measurement used a uniform `K' = K*(1+1e-3)` scaling as a proxy for
"numerics-only update"; cuDSS 0.8 returns the numeric factor at near-full
factorization cost on that fixture.

**The original P-Delta motivation is still open** because the fixture is
uniform scaling, while P-Delta `K_sigma` is a non-uniform diagonal-leaning
update on axial-loaded members. Whether `PHASE_REFACTORIZATION` wins on
that pattern was not measured. **First action on day 1 if this gets
re-prioritised:** extend `gpu_bench3_refactor.cpp` with a `K_sigma`-style
perturbation (only stiffness rows of axial-loaded members) and re-measure.

Removed from v2.12 priority list pending that fixture; see
HANDOFF_v2.11.1.md §A-12/D-2.

### 3. UE-side cuDSS lane verification

`feat(UE): Phase 7` wires Build.cs to link cuDSS when the env is present.
Standalone F67 + UE FFrameCoreGpuBacksubTest both verify the GPU path's
numeric correctness, but neither runs an interactive UE workload. Add a
in-editor demo level that drives `useGpuBacksub` on a tower model and
records the per-frame timing inside the UE 5.7 frame budget.

### 4. UE packaging recipe (cuDSS DLLs)

Today `RuntimeDependencies` copies cuDSS DLLs next to the editor binary.
A `Stage` build needs the equivalent bundling into the project's
`StagedBuilds/.../Binaries/Win64/`. Likely a 50-line `BuildTarget`
addition; document and verify.

### 5. Reorder benchmark (METIS vs SCOTCH vs Eigen AMD)

Factor cost is now a small fraction of the per-frame budget; the leverage
is low for the v2.11 headline (60 fps at 200 K). But for 300-600 K
"interactive loading" UX, even a 1.2× factor speedup saves 6-15 s on the
first build. Run on the production fixtures and document. Switch
default if SCOTCH wins by > 20%.

## Durable lessons (extra to HANDOFF_v2.10.0)

13. **Detect-and-skip beats opt-in flags.** Phase 3 (OpenMP) needed a
    user flag because the win depends on workload; Phase 3' (Qf-skip)
    needs no flag because the engine can detect the workload itself.
    Whenever possible, push the property-detection into the engine.
14. **One CUDA stream beats none.** Default-stream + cudaDeviceSynchronize
    cost ~2 ms / frame at 200 K just in host-device sync. The single-stream
    version is two extra API calls (cudssSetStream + cusparseSetStream)
    and a stream destroy in the dtor.
15. **cuDSS DLL load order matters.** cudss64_0 depends on
    cudss_mtlayer_vcomp14064_0 which depends on cudart64_12 which depends
    on the api-ms-win-core-* DLLs the OS provides. Preload them in the
    reverse dependency order in `StartupModule` so the loader doesn't
    have to search PATH.
16. **`bUseUnity = true` survives FRAMECORE_CUDA conditional fields.**
    The Phase 7 patch added 10 conditional fields to `SnSession::Impl`
    under `#ifdef FRAMECORE_CUDA`; UBT still merged the TU into the
    unity blob without conflicts because the conditional fields don't
    introduce anonymous namespaces.
17. **UE 5.7 cold-cache rebuild is fast after one good warm-up.** The
    night-shift v2.10 cycle worried about 1.5 h swap-thrash builds;
    v2.11 added ~150 lines of CUDA Build.cs + cuDSS link and the
    incremental was 37 s. The slow path only triggers on .gitignore
    changes that adapt-out unity.

## Reproducing the v2.11.0 numbers

```powershell
# Activate conda env (OpenBLAS + METIS + cuDSS + cuSPARSE + cudart)
conda activate framecore-direct

# Default 5-leg
$env:FRAMECORE_EXPECTED_ENGINE_VER = "2.11.0"
.\Scripts\run_gate.ps1 -RequireOpenSees

# 6th GPU leg
.\Scripts\run_gpu_gate.ps1

# Headline production numbers
.\Research\R2_realtime_150k\build_r2.bat
$env:Path = "$env:USERPROFILE\anaconda3\envs\framecore-direct\bin;$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;" + $env:Path
.\Research\R2_realtime_150k\r2_bench.exe --preset 90k  --gpu --compare --repeat 30
.\Research\R2_realtime_150k\r2_bench.exe --preset 150k --gpu --compare --repeat 20
.\Research\R2_realtime_150k\r2_bench.exe --preset 200k --gpu --compare --repeat 12

# Stress curve through the VRAM ceiling
.\Research\R2_realtime_150k\r2_bench.exe --preset 300k --gpu --compare --repeat 8
.\Research\R2_realtime_150k\r2_bench.exe --preset 400k --gpu --compare --repeat 5
.\Research\R2_realtime_150k\r2_bench.exe --preset 600k --gpu --compare --repeat 3
```

## Open questions for v2.12

1. Should `parallelRhs` move from opt-in to detect-and-enable? The Qf
   detection could be repurposed: when qfAllZero is false (= distributed
   loads), Phase 3 OpenMP path wins.
2. Should the v2 dispatcher GPU flag default true when the engine binary
   was built with FRAMECORE_CUDA=1? (Carried over from v2.10 -- still open.)
3. cuDSS PHASE_REFACTORIZATION wire-up for P-Delta -- is the productisation
   worth a v2.12 release, or saved for a v3.0 P-Delta-redux release?
