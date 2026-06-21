# R2 Round 4 step 3 — cuDSS PHASE_REFACTORIZATION: NEGATIVE on this workload

> Date: 2026-06-21 12:30
> Hardware: RTX 5070 Ti Laptop GPU, cuDSS 0.8.0.10
> Verdict: not viable for production integration -- documented for v2.12 pickup

## What was tested

`Research/R2_realtime_150k/gpu_bench3_refactor.cpp` mirrors the gpu_bench2 setup
(frame-tower fixture, cuDSS via the conda env) and adds a numerics-only K mutation:

```
K' = K * (1 + 1e-3)   // pattern stays, values change uniformly
```

Then times `cudssExecute(PHASE_REFACTORIZATION)` on `K'` against the original
`cudssExecute(PHASE_FACTORIZATION)` on `K`, plus a residual check via
`cudssExecute(PHASE_SOLVE)` on `K'`.

## Measurements

```
fixture: 90k frame tower (nf=88,920 / nnz=3,833,496)
  PHASE_ANALYSIS         : 253.47 ms (one-shot, shared between both factor paths)
  PHASE_FACTORIZATION    : 352.73 ms
  PHASE_SOLVE (on K)     :  16.68 ms (first-call warmup)
  PHASE_REFACTORIZATION  : 330.79 ms <- only 22 ms faster
  PHASE_SOLVE (on K')    :   2.36 ms
  speedup REFACTOR vs FACTOR: 1.07x
  residual rel on K'     : 5.3e-10 (PASS)

fixture: 150K frame tower (nf=161,280 / nnz=6,934,608)
  PHASE_ANALYSIS         :  511.77 ms
  PHASE_FACTORIZATION    : 1052.37 ms
  PHASE_SOLVE (on K)     :   19.30 ms
  PHASE_REFACTORIZATION  : 1015.15 ms <- 37 ms faster
  PHASE_SOLVE (on K')    :    4.58 ms
  speedup REFACTOR vs FACTOR: 1.04x
  residual rel on K'     : 1.3e-9  (PASS)
```

## Verdict

REFACTORIZATION is **1.04-1.07× the FACTORIZATION speed** on this workload — far
below NVIDIA's typical 3-5× claim for `cuDSS::REFACTORIZATION`. Residual quality
matches the FACTORIZATION path (FP64 round-off floor ~ 1e-9 at 150K). The patch
works numerically but doesn't deliver the predicted production-relevant speedup.

## Why the predicted 3-5× didn't materialise

Several plausible reasons; we don't currently have data to distinguish them:

1. **cuDSS 0.8 may not skip as much work as later versions**. The 3-5× number
   appears in 2024 cuDSS slides referring to "newer ordering reuse"; 0.8 may
   only skip the AMD/METIS reorder, not the symbolic factor.
2. **Frame-tower sparsity is benign**. The numeric factor itself is ~ 1 GFlop/s
   for this kind of structured matrix; saving the symbolic phase (a small fraction
   of factor cost) leaves the dominant numeric pass unchanged.
3. **The cuDSS 0.8 API doesn't expose a "values-only" hint**. We call
   `cudssMatrixSetValues(cuK, d_val)` which signals new values but the
   library may still re-run the numeric factor from scratch.

Whatever the cause, the measured speedup is **not worth the production
integration cost** for a 1.04-1.07× factor speedup. The cuDSS GPU lane stays
on the v2.11.0 `PHASE_FACTORIZATION` path for all per-event refactors.

## Where this leaves the v2.12 list

`HANDOFF_v2.11.0.md` listed REFACTORIZATION as item 2 in the v2.12 priorities.
After this measurement it drops off the list. Restored P-Delta interactive
performance now needs either:

- A future cuDSS version with real ordering-reuse speedup, OR
- A different attack vector (Woodbury Sherman-Morrison rank-1 update of the
  GPU factor for one-element-at-a-time K changes; out of scope for v2.12).

Item 1 in v2.12 (moving the remaining CPU RHS stages onto the GPU stream) stays
the highest-leverage v2.12 target.

## Not a regression

This is a research-lane prototype that did not touch production code. The v2.11.0
release lane is unaffected. No commit hits main except this writeup +
`gpu_bench3_refactor.cpp` / `build_gpu_bench3.bat` (research-only, gitignored
binaries).
