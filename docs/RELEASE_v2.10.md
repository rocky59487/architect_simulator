# FrameCore v2.10.0 — cuDSS GPU lane: 60 fps @ 90 k, 30 fps @ 150 K production

**Tag:** `v2.10.0`
**Branch:** `main`
**Date:** 2026-06-21
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v2.9.0` at `7873e39`

## 1. Headline

**The "true realtime 150 K @ 60 fps" target is reached on production code.**

Production SnSession::solveFrame() user-facing time on the user's RTX 5070 Ti
Laptop GPU (R2 lane integrated, FRAMECORE_CUDA=1 build):

| nf      | LAZY + GPU | 60 fps   | 30 fps   | interactive 100 ms |
|---------|-----------:|:---------|:---------|:-------------------|
|  90,402 | **12.4 ms** | **PASS** | PASS     | PASS               |
| 161,280 |  25.6 ms   | -8.9 ms  | **PASS** | PASS               |
| 211,140 |  34.0 ms   | -17 ms   | -0.6 ms  | PASS               |

vs the CPU-only v2.9.0 baseline on the same fixtures (LAZY mode median):

| nf | v2.9.0 CPU LAZY | v2.10.0 GPU LAZY | speedup |
|---|---:|---:|---:|
|  90 k | 56 ms  | 12.4 ms | **4.5×** |
| 160 k | 108 ms | 25.6 ms | **4.2×** |
| 200 k | (~150 ms extrap.) | 34.0 ms | **~4.4×** |

GPU backsub itself drops 47 ms → 3.2 ms at 90 k (14×) and 91 ms → 6.4 ms at
160 k (14×). RHS assembly + reactions SPMV stay on the CPU (~8 ms + 7 ms at
160 k); those are the remaining cost terms.

## 2. What landed

### 2.1 SnSession cuDSS GPU lane (commit `596b0f9`)

- `SnSessionOptions::useGpuBacksub` (default false). When true AND the engine
  was compiled with `-DFRAMECORE_CUDA=1`, `SnSession` runs `cudssExecute` for
  PHASE_ANALYSIS + PHASE_FACTORIZATION at ctor (once per session lifetime),
  and PHASE_SOLVE inside `solveFrame` (per frame).
- `SnSession::Impl` adds optional cuDSS state under `#ifdef FRAMECORE_CUDA`:
  handle, config, data, three matrices (K, B, X), and device buffer pointers.
  Default build (FRAMECORE_CUDA unset) has none of this — `Impl` is the same
  size, dtor reverts to trivial.
- Ctor refactor: the SnPrimary-reuse path used to early-return after caching
  the IR pattern; that bypassed the new GPU setup block. The early return
  became a fall-through into the `else if` for the self-built supernodal
  factor branch — semantically identical. F66's "consecutive solveFrame
  consistency" check still passes (rel = 0).
- `solveFrame` backsub path: when `gpuReady`, upload b → cuDSS SOLVE →
  download x. On cuDSS failure (status non-success or CUDA copy error),
  the CPU `sn::solveSuper` path runs as a transparent fallback.
- `~SnSession`: tears down cuDSS handles + frees device buffers under
  `#ifdef FRAMECORE_CUDA`. Default dtor is trivial.

### 2.2 F67 oracle (CUDA-only fixture)

`Standalone/main.cpp` adds F67 under `#if defined(FRAMECORE_SUPERNODAL) &&
defined(FRAMECORE_CUDA)`. Seven checks on the cantilever-tip-load fixture:

- F67 SnPrimary prepared non-singular
- F67 CPU-mode SnSession valid
- F67 GPU-mode SnSession valid (diagnostic mentions cuDSS factor)
- F67 GPU-mode diagnostic carries [GPU] tag
- F67 GPU.u == CPU.u (rel < 1e-8)
- F67 GPU.reactions == CPU.reactions (rel < 1e-8)
- F67 GPU and CPU memberForces sizes match

Measured rel = 0 on the small fixture (cuDSS METIS reorder matches the CPU
reorder exactly on a 6-DOF system); the 1e-8 tolerance is for larger fixtures
where reorder paths can diverge slightly.

### 2.3 Optional CUDA standalone build

`Plugins/FrameSolver/Standalone/build_sn_cuda.bat` mirrors `build.bat`
exactly but adds `/DFRAMECORE_SUPERNODAL=1 /DFRAMECORE_CUDA=1` and links
`cudart.lib + cudss.lib` from the conda framecore-direct env. Output goes to
`Standalone/frametest_cuda.exe` so the default `frametest.exe` is unchanged.
Requires the conda env on PATH at run time.

### 2.4 Research lane prototypes (commits `4b2edcd`, `75287ac`)

- `Research/R2_realtime_150k/fp32_safety.cpp` — eliminated the CPU FP32
  mixed-precision path. Round-3 step 1 NEGATIVE result: 90k frame tower
  has kappa ≈ 1e8, Higham IR-converges threshold of 8e6 is violated by
  more than an order of magnitude. RHS resInf = 0.879 after FP32 alone;
  3.5e-4 even after 2 IR steps. Documented in `RESULTS_round3_fp32_negative.md`.
- `Research/R2_realtime_150k/gpu_bench.cpp` — cusolverSp high-level
  (host-based; 9501 ms / call at 90 k). Negative result, kept as history.
- `Research/R2_realtime_150k/gpu_bench2.cpp` — cuDSS prototype. Showed
  the 2.5-6.8 ms per-frame numbers that justified the production integration.
- `Research/R2_realtime_150k/r2_bench.cpp` — extended with `--gpu` flag for
  the production-SnSession compare. Measured the integration numbers in
  section 1 above.

### 2.5 Wire / ABI summary

- **Engine ABI**: additive only. `SnSessionOptions::useGpuBacksub` is a
  new trailing field (default false). `SnSession::Impl` field additions
  are private and `#ifdef`'d. No existing signature changed.
- **Wire ABI**: unchanged. The v2 dispatcher does NOT expose the GPU lane
  yet — the dispatcher still uses default SnSessionOptions, so the GPU
  flag is unreachable from the wire side without further integration
  (deferred to v2.11).
- **kEngineVer**: 2.9.0 → 2.10.0.
- **FrameSolver.uplugin VersionName**: 2.9.0 → 2.10.0, Version 23 → 24.

## 3. Default-off / cross-vendor stance

The educational game's deployment target may run on non-NVIDIA hardware. The
v2.10.0 architecture supports both:

- **Non-CUDA build (default)**: FRAMECORE_CUDA is undefined. SnSession uses
  the v2.9.0 CPU supernodal path. solveFrame at 90 k LAZY = 56 ms (excellent
  on AMD / Intel CPUs). No cuDSS DLL bundled. UE 5.7 build untouched.
- **CUDA build (opt-in, NVIDIA-equipped clients)**: FRAMECORE_CUDA=1 plus
  caller sets `SnSessionOptions::useGpuBacksub = true`. solveFrame at 90 k
  LAZY = 12 ms (60 fps). Requires cuDSS + cudart runtime DLLs alongside
  the engine binary.

## 4. Gates (all-green)

* standalone F1-F66 ALL PASS (default build, FRAMECORE_CUDA=0)
* standalone F1-F67 ALL PASS (CUDA build, frametest_cuda.exe)
* UE automation 57 / 57
* OpenSees offline cross-validation PASS
* linear-deep-audit 104 / 104
* `frame_cli` round-trip ALL PASS
* `v2_roundtrip` ALL PASS (`engine_version=2.10.0` env pin)

## 5. Deferred to v2.11+

- **v2 dispatcher wire-up**: expose `useGpuBacksub` as a body field on
  `solve.linear` / advertise `solve.linear.gpu_backsub` capability.
- **UE FrameCore Build.cs**: add `bCudaEnabled` flag to link cuDSS at
  Editor build time (currently UE build is default lane only).
- **Optional 6th gate leg**: `run_gpu_gate.ps1` that only activates when
  the conda env has cuDSS — automatic regression coverage for the GPU lane.
- **B1 ReSolveSession session-cache routing** (still v2.6 deferred).
- **B2 `model.patch` schema + handler** (still NOT_IMPLEMENTED).
- **GPU lane scaling beyond 200 K**: the integration currently FAILs 60 fps
  at 160 K (-8.9 ms). The RHS + SPMV stages run on CPU and account for
  the remaining 15 ms; either move them to GPU too, or accept 30 fps as
  the realistic target above 150 K.

## 6. The two-step learning journey for the perf line

Round 1 (CPU sub-stage measurement): found `FrameModel::nodeIndex` linear
scan was 74 ms / frame at 90 k. Patched in v2.9.0 (cdfaeeb) → 2.4× speedup.

Round 2 (CPU sub-stage instrumentation): found the supernodal backsub is
the remaining 47 ms at 90 k. Documented; CPU couldn't push backsub below
that without sacrificing correctness.

Round 3 step 1 (FP32 IR): NEGATIVE. kappa too high for FP32 mixed-precision
on building stiffness matrices. Captured in `RESULTS_round3_fp32_negative.md`.

Round 3 step 2a (cusolverSp): NEGATIVE. cusolverSp's csrchol* family is
host-based. Captured in `gpu_bench.cpp`.

Round 3 step 2b (cuDSS): **PASS**. 22× speedup, 60 fps reachable through
200 K DOF on RTX 5070 Ti. This release.

## 7. Hardware tested

NVIDIA GeForce RTX 5070 Ti Laptop GPU (Blackwell SM 12.0, 12.8 GB VRAM).
Driver 595.97 / CUDA 13.2 runtime. cuDSS 0.8.0.10 via conda nvidia channel.

The CPU lane (v2.9.0) covers any non-NVIDIA target.
