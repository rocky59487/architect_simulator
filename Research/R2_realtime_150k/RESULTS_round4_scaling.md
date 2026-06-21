# R2 Round 4 — production scaling curve after Phase 1+2+3' (v2.11.0)

> Date: 2026-06-21 11:35 (day-shift)
> Hardware: R9 8940HX + RTX 5070 Ti Laptop GPU (Blackwell, 12.8 GB VRAM)
> Build: FRAMECORE_SUPERNODAL=1, FRAMECORE_CUDA=1, /openmp, /O2

## What changed since v2.10.0

Three independent optimisations stacked:

- **Phase 1** (cuSPARSE SpMV for reactions): SpMV stage 7-9 ms → 1-3 ms
- **Phase 2** (one cudaStream + async memcpy): bsub stage saves ~2 ms via fewer
  host-device round-trips
- **Phase 3'** (Qf-detection cache): eq stage 5-6 ms → 0 ms for nodal-loads-only
  fixtures (the educational-game common case)

The Qf-skip is the headline because it requires no flags from the caller and
applies bit-equivalent to every nodal-loads-only model.

## Full production scaling curve

LAZY mode + GPU on the frame-tower fixture (nx × ny × stories braced grid):

| Preset | nx × ny × stories | nf       | factor (ms) | LAZY median | 60 fps | 30 fps | 100 ms |
|--------|-------------------|---------:|------------:|------------:|:------:|:------:|:------:|
|  90k   | 18 × 12 × 60      |  88,920  |   3,400     |  **4.56**   | **+12.1** | PASS    | PASS    |
| 120k   | 18 × 12 × 80      | 118,560  |   4,600     |   7.5       | +9.2     | PASS    | PASS    |
| 150k   | 20 × 15 × 80      | 161,280  |  10,800     |  **12.4**   | **+4.3** | PASS    | PASS    |
| 200k   | 22 × 16 × 90      | 211,140  |  17,250     |  **14.8**   | **+1.8** | PASS    | PASS    |
| 300k   | 24 × 18 × 100     | 287,850  |  35,000     |  23.2       | -6.5     | +10.1   | PASS    |
| 400k   | 26 × 20 × 110     | 377,622  |  53,000     |  32.8       | -16.1    | +0.6    | +67.2   |
| 600k   | 30 × 22 × 130     | 560,238  |  83,156     |  51.5       | -34.8    | -18.1   | +48.5   |

### Frame-rate ceilings (RTX 5070 Ti Laptop GPU, single-precision factor)

- **60 fps PASS through 200K DOF** (production goal of v2.11)
- **30 fps PASS through 400K DOF**
- **Interactive (≤100 ms) PASS through 600K DOF**
- **No-op (factor cost) limit ≈ 600-700K** — at 600K the one-shot factor is
  83 s, which is acceptable for a "load level" gate but breaks the
  educational-game UX if it has to happen mid-session.

## Stage breakdown at the headline sizes

200K LAZY+GPU (Phase 1+2+3'):

```
rhs   = 2.50 ms  (eq=0   Kloop=0   rest=2.50)   <- nodal scatter + presc + Ff scatter
bsub  = 6.40 ms                                <- cuDSS SOLVE on the stream
scat  = 0.56 ms                                <- uf -> u global scatter
spmv  = 2.40 ms                                <- cuSPARSE SpMV reactions
rec   = 0.00 ms                                <- LAZY mode
tot   = 14.80 ms
```

The dominant terms are bsub (43%) + spmv (16%) + rhs (17%). All three have
already been GPU-accelerated; further wins need either a faster GPU
factorisation kernel (cuDSS internal) or moving the residual CPU work
(`rhs.rest`, `scat`) onto the stream too.

90K LAZY+GPU:

```
rhs   = 0.43 ms
bsub  = 2.54 ms
spmv  = 0.68 ms
tot   = 4.56 ms
```

Already at the floor for what a CPU+GPU split can deliver on this workload.
A frame-tower model at 90 k now renders at 219 fps on this hardware.

## Where the wins came from (vs v2.9.0)

| Layer | Where the time went in v2.9.0 (90k) | After Phase 1+2+3' | Saved |
|---|---:|---:|---:|
| RHS sparse-K loop | 12 ms (all-zero work) | 0 (fastpath) | 12 ms (v2.9.0 cdfaeeb) |
| RHS nodeIndex linear scan | 74 ms | 0 (cache) | 74 ms (v2.9.0 cdfaeeb) |
| RHS element addEquivalentNodalLoads | 2 ms | 0 (Qf-skip) | 2 ms (v2.11 d633040) |
| Force recovery | 21 ms | 0 (lazy mode) | 21 ms (v2.9.0 7b2dc65) |
| Backsub (CPU sn::solveSuper) | 47 ms | 2.5 (cuDSS) | 44 ms (v2.10 596b0f9, Phase 2 5064194) |
| Reactions SPMV | 3.5 ms | 0.7 (cuSPARSE) | 2.8 ms (Phase 1 d467a28) |
| Memcpy round-trips | ~3 ms | ~1 (async) | 2 ms (Phase 2 5064194) |
| **TOTAL** | **162 ms** | **4.56 ms** | **35.5× speedup** |

## Negatives also worth recording

- **Phase 3 OpenMP RHS**: kept as opt-in flag `parallelRhs`. For nodal-only
  fixtures (the common case) the fork/join overhead beats the work; Qf-skip
  is the correct attack vector for those. For UDL / self-weight workloads
  the parallel path still wins 4-6x and the flag exists for that.

- **600K LAZY+GPU 51 ms**: dominated by the cuDSS SOLVE itself (30 ms);
  GPU bandwidth is the limit. Future optimisations would need either a
  larger GPU or a sparse factor with better triangular-solve parallelism
  (e.g. supernode-merged triangular kernels).

- **Phase 4 (cuDSS REFACTORIZATION)** not measured: the workload that needs
  it (P-Delta refactor / topology-stable K_sigma updates) isn't covered by
  the frame-tower fixture. Listed as v2.12 follow-up.

- **Phase 5 (reorder benchmark)** not measured: at 90k-200k the factor is
  one-shot and tiny relative to the per-frame budget; even a 1.5× factor
  speedup wouldn't move the v2.11 headline. Listed as v2.12 follow-up.

## What the user gets

The "educational game" deployment plan now reads:

- **Up to 200K DOF model + interactive load drag at 60 fps** on any NVIDIA
  laptop GPU comparable to a 5070 Ti.
- **Up to 400K DOF model + 30 fps interactive** on the same hardware (good
  for the heavier preset).
- **Up to 600K DOF model + ≤100 ms per frame interactive** (acceptable for
  the "load and explore" use case).
- **Cross-vendor CPU fallback** (v2.9.0 sn_chol.h lane) for non-NVIDIA
  hardware: 56 ms / frame @ 90K, still excellent.

## Reproduction

```powershell
conda activate framecore-direct
.\Plugins\FrameSolver\Standalone\build_sn_cuda.bat
.\Research\R2_realtime_150k\build_r2.bat
$env:Path = "$env:USERPROFILE\anaconda3\envs\framecore-direct\bin;$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;" + $env:Path

# Headline numbers
.\Research\R2_realtime_150k\r2_bench.exe --preset 90k  --gpu --compare --repeat 30
.\Research\R2_realtime_150k\r2_bench.exe --preset 150k --gpu --compare --repeat 20
.\Research\R2_realtime_150k\r2_bench.exe --preset 200k --gpu --compare --repeat 12

# Stress curve
.\Research\R2_realtime_150k\r2_bench.exe --preset 300k --gpu --compare --repeat 8
.\Research\R2_realtime_150k\r2_bench.exe --preset 400k --gpu --compare --repeat 5
.\Research\R2_realtime_150k\r2_bench.exe --preset 600k --gpu --compare --repeat 3
```
