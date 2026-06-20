# R2 Round 3 plan — mixed-precision IR backsub (v2.10 candidate)

> Status: PLAN ONLY — no code yet. Written 2026-06-21 07:55 at the end of the
> v2.9.0 night-shift to brief whoever picks this up next.

## Why now

After the R2 RHS fastpath + nodeIdx cache landed (commits `cdfaeeb` and
`c76692b` in v2.9.0), `SnSession::solveFrame` LAZY mode is dominated by the
backsub itself:

| nf      | LAZY total | backsub  | rest   |
|---------|-----------:|---------:|-------:|
| 90 k    | 55.8 ms    | 46.6 ms (83 %) | 9.2 ms |
| 120 k   | 67.7 ms    | 56.3 ms (83 %) | 11.4 ms |
| 160 k   | 108.0 ms   | 91.4 ms (85 %) | 16.6 ms |

50-repeat / 30-repeat stress runs show the same picture (90 k LAZY median 58.7 ms,
120 k 75.7 ms). The next leverage point is the supernodal forward/back sub itself.

## Target

| target | gap @ 90 k | gap @ 160 k |
|---|---:|---:|
| 30 fps (33 ms) | -22 ms LAZY | -75 ms LAZY |
| 100 ms interactive | already PASS | -8 ms LAZY |

A 1.7-2.0× backsub speedup clears 30 fps @ 90 k (47 → 24 ms backsub ⇒ LAZY
~33 ms) and ≤ 100 ms at 160 k (91 → 50 ms ⇒ LAZY ~67 ms). 60 fps remains a
physics wall on a single CPU thread.

## Approach — FP32 backsub + FP64 Neumaier IR

The standard recipe (LAPACK xPOSRFS, ICL/MAGMA mixed-precision SPD reports):

1. Factor stays FP64 (one-shot at session ctor, cost amortised).
2. Maintain an FP32 mirror of `sn::SnSuper::values` at session ctor.
3. Backsub runs FP32 forward / FP32 back substitution → FP32 candidate `x`.
4. Promote to FP64; compute residual `r = b - K * x` in FP64 using the cached
   `Ap/Ai/Ax` CSC arrays (already populated when `irSteps > 0`).
5. Solve correction `d = K \ r` again in FP32, x += d. Repeat 1-2 times or
   until `‖r‖_∞ ≤ irTol * ‖b‖_∞`.

Per Higham (*Accuracy and Stability* §15.5), FP32-residual + FP64-IR converges
for SPD systems when `κ(A) < 1/u_FP32 ≈ 8 × 10⁶`. For typical building
stiffness (well-conditioned beams + shells, κ ≈ 10⁴-10⁵), this is comfortable.

## Risks

- **Condition number under shell + beam-release mixes**: κ can spike past
  10⁶ when slender shells join stiff beams with end releases. The fallback
  has to be FP64 backsub on residual blow-up.
- **FP32 supernodal kernel write**: `sn_chol.h` is FP64-only today. The patch
  has to duplicate the dense panel kernels with float casts, or template them
  on a scalar type. The latter is cleaner but more invasive.
- **Memory**: FP32 factor mirror doubles the cache footprint (90 k tower factor
  is ~5 MB → 7.5 MB total).
- **OpenBLAS interaction**: `cblas_sgemm` / `cblas_strsm` are the FP32 calls;
  thread-count rules from M3b (`recommendedThreads`) still apply.

## Step-by-step implementation order (for v2.10)

### Step 1 — Verify FP32 SPD safety on the actual fixtures (30 min)

Before touching the engine, run a numerical experiment:

```cpp
// In Research/R2_realtime_150k/fp32_safety_check.cpp:
// Build the 90k frame tower model (same as r2_bench).
// Assemble K_ff in Eigen FP64 (existing path).
// Convert K_ff -> Eigen::SparseMatrix<float>.
// Eigen::SimplicialLDLT<SparseMatrix<float>> ldlt32; ldlt32.compute(K32);
// Solve, then compare residual ‖K64 * cast(x32) - b‖ / ‖b‖.
// PASS gate: rel residual ≤ 1e-3 after 0 IR steps.
//            rel residual ≤ 1e-9 after 1 IR step.
// FAIL: drop the plan, the condition number is over the FP32 wall.
```

### Step 2 — sn_chol.h scalar templating (90 min)

`sn::SnSuper::values` becomes `std::vector<Scalar>` (templated), with explicit
instantiations for `float` and `double`. The dense panel kernels (`syrk`,
`gemm`, `trsm` calls) follow. Keep both instantiations linked.

### Step 3 — SnSession FP32 mirror + IR loop (60 min)

`SnSessionOptions::mixedPrecisionBacksub` (default false). On ctor, when true,
populate a parallel `sn::SnSuper<float>` from the FP64 factor. `solveFrame`'s
backsub path routes through F32 then runs Neumaier IR on the FP64 `Ap/Ai/Ax`.
The `irSteps` option already exists from v2.7 and gates the residual loop;
just reuse it with default 2 in mixed mode.

### Step 4 — F67 oracle fixture (30 min)

```cpp
// F67: mixed-precision backsub equivalence
// Build the F58 patch-test fixture (high cond, ~ 1e6).
// Solve with mixedPrecisionBacksub=false (FP64 baseline) -> u_ref.
// Solve with mixedPrecisionBacksub=true, irSteps=2 -> u_mp.
// Assert rel(u_mp - u_ref) < 5e-9 (typical IR residual ceiling on the cached fixture).
// Assert IR converged in <= 2 steps (otherwise drop to FP64 fallback in code).
```

### Step 5 — Bench + RESULTS_round3.md

Run the same 90k / 120k / 160k comparison as round 2. Expected:

| nf | LAZY FP64 | LAZY mixed (target) |
|---|---:|---:|
| 90 k | 56 ms | 33 ms |
| 120 k | 68 ms | 41 ms |
| 160 k | 108 ms | 65 ms |

If the actual numbers come in within 20 % of target, ship as v2.10. If less,
write a negative result and keep FP64 as default.

### Step 6 — Wire ABI

`SnSessionOptions::mixedPrecisionBacksub` is engine-side only at first.
Exposing on the v2 wire (body field + capability `mixed_precision_backsub`)
is a follow-up once production callers actually want it.

## What NOT to do

- Don't write a separate `solveLoadFP32` lane. Keep the routing inside
  `SnSession` so the same factor-reuse contract holds.
- Don't try GPU offload as a fallback. CANDIDATES.md ruled it out: sparse
  triangular solve has poor GPU parallelism and tying the engine to D3D12
  ComputeShader / CUDA breaks the educational-game cross-platform deal.
- Don't attempt FP16. The 10⁴-10⁵ κ range is already on the edge for FP32;
  FP16 would lose the IR convergence guarantee.

## Open questions

1. Should the FP32 mirror be lazy-built (on first `solveFrame`) or eager (at
   ctor)? Lazy avoids cost when `mixedPrecisionBacksub` is set but the user
   never calls `solveFrame`; eager keeps `solveFrame` predictable.
2. Should IR convergence be silent (return whatever x we got) or hard
   (`SolveResult::singular = true` on failure)? Default-hard is safer; the
   educational game would prefer a clear error.
3. Should the v2 wire body advertise mixed-precision as a per-session option
   or per-call? Per-session matches `SnSessionOptions`; per-call would let a
   client switch dynamically, but adds session-state complexity.
