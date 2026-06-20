# Handoff — v2.10.0 (post night-shift 2026-06-21, extended segment)

> Supplements `HANDOFF_v2.9.0.md`. Captures the GPU lane integration and the
> three-step R2 round-3 research journey behind it.

## TL;DR

- **HEAD on `main`**: `c6924d7` (night-shift v2.10 addendum) ahead of tag
  `v2.10.0` (`cbd3aa5`).
- **Tag `v2.10.0`** ships the cuDSS GPU lane: `SnSessionOptions::useGpuBacksub`
  + `#ifdef FRAMECORE_CUDA` integration. **60 fps @ 90k production** /
  **30 fps @ 161K (~150K) production** / **interactive @ 211K (~200K)**.
- **All gates green**: standalone F1-F66 (default) + F1-F67 (CUDA),
  UE 57/57, OpenSees, deep audit 104, CLI, v2_roundtrip.
- **kEngineVer**: 2.10.0; **FrameSolver.uplugin Version**: 24, VersionName 2.10.0.

## The night-shift extended segment (03:25 onward)

The user woke up mid-shift, gave hardware specs (R9 8940HX + RTX 5070 Ti),
and said "any tech is fair game, just verify rigorously." The shift moved into
R2 round 3 -- attacking the backsub stage that v2.9.0 left as the dominant
per-frame cost (47-91 ms across 90k-160k).

### Step 1 — CPU FP32 mixed-precision IR: NEGATIVE

`Research/R2_realtime_150k/fp32_safety.cpp` ran Eigen FP32 SimplicialLDLT
against FP64 on the 90k frame tower:

```
FP64 LDLT (Eigen):  factor=44.9s   solve=115ms   resInf/||F||=2.0e-9
FP32 LDLT (Eigen):  factor=35.0s   solve= 96ms   IR_step=99.6ms
residual rel:       IR=0  0.879   IR=1  0.0168   IR=2  3.5e-4
speedup FP32 backsub: 1.19x
```

Root cause: the braced frame tower has κ ≈ 1e8 (axial EA/L vs bending
12EI/L³ is 270× per element, multiplied through 60 stories of three section
sizes). FP32 IR-converges threshold is 1/u_FP32 ≈ 8e6 — violated by 12×.
The FP32 factor itself is unstable on this matrix.

Speedup also unimpressive (+20% vs predicted 2×).

**Decision**: drop CPU FP32. Captured in
`Research/R2_realtime_150k/RESULTS_round3_fp32_negative.md`.

### Step 2a — cusolverSp high-level GPU: NEGATIVE

`Research/R2_realtime_150k/gpu_bench.cpp` ran `cusolverSpDcsrlsvchol`:

```
cusolverSpDcsrlsvchol solve (90k): 9501.79 ms
residual rel: 4.2e-10
```

Per NVIDIA docs the `cusolverSp` `csrchol*` family runs **on the host CPU**,
single-threaded. The GPU device pointers we passed were upload/download
endpoints, not compute targets. 170× SLOWER than the v2.9.0 CPU sn_chol path.

**Decision**: cusolverSp is the wrong tool. cuDSS (NVIDIA 2024+) is.

### Step 2b — cuDSS GPU-native: POSITIVE

`Research/R2_realtime_150k/gpu_bench2.cpp` ran the cuDSS PHASE_ANALYSIS +
PHASE_FACTORIZATION + PHASE_SOLVE pattern. Per-frame end-to-end
(upload b + GPU solve + download x):

| nf      | per-frame | residual rel |
|---------|----------:|-------------:|
| 90,402  | **2.5 ms** | 5.3e-10 |
| 120,042 | 3.3 ms     | 7e-10   |
| 163,296 | 5.5 ms     | 1.6e-9  |
| 207,000 | 6.8 ms     | 1.7e-9  |

60 fps PASS through the entire 90k-200K range on RTX 5070 Ti.

Captured in `Research/R2_realtime_150k/RESULTS_round3_gpu_success.md`.

## Production integration (commit `596b0f9`)

`SnSessionOptions::useGpuBacksub` (default false). When true AND
`FRAMECORE_CUDA=1`, `SnSession::Impl` carries cuDSS state under `#ifdef`;
ctor runs PHASE_ANALYSIS + PHASE_FACTORIZATION, solveFrame routes
PHASE_SOLVE, dtor tears down.

### Ctor refactor

The SnPrimary-reuse path used to early-return after caching the IR pattern;
that bypassed the GPU setup block at the end of ctor. The early return became
a fall-through into the `else if` for the self-built supernodal factor branch
— semantically identical. F66's "consecutive solveFrame consistency" check
still passes (rel = 0).

### F67 fixture (CUDA-only)

`Standalone/main.cpp` adds F67 under `#if defined(FRAMECORE_SUPERNODAL) &&
defined(FRAMECORE_CUDA)`. Seven checks:

- F67 SnPrimary prepared non-singular
- F67 CPU-mode SnSession valid
- F67 GPU-mode SnSession valid
- F67 GPU-mode diagnostic carries [GPU] tag
- F67 GPU.u == CPU.u (rel < 1e-8)
- F67 GPU.reactions == CPU.reactions (rel < 1e-8)
- F67 GPU and CPU memberForces sizes match

Measured rel = 0 on the cantilever fixture (cuDSS METIS reorder matches CPU
reorder exactly at this size); the 1e-8 tolerance is for larger fixtures.

### r2_bench --gpu production measurements

After `cdfaeeb` (v2.9.0 CPU lane) and `596b0f9` (v2.10.0 GPU lane):

| nf      | LAZY+GPU  | 60fps | 30fps | speedup vs v2.9.0 CPU LAZY |
|---------|----------:|:------|:------|---------------------------:|
| 90,402  | 12.4 ms   | PASS  | PASS  | 4.5× (vs 56 ms)             |
| 161,280 | 25.6 ms   | -8.9  | PASS  | 4.2× (vs 108 ms)            |
| 211,140 | 34.0 ms   | -17.3 | -0.6  | ~4.4× (vs ~150 ms)          |

GPU backsub itself drops 47 ms → 3.2 ms at 90 k (14×) and 91 ms → 6.4 ms at
160 k (14×). RHS assembly + reactions SPMV stay on the CPU (~8 ms + 7 ms at
160 k); that's why 60 fps slips at 160 k -- the GPU has nothing left to
accelerate on the CPU-bound stages.

## v2.11 deferred items (priority order)

### 1. v2 dispatcher GPU wire-up

The v2 dispatcher always uses default `SnSessionOptions`, so a wire client
can't opt into the GPU lane today. v2.11 should:

- Add body field `gpuBacksub: bool` (default false) to `solve.linear` and
  `analysis.reanalysis_solve` requests.
- Add capability `solve.linear.gpu_backsub` advertised in hello when the
  loaded engine was built with `FRAMECORE_CUDA=1` (compile-time detect).
- Pass through to `SnSessionOptions::useGpuBacksub` when constructing the
  session.

### 2. UE FrameCore Build.cs `bCudaEnabled` flag

UE 5.7 builds default lane only today (UE rebuild times for FrameCore are
~40 s; CUDA link adds dependency on cuDSS + cudart runtime DLLs that need
to ship with the packaged game). v2.11 should:

- Add `bCudaEnabled = false` default to `FrameCore Build.cs`.
- When on, add CUDA include + cuDSS include, link `cudss.lib + cudart.lib`,
  preload `cudss64_0.dll` in `FrameCoreModule.cpp` alongside the existing
  OpenBLAS preload.
- Document the runtime DLL bundle in UE packaging docs.

### 3. Optional 6th gate leg `run_gpu_gate.ps1`

Activate only when the conda env has cuDSS installed. Runs
`frametest_cuda.exe` + a future v2 roundtrip that exercises the GPU lane.
This keeps default CI minimal but ensures the GPU path doesn't silently
regress on CUDA-capable dev boxes.

### 4. RHS + reactions SPMV on GPU

At 160 k LAZY+GPU, solveFrame is 25.6 ms (60 fps -8.9 ms). The breakdown:

```
rhs   = 7.8 ms  (CPU)
bsub  = 6.4 ms  (GPU cuDSS)
spmv  = 7.0 ms  (CPU reactions K*u)
scat  = 0.6 ms  (CPU)
```

If RHS + reactions SPMV move to GPU (cuSPARSE SpMV), we lose the
upload(b) + download(x) one-shot pattern; instead the entire u-life-cycle
stays on device. Predicted: 160 k LAZY+GPU → ~12 ms, 200 k → ~17 ms;
60 fps reachable through 200 K. Significant engineering work.

### 5. B1 ReSolveSession dispatcher session-cache routing

Still v2.6 deferred. `HandleReanalysis` rebuilds the Woodbury → PCG →
rebaseline ladder per call.

### 6. B2 `model.patch` schema + dispatcher wire

Still NOT_IMPLEMENTED.

## Durable lessons (extra to HANDOFF_v2.9.0)

7. **cusolverSp `csrchol*` is HOST-based.** Don't assume the SP suffix means
   GPU compute. cuSOLVER's GPU-native sparse direct solver is cuDSS (2024+).
8. **FP32 doesn't survive building-stiffness κ.** Axial vs bending stiffness
   ratios alone are 200-300× per element; multiplied through stories +
   section sizes, κ ≈ 1e8 is normal. Higham IR-converges threshold of 8e6
   is violated routinely. Don't try FP32 IR on a stiffness matrix without
   first checking κ on the fixture.
9. **cuDSS DLL group**: cudss64_0 + cudss_mtlayer_vcomp14064_0 + cudart64_12
   + cusparse64_12 + cublas64_12 + cublasLt64_12 + nvJitLink_120_0 all
   transitively required. Find them with `dumpbin //dependents` (double-slash
   in Git Bash to defeat path translation) and co-locate.
10. **conda `-c nvidia` lightweight CUDA bundle**: `cuda-cudart-dev
    libcusolver-dev libcusparse-dev libcublas-dev libcudss-dev libnvjitlink
    cuda-cccl cuda-crt cuda-nvcc` — ~1.5 GB, enough to build cuDSS host
    code with cl (no nvcc kernel code needed).
11. **Research lane can graduate to main**. `Research/R2_realtime_150k/`
    now ships its prototypes + writeups in the repo (`gpu_bench.cpp`,
    `gpu_bench2.cpp`, `fp32_safety.cpp`, three RESULTS docs). The exe / obj
    artefacts stay `.gitignored`. PROGRESS_R_supernodal set the precedent.
12. **Default-off opt-in is a deployment requirement**, not a style choice.
    The educational game's target may not have NVIDIA hardware; the v2.10.0
    architecture lets the same engine binary cover both with a single flag.

## Reproducing the v2.10.0 numbers

```powershell
# Activate conda env containing OpenBLAS + METIS + cuDSS + CUDA runtime
conda activate framecore-direct

# Default standalone gate (CPU only)
.\Plugins\FrameSolver\Standalone\build.bat
.\Plugins\FrameSolver\Standalone\frametest.exe   # F1-F66

# CUDA-enabled standalone gate
.\Plugins\FrameSolver\Standalone\build_sn_cuda.bat
.\Plugins\FrameSolver\Standalone\frametest_cuda.exe   # F1-F67

# r2 bench with GPU on
.\Research\R2_realtime_150k\build_r2.bat
$env:Path = "$env:USERPROFILE\anaconda3\envs\framecore-direct\bin;" + `
            "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;" + $env:Path
.\Research\R2_realtime_150k\r2_bench.exe --preset 90k  --gpu --compare --repeat 30
.\Research\R2_realtime_150k\r2_bench.exe --preset 150k --gpu --compare --repeat 20
.\Research\R2_realtime_150k\r2_bench.exe --preset 200k --gpu --compare --repeat 12

# Full 5-leg + 6th leg
$env:FRAMECORE_EXPECTED_ENGINE_VER = "2.10.0"
.\Scripts\run_gate.ps1 -RequireOpenSees
python .\Tools\v2_roundtrip.py
```

## Open questions for v2.11

1. Should the v2 dispatcher GPU flag default true when the engine binary was
   built with `FRAMECORE_CUDA=1`? Pro: opt-out is a one-flag change for
   NVIDIA-equipped clients. Con: hidden state surprises non-GPU clients.
2. Should the CUDA-build path replace the default in CI, or run alongside?
   Pro: catches GPU regressions immediately. Con: blocks releases on conda
   availability.
3. How aggressively should RHS + reactions SPMV migrate to GPU? Full migration
   gets 60 fps at 200 K but ties more of the engine to NVIDIA hardware.
