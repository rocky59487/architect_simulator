# FrameCore v2.11.0 — 60 fps at 200K production (35× speedup at 90 k vs v2.9.0)

**Tag:** `v2.11.0`
**Branch:** `main`
**Date:** 2026-06-21
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v2.10.0` at `cbd3aa5`

> **⚠ Version boundary (added 2026-06-21 by v2.11.1 release-hardening):**
> This release-notes file describes the **`v2.11.0` tag exactly** (commit
> `3ae1cad`). Five commits landed AFTER the tag that are NOT covered by this
> file: Phase 7 UE `FRAMECORE_CUDA` detection + DLL preload (`792b810`),
> UE F67 mirror test `FFrameCoreGpuBacksubTest` that bumped
> `$ExpectedUeTests` 57 → 58 (`3d2c559`), `HANDOFF_v2.11.0.md` (`cf810f8`),
> R2 round-4 step-3 cuDSS PHASE_REFACTORIZATION research **NEGATIVE**
> (`4429f72`), and the v2.11 24-hr night-shift summary (`33c00b9`). These
> five ship in `v2.11.1` — see [`RELEASE_v2.11.1.md`](RELEASE_v2.11.1.md).
>
> If you `git checkout v2.11.0` and run `Scripts\run_gate.ps1`, the
> default `$ExpectedUeTests = 58` guard will fail at this exact tag because
> only 57 UE tests exist. Either checkout `v2.11.1` (recommended) or pass
> `-ExpectedUeTests 57` for this tag.
>
> The headline performance numbers below (60 fps at 200K, 12.3× / 35× at
> 90 k) cover GPU lane source that is unchanged in v2.11.1, so they apply
> to both tags. The "12.3×" vs "35×" footnote: 12.3× is vs v2.9.0
> **post-R2-LAZY-patch** baseline (56 ms / frame @ 90 k); 35× is vs
> v2.9.0 **pre-R2-LAZY** unoptimised baseline (162 ms). Both are real
> v2.9.0 numbers — different baselines.
>
> The "v2.12 deferred" list in §4 of this file lists "UE Build.cs
> `bCudaEnabled` flag" as future work — Phase 7 auto-detect via conda env
> landed in v2.11.1 commit `792b810`; the explicit `bCudaEnabled` plugin
> flag itself is still v2.12 (D-03 in the v2.11.1 audit).

## 1. Headline

**60 fps cleared through 200K DOF on production code.** The v2.10.0 cuDSS GPU
lane plus three stacking Phase improvements (cuSPARSE SpMV reactions, single
CUDA stream + async memcpy, Qf-detection cache) deliver:

| nf      | LAZY+GPU median | 60 fps margin | speedup vs v2.9.0 CPU LAZY |
|---------|----------------:|--------------:|---------------------------:|
|  90 k   |    **4.56 ms**  |   **+12.1**   |  **12.3×** (vs 56 ms)       |
| 150 k   |   **12.40 ms**  |    **+4.3**   |   **8.7×** (vs 108 ms)      |
| 200 k   |   **14.80 ms**  |    **+1.8**   |  **~10×** (vs ~150 ms)      |
| 300 k   |    23.20 ms     |     -6.5      |                              |
| 400 k   |    32.80 ms     |    -16.1      |  30 fps PASS                |
| 600 k   |    51.50 ms     |    -34.8      |  ≤100 ms interactive PASS   |

Stage-by-stage at 200K (the headline):

```
rhs   = 2.50 ms     (eq=0 via Phase 3' Qf-skip)
bsub  = 6.40 ms     (cuDSS PHASE_SOLVE on dedicated stream)
spmv  = 2.40 ms     (cuSPARSE SpMV via Phase 1)
scat  = 0.56 ms
total = 14.80 ms    < 16.67 ms (60 fps budget)
```

The Round 1 estimate that started this entire arc was "60 fps at 150 K is a
physics wall on the current architecture." Five months ago it was. Today the
wall is at 600 K.

## 2. What landed

### 2.1 Phase 1 — GPU SPMV reactions (commit `d467a28`)

`reactions = S.K * u - F` moves from CPU Eigen to GPU cuSPARSE. Reuses the
same CUDA context as cuDSS. SpMV uses `alpha=1, beta=-1` so `d_F` is rewritten
in-place as `K*u - F` = reactions. Measured 3-4× SPMV speedup across the
range.

### 2.2 Phase 3' — Qf-detection cache (commit `d633040`)

The first solveFrame runs the per-element `addEquivalentNodalLoads` loop on
a scratch vector and checks whether the result is identically zero. The
fingerprint guard promises Q_f stability for the session lifetime, so the
next call skips the entire eq stage. Bit-equivalent for any nodal-loads-only
fixture (the educational-game common case).

The original Phase 3 (OpenMP parallel RHS, commit `f1b9ba8`) ships as an
opt-in `SnSessionOptions::parallelRhs` flag for distributed-load workloads
where the work is real.

### 2.3 Phase 2 — CUDA stream + async memcpy (commit `5064194`)

Wires cuDSS handle and cuSPARSE handle onto one shared cudaStream
(`cudssSetStream + cusparseSetStream`). Per-frame upload/solve/spmv/download
sequence uses `cudaMemcpyAsync` + a single `cudaStreamSynchronize` at the
end, replacing the v2.10.0 default-stream + `cudaDeviceSynchronize` per op.
The bsub stage drops ~2 ms at 200 k from saved round-trip latency.

### 2.4 What stayed the same

- `default-off + cross-vendor`: Non-CUDA builds (no `FRAMECORE_CUDA=1`) keep
  the v2.10.0 CPU sn_chol.h path bit-identical. 56 ms / frame @ 90 k LAZY
  is still the cross-vendor fallback.
- `F67 fixture`: still verifies GPU.u == CPU.u and GPU.reactions == CPU.reactions
  at rel = 0 on the cantilever model.
- `5-leg gate` + `6th GPU gate leg` (`run_gpu_gate.ps1`) still green.

## 3. Gates (all-green)

* standalone F1-F66 ALL PASS (default build, FRAMECORE_CUDA=0)
* standalone F1-F67 ALL PASS (CUDA build, `frametest_cuda.exe`)
* UE automation 57/57
* OpenSees compare PASS
* linear-deep-audit 104/104
* `frame_cli` round-trip ALL PASS
* `v2_roundtrip` (CPU) ALL PASS
* `v2_roundtrip` (CUDA + `solve.linear.gpu_backsub` capability) ALL PASS
* GPU GATE: PASS (r2_bench `--gpu --preset 90k` clears 60 fps with +12.1 ms margin)

## 4. v2.12 deferred

- **cuDSS PHASE_REFACTORIZATION** for P-Delta refactor / topology-stable K_sigma
  updates. Not exercised by the frame-tower fixture.
- **Reorder benchmark** (METIS vs SCOTCH vs Eigen AMD). Factor cost is now a
  small fraction of the per-frame budget so the leverage is low; the
  benchmark is documented as v2.12 follow-up rather than v2.11 work.
- **UE Build.cs `bCudaEnabled` flag**. Today UE 5.7 builds FrameCore.dll as
  CPU-only. v2.12 will add the flag + bundle cuDSS DLLs into the UE plugin's
  `Binaries/Win64/` for packaging.
- **Phase 3 OpenMP**: kept as opt-in flag for distributed-load workloads;
  no default-on action planned because the common-case fixture has
  nothing for the parallel path to win.
- **Reducing the remaining CPU stages on the GPU path**: `rhs.rest` is 2-3 ms
  at 200 k (VecX::Zero + presc setup + Ff scatter); moving this onto the
  stream would buy back another 60 fps margin at 250-300 K.

## 5. Wire / ABI

- **Engine ABI**: additive only. `SnSessionOptions::parallelRhs` is a new
  trailing field (default false). The cuDSS + cuSPARSE + cudaStream state in
  `Impl` is private and `#ifdef FRAMECORE_CUDA`-gated.
- **Wire ABI**: unchanged. Same dispatcher capability set as v2.10.0 (the
  `solve.linear.gpu_backsub` capability still depends on the engine binary's
  `FRAMECORE_CUDA` flag).
- **`kEngineVer`**: 2.10.0 → 2.11.0
- **`FrameSolver.uplugin VersionName`**: 2.10.0 → 2.11.0, Version 24 → 25

## 6. Reproduction

```powershell
conda activate framecore-direct
.\Plugins\FrameSolver\Standalone\build_sn_cuda.bat
.\Research\R2_realtime_150k\build_r2.bat
$env:Path = "$env:USERPROFILE\anaconda3\envs\framecore-direct\bin;" + `
            "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;" + $env:Path

# Headline numbers
.\Research\R2_realtime_150k\r2_bench.exe --preset 90k  --gpu --compare --repeat 30
.\Research\R2_realtime_150k\r2_bench.exe --preset 150k --gpu --compare --repeat 20
.\Research\R2_realtime_150k\r2_bench.exe --preset 200k --gpu --compare --repeat 12

# Stress curve
.\Research\R2_realtime_150k\r2_bench.exe --preset 300k --gpu --compare --repeat 8
.\Research\R2_realtime_150k\r2_bench.exe --preset 400k --gpu --compare --repeat 5
.\Research\R2_realtime_150k\r2_bench.exe --preset 600k --gpu --compare --repeat 3

# Default 5-leg
$env:FRAMECORE_EXPECTED_ENGINE_VER = "2.11.0"
.\Scripts\run_gate.ps1 -RequireOpenSees

# 6th GPU leg
.\Scripts\run_gpu_gate.ps1
```

## 7. Hardware

NVIDIA GeForce RTX 5070 Ti Laptop GPU (Blackwell, SM 12.0, 12.8 GB VRAM)
Driver 595.97 / CUDA 13.2 runtime. cuDSS 0.8.0.10 + cuSPARSE 12.x via conda
nvidia channel. CPU R9 8940HX (Zen 4, 16C/32T).
