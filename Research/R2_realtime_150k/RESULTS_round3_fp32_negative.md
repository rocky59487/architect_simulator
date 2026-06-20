# R2 Round 3 step 1 — FP32 mixed-precision SAFETY CHECK: NEGATIVE result

> Date: 2026-06-21 08:50 (night-shift extended segment, user awake)
> Hardware: R9 8940HX (Zen 4, 16C/32T) + RTX 5070 Ti (Blackwell, 12 GB VRAM)
> User clarified hardware + asked for "more research, any tech is fair game"

## Question

Per `PLAN_round3_mixed_precision.md`: is the 90 k mixed-frame-tower stiffness
well-conditioned enough that Eigen FP32 SimplicialLDLT + 1-2 Neumaier IR steps
converges to FP64 baseline? (Higham §15.5 threshold: κ(A) < 1/u_FP32 ≈ 8 × 10⁶.)

## Method

`Research/R2_realtime_150k/fp32_safety.cpp` independent of the production
supernodal lane:

1. Build the same 90 k frame tower as `r2_bench` (nx=18, ny=12, stories=60).
2. Assemble K_ff directly in `Eigen::SparseMatrix<double>` using the public
   `localStiffness12 + transform12` element APIs (frame tower has no releases /
   hinges / shells, so this matches `BeamColumnElement::addToK` exactly).
3. Build RHS Ff from the tower's nodal-load fixture.
4. Solve FP64 baseline (Eigen `SimplicialLDLT<double>`) → `x64`.
5. Cast K_ff and Ff to FP32, solve with `SimplicialLDLT<float>` → `x32`.
6. Compute residual in FP64: `r = Ff - K * x32_as_double`, take ‖r‖_∞ / ‖Ff‖_∞.
7. Run 1 IR step: cast `r` to FP32, solve `d = ldlt32.solve(r32)`, x += d in FP64.
8. Repeat for 2 IR steps.

## Results

```
fixture: tower nx=18 ny=12 stories=60 (~90k DOF)
N=90402 nf=88920 members=46140
assemble Kff: nnz=3833496 build=222ms
RHS Ff: ||F||_inf = 3.800e+04

FP64 LDLT (Eigen):  factor=44901ms  solve=115ms  resInf/||F||=2.018e-09
FP32 LDLT (Eigen):  factor=35028ms  solve= 96ms  IR_step=99.6ms
residual rel:       IR=0  8.793e-01   IR=1  1.682e-02   IR=2  3.474e-04
|x32 - x64|_inf/|x64|_inf:  IR=0 1.842e-02   IR=1 3.520e-04   IR=2 6.742e-06
speedup FP32 backsub vs FP64 backsub: 1.19x

VERDICT
  res0 < 1e-3 (visual-good FP32 alone):  FAIL  (got 0.879 — basically NO convergence)
  res<=1e-9 after 1 or 2 IR steps:       FAIL  (best is 3.5e-4 at 2 IR steps)
  FP32 backsub faster than FP64 backsub: PASS  (1.19x — but marginal)
OVERALL: mixed-precision path is NOT VIABLE -- pivot to GPU offload (RTX 5070 Ti)
```

## Why it failed (root-cause)

The 90 k braced frame tower has an inherently bad κ for FP32:

- **Axial vs bending stiffness mismatch**: axial EA/L for a 520 mm² section over
  3.3 m ≈ 16 GN/mm; bending 12EI/L³ for a 220 mm² brace ≈ 60 MN/mm. The ratio
  alone is ~270× per element; through 60 stories the global K_ff spectrum spans
  many decades.
- **Mixed section sizes**: column / beam / brace use sections 520² / 360² / 220².
  Stiffness contributions differ by (520/220)⁴ ≈ 31× just in the Iy term.

These are not numerical artefacts; they're the physics of a building. FP32's
~6-decimal precision can't span the spread, so the FP32 factor itself stores
the off-diagonal terms with too much loss for IR to recover. Residual 0.879
after a bare FP32 solve confirms the FP32 factorization is itself unstable on
this matrix.

Higham's IR-converges threshold κ < 8e6 is violated by an order of magnitude or
more on this fixture.

## Speedup measurement is also unimpressive

FP32 backsub was only 1.19× faster than FP64 backsub at 90 k:

- FP64 LDLT solve: 115 ms
- FP32 LDLT solve:  96 ms

Even if accuracy hadn't failed, +20 % is not enough to clear the 60 fps gap (we
needed ~2× to push 90 k LAZY from 56 ms toward 33 ms). The dense BLAS3 panel
sizes that drive 2× mixed-precision speedups on supercomputers don't materialise
on Eigen's column-LDLT path — and the self-built `sn_chol.h` would face the same
memory-bandwidth wall (sparse triangular solve is fundamentally bandwidth-bound).

## Decision

**Abandon CPU mixed-precision FP32 backsub.** The PLAN_round3_mixed_precision
document is now superseded. The next leverage point is GPU offload of the
backsub stage, exploiting the available RTX 5070 Ti.

## GPU lane plan (round 3b, next session)

Target: 90 k LAZY backsub 46 ms → ≤ 10 ms (60 fps reachable).

### Prerequisites the user must install

- CUDA Toolkit 12.x or 13.x (current driver supports CUDA 13.2):
  `winget install Nvidia.CUDA` or download from developer.nvidia.com.
  `nvcc.exe` on PATH; CUDA include / lib reachable for `cl` to compile +
  link `cusolver.lib / cusparse.lib`.

### Implementation outline

1. **Bench-first, integrate later**: write `Research/R2_realtime_150k/gpu_bench.cu`
   that takes the same K_ff CSC arrays (from the IR cache or manual assembly)
   and runs cuSOLVER `cusolverSpDcsrlsvchol` factor + backsub. Measure:
   - upload time (CSC arrays + b → device)
   - factor time (one-shot)
   - backsub time per call (the SnSession reuse target)
   - download time (x → host)
   - Verify residual matches `r2_bench` FP64 baseline.
2. If end-to-end per-frame time (upload b + GPU backsub + download x) is
   < 16.67 ms at 90 k, GPU lane is GO. Otherwise document the wall and pick
   the next candidate.
3. **Production path** (only if GO):
   - `SnSessionOptions::useGpuBacksub = false` default.
   - `SnSession::Impl` adds optional GPU factor handle (cusolverDnSpoolMatHandle_t /
     similar) populated at ctor when flag is on.
   - `solveFrame` routes `sn::solveSuper` → GPU backsub when flag is on.
   - `FRAMECORE_CUDA=1` gates the whole path so non-CUDA builds compile clean.

### What's still unknown without measurement

- GPU upload/download latency on the per-frame path (PCIe gen4 ~ 16 GB/s; 90 k
  × 8 bytes RHS = 720 KB ≈ 45 μs upload, negligible). But cuSOLVER itself adds
  call overhead — we need numbers, not guesses.
- Whether cuSOLVER's sparse Cholesky beats OpenBLAS at 90 k. Reports vary: at
  sub-100k DOF the GPU often loses to optimized BLAS, because the kernel
  launch overhead eats the parallelism win. RTX 5070 Ti is Blackwell, but the
  workload size matters.
- The mixed-frame-tower fingerprint will need re-confirming on the GPU side:
  cusolverSpDcsrlsvchol uses METIS-equivalent ordering; results should match
  the self-built supernodal numerically.

## Updated CANDIDATES status

```
A — Mixed precision + IR :  DROP (this file's negative result)
B — Schur local update   :  Reserved for collapse-event re-factor speedup (orthogonal axis)
C — GPU offload          :  PROMOTED (next attempt; RTX 5070 Ti available)
D — BLR                  :  Still drop (1.5x not enough)
E — Reorder              :  Drop (5-15% not enough)
F — Multi-RHS batch      :  Drop (educational game is single-RHS per frame)
G — Lazy recover         :  SHIPPED (v2.9.0)
H/I — RHS fastpath       :  SHIPPED (v2.9.0)
```

## What's NOT a counter-argument to FP32 here

- "Try `Eigen::ConjugateGradient<float>` with FP32 preconditioner and FP64
  residual": same κ problem, same convergence failure.
- "Use scaled / equilibrated K": Eigen's `SimplicialLDLT` already does the
  AMD ordering with diagonal regularisation; further scaling doesn't recover
  the FP32 precision loss in this κ range.
- "Higher IR iteration count": IR converges with rate 1/κ per step; at κ ≈
  1e8, getting to 1e-9 needs ~30 steps each costing a full FP32 solve. That's
  3× FP64 cost — net loss.
