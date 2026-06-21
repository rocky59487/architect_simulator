# v2.11 24-hour development plan (2026-06-21 09:30 → 2026-06-22 09:00)

> User asked for "continue until 09:00 tomorrow, with a plan that pushes BOTH new
> technology AND existing technology to the limit." This document is the contract.
> Each phase has a hard hour budget; if a phase overruns, the plan is rewritten,
> not extended into the next phase.

## OUTCOME (added by v2.11.1 release-hardening, 2026-06-21)

The 24-hour plan finished in ~5 hours wall-clock — the front-loaded GPU
phases compounded faster than budgeted. The actual landings:

| Phase | Planned | Actual | Outcome |
|---|---|---|---|
| 1: GPU SPMV reactions (cuSPARSE) | 11:00-14:30 | `d467a28` ~11:00 | **PASS** — 60 fps @ 90 k via 3-4× SpMV speedup |
| 2: CUDA stream + async memcpy | 14:30-17:00 | `5064194` ~11:24 | **PASS** — 60 fps @ 200 K cleared (bsub -2 ms) |
| 3: OpenMP parallel RHS | 17:00-21:00 | `f1b9ba8` (opt-in flag) | **PASS as opt-in** — won't default-on; common-case Qf=0 fixture has nothing to parallelise |
| 3': Qf-skip cache (added mid-plan) | n/a | `d633040` ~10:25 | **PASS** — 60 fps @ 150 K cleared via 5-6 ms reclaim |
| 4: cuDSS PHASE_REFACTORIZATION | 21:00-00:30 (Round 4) | `4429f72` (post-tag research) | **NEGATIVE on frame-tower** (1.04-1.07× only); P-Delta fixture not measured — see `HANDOFF_v2.11.0.md §item 2` |
| 5: Reorder benchmark | 00:30-04:00 | not started | **DEFERRED to v2.12** — factor cost is now small fraction of per-frame budget; leverage low |
| 6: Stress curve through VRAM ceiling | 04:00-08:00 (subset of release) | included in scaling table | **PASS** — interactive ≤100 ms through 600 K |
| 7: UE Build.cs FRAMECORE_CUDA + DLL preload + UE F67 mirror | (not in original plan) | `792b810` ~11:55 (post-tag) + `3d2c559` ~12:10 | **PASS** — Phase 7 above-and-beyond, integrated into v2.11.1 |
| 8: release v2.11.0 | 04:00-08:00 | `3ae1cad` ~11:45 | **PASS** + v2.11.1 release-hardening on top |

**Net assessment.** Plan-超出: ~2 hr 完成 plan 24 hr 全部主軸 + extra Phase 7
UE wire-up. Round-4 cuDSS PHASE_REFACTORIZATION was the only NEGATIVE; the
methodology limitation (uniform vs P-Delta non-uniform K_sigma update) leaves
the question 50% open for v2.12. v2.11.1 release-hardening folded the 5
post-tag commits + closed 6 BLOCKER/HIGH findings from a 7-agent adversarial
audit (see `RELEASE_v2.11.1.md` for the full list).

## Standing constraints (carried from the earlier night-shift)

- **Default-off**. The 5-leg main gate (standalone F1-F66 + UE 57 + OpenSees + audit
  104 + CLI) must stay green at every commit. Production callers who haven't
  changed config must see bit-identical behaviour.
- **Bit-equivalent oracles**. Each new code path needs an F-fixture that compares
  it to the existing CPU reference at rel < 1e-8.
- **Honest negatives**. Round 3 showed that not every promising idea works. Each
  phase ends with PASS / NEGATIVE / DEFERRED — no inflated claims.
- **Commit cadence**. One commit per phase (or per logical sub-step). No
  "WIP" commits, no `--no-verify` skips, no `git add -A`.

## The two axes

### Axis A — push the GPU lane harder (new tech)

Already shipped (`v2.10.0`): cuDSS backsub via SnSession + F67 + 6th gate leg.

| Phase | Target | Expected ROI |
|------:|---|---|
| 1 | GPU SPMV reactions via cuSPARSE | 160K LAZY+GPU 25.6 → ~18.6 ms (closes 60 fps at 150K) |
| 2 | CUDA streams pipeline + memory pool | backsub stage 3.2 → 1-2 ms (closes 60 fps at 200 K) |
| 4 | cuDSS PHASE_REFACTORIZATION | P-Delta refactor 5s → 0.5s class (interactive collapse playback) |
| 6 | 300K / 400K / 600K stress | establish ceiling on RTX 5070 Ti's 12 GB VRAM |
| 7 | UE Build.cs `bCudaEnabled` | UE editor + packaged game also run GPU lane |

### Axis B — squeeze the CPU lane to the floor (existing tech)

| Phase | Target | Expected ROI |
|------:|---|---|
| 3 | OpenMP RHS assembly parallel (R9 8940HX 16C/32T) | RHS 3.5 ms → 0.8 ms (helps both CPU-only and GPU paths) |
| 5 | Reorder benchmark (METIS vs SCOTCH vs Eigen AMD) | 1.2-1.5× factor speedup if SCOTCH wins; opt-in if so |

### Why these and not others

- **AVX-512 dense panel kernels**: ruled out — implementation risk too high for 24 h
  window; OpenBLAS already vectorised; would only help one-shot factor not per-frame.
- **Multi-RHS batched solve**: not relevant for the interactive-load-drag UX
  (one RHS per frame).
- **Mixed precision FP32**: already NEGATIVE on building stiffness κ. Don't retry.
- **GPU RHS scatter**: nodalLoads loop is already small (~0.5 ms after the
  nodeIdx cache); GPU scatter buys back maybe 0.3 ms minus the upload overhead.
  Not worth the integration cost. Stays on CPU.

## Phase schedule

| Phase | Window | Budget | Description |
|------:|--------|-------:|---|
| 0  | 09:28-09:45 | 17 min | This plan, committed |
| 1  | 09:45-12:00 | 2.25 h | GPU SPMV reactions (cuSPARSE SpMV reusing cuDSS K) |
| 2  | 12:00-14:00 | 2.0 h  | CUDA streams pipeline + persistent device buffers |
| 3  | 14:00-15:30 | 1.5 h  | OpenMP RHS assembly parallel |
| 4  | 15:30-17:30 | 2.0 h  | cuDSS PHASE_REFACTORIZATION (P-Delta path) |
| 5  | 17:30-19:30 | 2.0 h  | Reorder bench (METIS / SCOTCH / AMD), opt-in if PASS |
| 6  | 19:30-21:00 | 1.5 h  | 300K/400K/600K stress + scaling-curve update |
| 7  | 21:00-00:30 | 3.5 h  | UE FrameCore Build.cs `bCudaEnabled` + UE F67 test |
|    | 00:30-04:00 | 3.5 h  | Buffer / hard-problem recovery / second-pass on missed targets |
| 8  | 04:00-08:00 | 4.0 h  | release v2.11.0: RELEASE notes / HANDOFF / tag / GH release / docs / MEMORY |
|    | 08:00-09:00 | 1.0 h  | Final gates + sanity + spare |

Each phase's first 30 minutes are "design + measure expectation" (so we'd notice if
the actual ROI is far from the planned one). The remaining time goes into
implementation, fixture, gate run, commit, push.

## Phase 1 detail: GPU SPMV reactions

Goal: `reactions = S.K * u - F` currently runs in CPU (Eigen sparse mat-vec on
the full N x N stiffness). Measured at 7 ms / frame at 160 k DOF — the second-
largest stage in `SnSession::solveFrame` after backsub itself.

The cuDSS lane already uploads the K_ff (free-DOF) CSR to device at SnSession
ctor. We *do not* have S.K (full) on device, so the GPU SPMV needs a separate
upload, OR we accept that reactions stays CPU and target a different stage.

Actually wait: `S.K` is the FULL N x N matrix (Eigen SparseMatrix<double>),
including fixed DOF columns. `reactions = S.K * u - F` is N-element output;
the only DOFs with non-trivial reactions are the fixed ones (free DOFs get
reactions ≈ F by static equilibrium).

Two design choices:

A. Upload S.K to device at ctor (full N x N), do SpMV on device. Extra 30-50%
   memory cost vs current cuDSS K_ff upload. But this lets us reuse the upload
   across frames (one-shot).
B. Compute `r_fixed = K_fc.T * u_free + K_cc * u_fixed - F_fixed` and only the
   fixed-DOF block. Smaller upload, smaller SpMV. But needs `K_fc / K_cc` extraction.

Choice A is simpler and the integration matches the existing cuDSS K upload
pattern. Go with A. cuSPARSE generic SpMV API (cusparseSpMV) handles CSR + dense
vec. Reuse the `cudssHandle` cudaStream.

Expected savings:
- 160 k cuSPARSE SpMV vs CPU Eigen SpMV: target ratio ~5-10× (limited by GPU
  memory bandwidth, not compute). Predicted: 7 ms → 1-1.5 ms.
- 90 k: 4 ms → 0.6 ms.
- 200 k: 9 ms → 2 ms.

Verdict thresholds:
- PASS: GPU SPMV < 50% of CPU SPMV at 90 k AND residual matches CPU to 1e-12.
- DEFERRED: 50-100% (no clear win, integration cost not paid).
- NEGATIVE: GPU SPMV > CPU SPMV → kept on CPU, document why.

Fixture F68: cantilever cuDSS session, compare `R.reactions` between CPU SPMV
path (FRAMECORE_CUDA=0 build) and GPU SPMV path (CUDA build), require rel < 1e-12.

## Phase 2 detail: CUDA streams pipeline

Goal: today's `SnSession::solveFrame` per-frame GPU path is synchronous:

```
cudaMemcpy(d_b, b, n*sizeof(double), HostToDevice)     // ~50 µs at 90 k
cudssExecute(PHASE_SOLVE, ...)                          // ~2 ms at 90 k
cudaMemcpy(x, d_x, n*sizeof(double), DeviceToHost)      // ~50 µs
cudaDeviceSynchronize()                                  // implicit barrier
```

Total measured: 2.5 ms median. With pipelining:

- Frame t: SOLVE on stream A while frame t+1's RHS is being prepared on CPU
- Frame t-1's downloaded x is already in host memory when frame t commits

This buys back the upload + download (~100 µs total) per frame; small but
measurable. The bigger win is from `cudaMallocAsync` + memory pool eliminating
per-frame allocator overhead.

Risk: cuDSS PHASE_SOLVE is not documented as stream-safe across concurrent
invocations on the same data. Will measure single-stream cudaMallocAsync first;
multi-stream pipelining behind a separate flag if it works.

Fixture F69: stream-aware path passes bit-equivalent vs sync path on F68 model.

## Phase 3 detail: OpenMP RHS assembly

Goal: `for (const auto& el : S.elems) el->addEquivalentNodalLoads(F)` runs
~3.5 ms at 160 k (RhsEq sub-stage from SnSessionTimings). 16 cores → upper
bound 16× speedup; realistic 4-6× (load imbalance, shared F write).

Pattern:

```cpp
const int nThreads = std::min(8, omp_get_max_threads());  // pick by problem size
std::vector<VecX> Fpriv(nThreads, VecX::Zero(N));
#pragma omp parallel num_threads(nThreads)
{
    const int tid = omp_get_thread_num();
    VecX& Floc = Fpriv[tid];
    #pragma omp for schedule(dynamic, 64)
    for (size_t e = 0; e < S.elems.size(); ++e) S.elems[e]->addEquivalentNodalLoads(Floc);
}
for (int t = 0; t < nThreads; ++t) F += Fpriv[t];
```

Risk: `IElement::addEquivalentNodalLoads(F)` may not be thread-safe if any element
mutates element-side state. Must audit.

Fixture F70: OpenMP RHS path bit-equivalent vs serial path (rel = 0 exactly,
because the underlying math is the same).

## Phase 4 detail: cuDSS PHASE_REFACTORIZATION

Use case: P-Delta analysis incrementally updates K with the geometric stiffness
K_sigma(P). The non-zero pattern of K + K_sigma stays the same — only values
change. cuDSS PHASE_REFACTORIZATION takes a new value array and rebuilds the
numeric factor faster than the full FACTORIZATION because it skips the
symbolic + ordering phases (which we cached at session ctor).

Predicted: full FACTORIZATION at 160 k = 1007 ms; REFACTORIZATION estimate
200-300 ms (NVIDIA reports 3-5× speedup vs full re-factor). For P-Delta this
turns the 10-iteration refactor loop from 10 s into 2-3 s.

Fixture: add `--mode pdelta` to gpu_bench2.cpp; compare K+εK_sigma full
factor vs refactor at the same residual.

## Phase 5 detail: Reorder benchmark

Conda has `python-scotch` and `scotch` packages. The hypothesis:
- METIS multilevel nested dissection has been tuned for SPD systems but on
  building stiffness matrices (sparsity comes from element connectivity, not
  random) it may not be optimal.
- SCOTCH's k-way partitioning sometimes wins on structured meshes.
- Eigen's built-in COLAMD ordering is the safety net.

We're not going to switch the default — METIS stays. But if SCOTCH wins by
> 20% on factor cost across all three regimes (90k, 160k, 200k), add a
`SnSessionOptions::reorder` enum and opt-in.

## Phase 6 detail: 300K / 400K / 600K stress

Today the largest fixture is `--preset 200k` (211 k DOF). On RTX 5070 Ti
laptop 12 GB VRAM, cuDSS factor at 200 k peaked around 1.5 GB (estimated).
Higher DOFs will hit two walls:
- VRAM: 12 GB ÷ (FP64 factor + workspace) ~ 600-800 k upper bound.
- Per-frame GPU SOLVE time scales O(n log n) ish; 600 k expected 12-15 ms.

Will produce a new RESULTS_round4 with the scaling curve through whichever
size hits "OUT_OF_MEMORY" first.

## Phase 7 detail: UE Build.cs `bCudaEnabled`

Today UE 5.7's FrameCore.dll is CPU-only (default build). The UE side has
two challenges:

1. cuDSS DLLs (cudss64_0 + cudart64_12 + cusparse64_12 + cublas64_12 + ...)
   need to be available at UE runtime. Options:
   a. Copy them into `Plugins/FrameSolver/Binaries/Win64/` (huge — ~600 MB).
   b. Use UE's third-party DLL packaging system (RuntimeDependencies + 
      `PublicDelayLoadDLLs`).
   c. Require conda framecore-direct env on PATH (developer-only).
2. UE packaging for end-users needs to bundle the same DLLs. Educational game
   ships on student laptops — likely no conda env, so option (a) or (b)
   becomes mandatory.

v2.11 ships (a) with the understanding that v2.12 will move to (b). The
UE F67 test imitates the standalone F67 and runs under the 5-leg gate.

## Phase 8 detail: release v2.11.0

Pulls together:
- RELEASE_v2.11.md — explicit numbers for each phase's improvement
- HANDOFF_v2.11.0.md — v2.12 pickup brief
- CLAUDE.md / MEMORY / NIGHT_SHIFT — same as before
- v2.11.0 tag + GitHub release with binary bundle (now includes CUDA DLLs)
- gh release create with full notes

## Risk register

| Risk | Mitigation |
|---|---|
| cuSPARSE SpMV slower than CPU on this size | Phase 1 verdict thresholds — accept NEGATIVE if so |
| CUDA streams aren't safe inside cuDSS | Single-stream memory pool as fallback |
| `IElement::addEquivalentNodalLoads` not thread-safe | Audit before parallelising; if not, pre-build per-element F contributions and merge serially |
| cuDSS REFACTORIZATION not stable across small numeric changes | Verify residual after K_sigma += εI;  refuse + fall back to full factor if blowup |
| UE link of cuDSS fails (ABI / linker mismatch) | Document failure, ship UE CUDA disabled by default; investigate v2.12 |
| 600K hits VRAM ceiling | Use that ceiling as the documented limit; not a failure |

## What "extreme" means here

For the GPU lane we want:
- 60 fps comfortably at 200 K (not just 200 K barely PASS).
- < 5 ms per-frame at 90 k (room for game-side work).
- One-shot factor < 1 s at 200 K.

For the CPU lane we want:
- 30 fps at 90 k LAZY (today is 56 ms → need < 33 ms; Phase 3 + 5 deliver).
- Memory ≤ 8 GB at 200 K factor (the conda OpenBLAS path; nothing to do).

If Phase 1-3 land on target, the **60 fps @ 150 K production** goal becomes
**60 fps @ 200 K production**. That's the headline for v2.11.

## After v2.11

If we somehow finish early, the bench bar moves to:
- 600 K + 1 M DOF with `SnPrimary` + cuDSS (multi-GPU, model partitioning)
- Eigenvalue analysis on GPU (modal, buckling)
- Adaptive remeshing for the educational-game UX (level of detail)

These are explicitly v2.12+, listed here only as a forward-looking pointer.

## Sleep budget

None. Continuous work to 09:00. If a phase needs offloading to a subagent,
spawn one. If two phases are independent (e.g. Phase 3 RHS + Phase 5 reorder),
run them in parallel via subagents.
