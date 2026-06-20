# Handoff — v2.9.0 (post night-shift 2026-06-21)

> Where the engine sits, what's done, what's next, and the durable lessons.
> Same format as `HANDOFF_v2.8.1.md`. Read both — this one supplements, doesn't replace.

## TL;DR

- **HEAD on `main`**: `3ad6dd0` (night-shift final log) ahead of tag `v2.9.0` (`7873e39`).
- **Tag `v2.9.0`** ships R2 realtime perf breakthrough (2.4× @ 90 k / 3.6× @ 160 k on
  user-facing `SnSession::solveFrame`) + `dyn_collapse.event` live channel + R2 lazy
  force recovery + v2.8.2 audit closeouts.
- **All gates green**: standalone F1-F66, UE 57/57, OpenSees, deep audit 104/104,
  CLI round-trip, v2_roundtrip.
- **kEngineVer**: 2.9.0; **FrameSolver.uplugin Version**: 23, VersionName 2.9.0.

## What landed (the eight commits)

| # | hash | one-liner |
|---|---|---|
| 1 | `a307a0c` | v2.8.2 audit closeouts (A1 engine_version semver+pin / A2 post-event cancel poll / A5 F41-F60 policy comment) |
| 2 | `7b2dc65` | SnSession lazy force recovery (`skipForceRecovery` flag) + F65 fixture |
| 3 | `cdfaeeb` | SnSession RHS fastpath + nodeIdx cache: 2.4× speedup at 90 k (F66 fixture) |
| 4 | `c76692b` | Propagate same patches to `FrameSolver::solveLoad` |
| 5 | `22c4971` | A4 `dyn_collapse.event` live channel + `dyn_collapse.live.events` capability |
| 6 | `7873e39` | Release v2.9.0 — version bumps + RELEASE_v2.9.md + Research/R2 lane tracked |
| 7 | `3ad6dd0` | Night-shift final log update |

GitHub release: <https://github.com/rocky59487/architect_simulator/releases/tag/v2.9.0>

## What's still deferred to v2.10

### Highest leverage — Mixed-precision IR backsub

Round 2 sub-stage timing showed backsub now dominates the post-R2 budget:
- 90 k: 47 ms backsub / 56 ms LAZY total — 30 fps target needs ≥ 22 ms cut
- 160 k: 91 ms backsub / 108 ms LAZY total — 100 ms interactive needs ≥ 8 ms cut

The FP32 forward / FP32 backsub / FP64 Neumaier-compensated residual loop is the
standard recipe (LAPACK xPOSRFS pattern). The on-disk cache structure
(`SnSession::Impl::Ap/Ai/Ax`) is already there for the v2.7 IR work. The patch is:

1. Add an FP32 mirror of `sn::SnSuper::values` populated at session ctor when
   `SnSessionOptions::mixedPrecisionBacksub == true`.
2. New `sn::solveSuperF32(const SnSuper&, const SnSymbolic&, const double* b, double* x)`
   that runs the supernodal forward/back sub in FP32 with FP64 b/x staging.
3. SnSession::solveFrame: if `mixedPrecisionBacksub`, route through F32 path,
   then run 1-2 Neumaier IR steps on the existing `Ap/Ai/Ax` FP64 matrix.

Expected speedup: 1.7-2× backsub (per ICL/MAGMA mixed-precision SPD literature).
At 90 k LAZY this brings solveFrame to ~32-35 ms, **clearing 30 fps**.

Risk: condition number on mixed-shell + frame-release scenarios can push κ above
the FP32-safe threshold (≈ 8×10⁶). IR converges only when κ < 1/u_FP32. The
fallback is keep FP64 (default flag false; F64 fixture verifies bit-equivalence
or rejects to FP64 on residual blow-up).

Implementation order: prototype in `Research/R2_realtime_150k/` first (FP32 vs
FP64 Eigen SimplicialLDLT on the 90 k tower) to confirm κ stays safe, then port
to `sn_chol.h` proper.

### B1 — ReSolveSession dispatcher session-cache routing

`Dispatcher::HandleReanalysis` rebuilds `ReSolveSession rs(*sess->model, opts)`
on every call (Dispatcher.cpp:1331). The Woodbury → PCG → rebaseline ladder
should be cached in `SessionContext` keyed on model fingerprint. Risk: stale
cache after a structural change — fingerprint check before reuse, rebuild on
miss.

### B2 — `model.patch` schema + dispatcher wire

Still NOT_IMPLEMENTED in `Dispatcher.cpp::HandleModelPatch`. Needs:
- Schema in `docs/specs/S6d_model_patch.md` (add / remove / update verbs with
  key-by-id semantics).
- Dispatcher handler applies the patch to `sess->model`, re-fingerprints,
  invalidates the supernodal session if topology / supports changed.
- `Capabilities()` adds `"model.patch"`.
- `v2_roundtrip` fixture: patch add member → `solve.linear` → compare hand-
  computed displacement.

### C-09 / C-10 widening

`Dispatcher.cpp::HandleAnalysisModal` / `HandleAnalysisBuckling` refuse on a
SnPrimary session with NOT_IMPLEMENTED because the LDLT factor those analyses
need was never built. Widening: spin up a transient LDLT side-system on demand
(re-`assembleAndFactor(model, optDefault)` inside the handler, cache, run the
LDLT-bound analysis, return). Trade-off: a second factor cost on first call.

### A3 — `abortReason` micro-perf

`Dispatcher.cpp:1245` uses `std::shared_ptr<std::string>` for cross-callback
abort signalling. Switching to `std::optional<std::string>` with `&abortReason`
by-ref capture eliminates the heap alloc. Both callbacks live and die together
inside the dispatcher call so lifetime is safe. Micro perf — defer indefinitely.

## Engineering durable lessons (durable)

These came out of the night-shift; some belong in CLAUDE.md, some in the auto-memory:

1. **Per-stage budget questions get measured, not guessed.** Round 1 estimated
   force recovery was the dominant cost (50-80 ms estimate) and was wrong by 4×.
   The actual culprit (110 M-iter `nodeIndex` linear scan) was completely off
   the radar. Lesson lives in `Research/R2_realtime_150k/RESULTS_round2.md`.
2. **80 ms of "unknown rest" is never noise.** When the categorised stages sum
   to less than the measured total, find the gap — that's where the real
   bottleneck hides.
3. **Bit-equivalent skips are the safe form of optimisation.** Two patches in
   this release skipped O(nnz) work when its result would have been zero; both
   are functionally identical to the slow path and proved so by F66 against the
   LDLT oracle (`rel = 0` exactly).
4. **Research lanes can graduate into main.** PROGRESS_R_supernodal set the
   pattern; R2_realtime_150k follows it. Source + docs tracked, build artifacts
   .gitignored, lane not in build.bat / run_gate.ps1.
5. **60 fps @ 150 K is a physics wall on the current architecture.** Be
   honest about reachable targets (30 fps @ 90 K interactive, 100 ms @ 150 K
   interactive); a real-time educational game does not need 60 fps on a static
   load drag.
6. **UE incremental build is fast on hot cache.** ~40 s for an SnSession.cpp
   change; don't preempt to MEMORY's prior 1h57m swap-thrash worst-case unless
   you actually see CPU 0.4 % + RAM saturated.

## Reproducing the perf numbers

```powershell
# Assumes:
#   - E:\project\UE_5.7 with Eigen at Engine\Source\ThirdParty\Eigen
#   - conda env `framecore-direct` (OpenBLAS + METIS) on PATH
#   - VS 18 preview-aware vswhere

cd E:\project\ArchSim
Research\R2_realtime_150k\build_r2.bat
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;" + $env:PATH
Research\R2_realtime_150k\r2_bench.exe --preset 90k --compare --repeat 20 --warmup 3
Research\R2_realtime_150k\r2_bench.exe --preset 120k --compare --repeat 12
Research\R2_realtime_150k\r2_bench.exe --preset 150k --compare --repeat 8
```

Expected: 90 k LAZY median ≈ 55-60 ms; 120 k ≈ 65-70 ms; 160 k ≈ 105-115 ms.

## Verify the release

```powershell
cd E:\project\ArchSim
$env:FRAMECORE_EXPECTED_ENGINE_VER = "2.9.0"
.\Scripts\run_gate.ps1 -RequireOpenSees   # 5-leg gate
python .\Tools\v2_roundtrip.py            # 6th leg (v2 wire)
```

All must report ALL PASS. The version-pin env var catches kEngineVer drift; the
R2.3 fixture verifies live event streaming; the dyn_collapse.live.events
capability check verifies the hello handshake advertises the new channel.

## Open questions for v2.10

- Should `SnSessionOptions::skipForceRecovery` be exposed on the v2 wire?
  Currently engine-only; the dispatcher always recovers. A UE-side interactive
  drag could benefit but needs a body field + capability.
- Should the supernodal factor build itself parallelise across multiple
  sessions safely? The OpenBLAS process-global thread-count race is documented
  in `SnSession.h` but no fix has been attempted.
- The 30 fps @ 90 K LAZY target needs mixed-precision OR a much faster
  supernodal backsub kernel. The kernel is already at OpenBLAS-best on a
  single thread; another 2× would require either FP32 or GPU offload (which
  CANDIDATES.md ruled out).
