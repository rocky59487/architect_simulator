# R2 realtime-150k — Round 2: sub-stage timing reveals real bottleneck

> Date: 2026-06-21 04:00 (night-shift seg-3)
> Builds on Round 1 (`RESULTS_round1.md`). Round 1's "real bottleneck is backsub + recover"
> hypothesis was wrong on the order of magnitude. Round 2's sub-stage instrumentation found
> the actual bottleneck and the fix produced a **2.0× speedup**.

## What changed

`SnSession.h` now exposes `SnSessionTimings` and `SnSession::lastTimings()`. The
`#ifdef SN_SESSION_TIMING` block in `SnSession.cpp` populates per-stage timings; main lane
builds without the flag (default) and pay zero cost. `Research/R2_realtime_150k/build_r2.bat`
opts in via `/DSN_SESSION_TIMING=1`.

`r2_bench.cpp` now prints a per-repeat breakdown:
```
rhs (eq=el->addEquivalentNodalLoads loop, Kloop=sparse-K prescribed reduction, rest=the rest)
bsub (sn::solveSuper itself)
ir   (Neumaier IR; 0 unless irSteps>0)
scat (uf -> u global scatter)
spmv (K*u - F for reactions)
rec  (per-element recover; 0 in lazy mode)
tot  (end-to-end solveFrame)
```

## Round 2 measurement: 90k frame tower

### Before any fix (BASELINE, after lazy-recover only)

| stage | RECOVER mode | LAZY mode |
|---|---|---|
| **rhs**     | **88.8 ms** | **88.3 ms** |
| ↳ eq sub-stage | 2.5 ms | 2.6 ms |
| ↳ Kloop sub-stage | (unmeasured) | (unmeasured) |
| ↳ rest      | ~85 ms | ~85 ms |
| backsub  | 46.6 ms | 46.6 ms |
| scatter  | 0.2 ms | 0.2 ms |
| spmv     | 3.3 ms | 3.3 ms |
| **recover** | **21.4 ms** | 0 |
| **total** | **161 ms** | **139 ms** |

The `rest` of RHS — 85 ms — was the elephant in the room. Round 1's estimate of "recover
dominates" was off by 4×.

### Hypothesis chase

Looking at `SnSession.cpp:152-173` line-by-line, two suspects stood out:
1. `for (int c = 0; c < N; ++c) for (SpMat::InnerIterator it(S.K, c); it; ++it)` — runs
   O(nnz) per frame to apply prescribed-displacement contributions, even when **every
   prescribed value is 0** (the base-fixed-at-zero educational-game common case).
2. `model.nodeIndex(nl.node)` inside the nodal-load loop. `FrameModel.cpp:12` shows it's a
   linear O(N) scan. With 14,822 nodal loads × 15,067 nodes averaging 7,500 iter/lookup =
   **~110 million iterations per frame** of straight `id == query` checks.

Patch 1: skip sparse-K loop when `hasNonZeroPresc == false` (the column-iteration body's
work was `presc * K = 0 * K = 0` anyway — bit-equivalent skip).

Patch 2: SnSession lazily builds `std::unordered_map<NodeId, int>` on first solveFrame() and
re-uses it (fingerprint guard keeps it valid for the session lifetime).

### After both patches

| stage | RECOVER mode | LAZY mode |
|---|---|---|
| **rhs**     | **3.4 ms** | **3.6 ms** ← **24× drop** |
| ↳ eq sub-stage | 2.4 ms | 2.6 ms |
| ↳ Kloop sub-stage | 0.000 ms | 0.000 ms (fastpath confirmed) |
| ↳ rest      | 1.0 ms | 1.0 ms |
| backsub  | 46.4 ms | 48.1 ms |
| scatter  | 0.2 ms | 0.2 ms |
| spmv     | 3.1 ms | 3.3 ms |
| **recover** | **21.5 ms** | 0 |
| **total** | **75.8 ms** | **56.4 ms** |

### Speedups

solveFrame median:
- RECOVER: 151 → 74 ms (**2.0×**)
- LAZY:    134 → 56 ms (**2.4×**)

### Verdict update

| target | LAZY before R2 (ms) | LAZY after R2 (ms) | result |
|---|---|---|---|
| 60 fps (16.67ms) | 134 | 55.8 | **still FAIL** (-39 ms) |
| 30 fps (33.33ms) | 134 | 55.8 | **still FAIL** (-22 ms) |
| 100 ms interactive | 134 (-34) | 55.8 (**+44**) | **PASS** |

90k frame tower **with full force recovery in LAZY mode** clears the 100 ms interactive
target by a wide margin. 30 fps is 22 ms short — within reach if backsub can be pushed
from 47 ms → 25 ms (the mixed-precision-IR target). 60 fps remains a physics wall.

## Diagnostic correctness

The sub-stage timer found a 110-million-iter linear-scan we'd been paying for since the
self-built supernodal lane went live in 2026-06-15 (PROGRESS_R_supernodal.md), and the
fix is a 30-line patch that's bit-equivalent under every existing fixture. Round 1's
estimate was wrong because it assumed the obvious culprits (backsub, recover) without
measurement. **Lesson: when the per-stage budget is in question, measure first.**

The two F66 sub-checks verify the bit-equivalence of the slow path (prescribed != 0)
against a non-SnPrimary solveLoad oracle.

## Gates

* standalone F1-F66 ALL PASS (failures=0, F66 new)
* UE automation 57/57
* OpenSees PASS
* linear-deep-audit 104/104
* frame_cli round-trip ALL PASS
* v2_roundtrip ALL PASS (FRAMECORE_EXPECTED_ENGINE_VER=2.8.1)

## Scaling sweep (after the fix)

LAZY mode solveFrame median, mid-night:

| nf | Before R2 | After R2 fastpath | Speedup | 100 ms target |
|---|---|---|---|---|
| 90,402  | 134.0 ms | **55.8 ms**  | **2.4×** | PASS (+44.2) |
| 120,042 | 218.3 ms | **67.7 ms**  | **3.2×** | PASS (+32.3) |
| 163,296 | 389.2 ms | **108.0 ms** | **3.6×** | FAIL (-7.9, close) |

Speedup grows with size: the `nodeIndex` linear scan is O(loads × nodes) so larger
models save more time per call. At 160k the user-facing solveFrame is 3.6× faster
than the pre-R2 baseline, and only 8 ms short of the 100 ms interactive target.

The backsub stage (`sn::solveSuper`) is now the dominant term:
- 90k:  47 ms backsub (60% of 56 ms LAZY total)
- 120k: 56 ms backsub (83% of 68 ms)
- 160k: 91 ms backsub (84% of 108 ms)

So clearing 30 fps at 90k or 100 ms at 160k now requires attacking backsub itself.
The expected 1.7-2× mixed-precision IR (FP32 factor + FP32 backsub + FP64
Neumaier residual) would push:
- 90k:  47 → 25 ms backsub  ⇒  56 → 34 ms LAZY total  ⇒  30 fps (33 ms) **achievable**
- 160k: 91 → 50 ms backsub  ⇒  108 → 67 ms LAZY total ⇒  far below 100 ms interactive

## Open work for round 3

1. **Mixed-precision IR backsub** — see above. The IR cache structure is already there
   (`Ap/Ai/Ax` set by `SnSessionOptions::irSteps>0`), so the FP32 factor + FP32 backsub
   + FP64 Neumaier residual loop is the smallest possible delta.
2. **DONE — Propagate fastpath + nodeIdx cache to `solveLoad`** (commit c76692b). PDelta /
   Reanalysis / Modal initial-step now ride the same speedup.
3. **120k / 160k revisit** — DONE; included in the table above.
4. **120k / 160k under v2.9** to publish the post-R2 baseline for future regression.
