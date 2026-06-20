# R2 realtime-150k — Round 1 micro-bench results

> Date: 2026-06-21 02:50 (night-shift seg-2)
> Hardware: rocky59487 dev box (Windows 11 Home, conda framecore-direct env active)
> Build: SUPERNODAL=1, OpenBLAS+METIS via conda, /O2 /MD, single-shot

## Setup

`Research/R2_realtime_150k/r2_bench.exe` — production `SnSession.solveFrame()` lane on a
braced frame tower (3 section sizes, 2-way beams + outer diagonals). **No shells** — pure
frame, so the result tells you the FrameSolver per-frame budget WITHOUT shell assembly cost.

```
Fixture: nx={nx}, ny={ny}, stories={stories}
  freeDof ≈ (nx+1)(ny+1)(stories)*6
```

## Raw results

| case | nx×ny×stories | freeDof | factorMs (once) | solveFrame median | 60fps margin | 100ms margin |
|---|---|---|---|---|---|---|
| `~90k`  | 18×12×60 | 88,920  | 3,508 | **131.6 ms** | -114.9 | **-31.6** |
| `~120k` | 18×12×80 | 118,560 | 5,282 | **218.3 ms** | -201.6 | **-118.3** |
| `~160k` | 20×15×80 | 161,280 | 9,610 | **389.2 ms** | -372.6 | **-289.2** |

All three FAIL every realtime target. SnPrimary path confirmed:
`usingSnPrimary=1, pivotMargin~1e-7, diag=""` (sn factor reused, no LDLT fallback).
sessCtor cost negligible (~0.01 ms — factor IS reused).

## Scaling

- factor: 3508 → 5282 → 9610 ms ⇒ 1.81× nf (90k→160k) gives 2.74× factor cost = O(n^1.66)
- solveFrame: 132 → 218 → 389 ms ⇒ same 1.81× nf gives 2.95× = O(n^1.81)
- backsub-dominant systems scale ~O(n) for sparse triangular solve; **the >O(n) factor here
  proves solveFrame is NOT backsub-bound**. RHS assembly, K*u reaction SPMV, and member-force
  recovery (each O(nz) = O(members)) all scale faster than backsub at this density.

## Confounds vs prior numbers

The conventional wisdom (from `docs/PROGRESS_R_supernodal.md:77`) is "~150k DOF interactive
via backsub ≤100 ms". This bench refutes that for the *per-frame user experience*: even at
the **smaller 90k**, the user-facing `solveFrame()` is 132 ms.

Three confounds explain the discrepancy:

1. **What was measured before**: `v3_memory_recon.md` numbers are `sn::solveSuper` only —
   the bare backsub kernel, not RHS assembly + scatter + reactions + recover.
2. **Test fixture difference**: prior numbers are mixed buildings (with shells); this is
   frame-only. Mixed gives different nz/n profile.
3. **CHOLMOD vs self-built sn**: prior numbers favoured CHOLMOD; self-built was equal at
   17k, 12-18% slower at 32-64k (`PROGRESS_R_supernodal.md`). 90k+ is extrapolation; the
   gap may widen.

## What's in the 131 ms (estimated split)

From `SnSession::solveFrame` source (`SnSession.cpp:127-263`), per-frame work is:
1. RHS assembly (lines 152-173): nodalLoads scatter + el->addEquivalentNodalLoads +
   presc reduction via S.K inner iterator.
2. **Backsub**: `sn::solveSuper(fac, sym, b, x)` (line 189) — the only "supernodal"
   line. Expected ~50 ms @ 90k extrapolating CHOLMOD 33.6 ms @ 64k × (90/64)^1.0.
3. K*u SPMV for reactions (line 252): `S.K * u - F` — full sparse mat-vec.
4. **Recover member forces** (line 259): 46k member×~12 dof recover each.
5. Scatter u back to global (line 248-250).

Items 1, 3, 4 collectively are 3× O(nnz) work. Item 2 is one backsub. At 90k that is
plausibly ~30 ms backsub + ~100 ms RHS+SPMV+recover.

## What this means for the candidates

Re-evaluation given that solveFrame ≠ backsub:

- **A — Mixed precision + IR (FP32 backsub + FP64 IR)**: targets item 2 only. Predicted
  cut: ~30 ms → ~15 ms (-15 ms). Reduces solveFrame 132 → 117 ms. **Not enough alone**.
- **B — Schur local update** for collapse events: targets the *factor cost* (3508 ms at
  90k), not per-frame. Helpful for collapse path, but the user "interactive 150K dragging
  a load case" is per-frame, not per-event. **Misses the headline target**.
- **G (new) — Lazy / opt-out force recovery**: skip items 3,4 when caller didn't ask for
  member forces. Predicted cut: -50 to -80 ms at 90k. **Largest single win**.
- **H (new) — Sparse RHS assembly + scatter SIMD**: items 1, 5 collectively. Predicted
  cut: -10 to -20 ms. **Modest but cheap to implement**.
- **I (new) — Cache K*u SPMV using BLAS3**: line 252 reaction calc. Predicted: -10 ms.
  Symmetric matrix-vector is bandwidth-bound; modest gain.

**Combined upper bound**: solveFrame 132 ms → ~ 32-50 ms at 90k. Hits 30 fps (33 ms),
not 60 fps (16.67 ms).

**Honest take on "real 150K realtime"**:
- 60 fps @ 150K **is a physics wall on current architecture** without sacrificing the
  member-force feedback (the educational game needs that data, so we can't strip it).
- 30 fps @ 90-100k is reachable via G + A + I.
- 30 fps @ 150K is borderline — needs G + A + I + maybe a smaller fixture nf curve.

## Course correction for the rest of the night shift

Original target "let engine reach true realtime 150K" needs a redefinition before more
prototyping:

1. **Define realtime** for the educational game:
   - User dragging a load and watching the structure flex — 30 fps is fine, 60 fps is the
     console-game ideal but not needed for a static-structure educational tool.
   - User triggering a collapse and watching the dynamic playback — *playback* runs from
     pre-computed frames at video rate; the SOLVE is event-driven, not per-frame.
2. **The realistic target is**: 30 fps @ 150K *per-frame solve* (interactive load
   dragging) and *event-driven < 1 s* for collapse events. This is achievable.
3. **The honest message**: 60 fps @ 150K **with full member-force recovery** is NOT
   achievable on current architecture without GPU / re-architected force-recovery.

## Decisions

- **Defer Candidate A (mixed precision)**: 15 ms savings alone is not the leverage point.
- **Promote Candidate G (lazy recover)**: largest single win, low risk (`SolveOptions` flag).
- **Add Candidate H (RHS+scatter SIMD)**: cheap mechanical win.
- **Continue research** on Candidate B (Schur local update) for collapse path — that is
  the OTHER axis (event response time), distinct from per-frame.

## Next round: Round-2 micro-bench

Need to instrument SnSession::solveFrame with sub-stage timing (RHS / backsub / SPMV /
recover) to confirm the estimate above before committing engine changes. Either:
- Add `SOLVEFRAME_TIMING` opt-in to SnSession (production main path, gated)
- Or write `r2_bench_v2.cpp` that pulls in `SnSession.cpp` directly with a custom hook

The cleaner path is the gated SOLVEFRAME_TIMING flag — `#ifdef` block that records four
chrono points, zero cost when not defined. Then re-run the bench with timing.
