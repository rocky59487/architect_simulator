# FrameCore v2.9.0 — Realtime perf breakthrough + dyn_collapse.event live channel

**Tag:** `v2.9.0`
**Branch:** `main`
**Date:** 2026-06-21
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v2.8.1` at `69c16e0`

## 1. Headline

The user-facing `SnSession::solveFrame()` time on a 90k-DOF frame tower dropped from
**134 ms to 55.8 ms (2.4×)** in LAZY mode, and the 160k tower dropped from
**389 ms to 108 ms (3.6×)**, putting both inside the 100 ms interactive target. The
fix was *not* in the linear-algebra core — it was two simple-but-unmeasured
inefficiencies that the v2.9 sub-stage instrumentation surfaced.

This release also closes the v2.6 deferred `dyn_collapse.event` live-streaming channel:
the dispatcher now pushes each event to the client the moment `runDynamicCollapse`
appends it to `H.events`, the same way v2.7 added live frame streaming.

## 2. R2 realtime-150k research lane (under `Research/R2_realtime_150k/`)

A new research lane spun up to chase "true realtime 150K" — the goal was 60 fps at
150k DOF. Round 1 estimated based on the existing literature; Round 2 instrumented
sub-stage timings and found the actual bottleneck was the RHS-assembly pass, not the
backsub or recover passes everyone assumed.

### 2.1 Sub-stage instrumentation (opt-in)

`SnSession.h` gained `SnSessionTimings` (rhs/rhsEq/rhsKloop/backsub/ir/scatter/spmv/
recover/total) and `SnSession::lastTimings()`. The default build sees the all-zero
stub: `#ifdef SN_SESSION_TIMING` gates the `chrono::now()` pairs so the production
hot path stays clean. `Research/R2_realtime_150k/build_r2.bat` opts in via
`-DSN_SESSION_TIMING=1`. The accessor is always-declared so callers don't have to
conditionally compile against the engine.

### 2.2 Patch G — lazy force recovery (`SnSessionOptions::skipForceRecovery`)

When the caller only needs `u` + `reactions` (an educational-game interactive load
drag, for example), `solveFrame` can skip the per-element recover pass that writes
`R.memberForces` / `R.shellForces`. Default `false` keeps every existing caller
bit-identical. On a 90k frame tower the saving is ~21 ms (~13 % of solveFrame).

* `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/SnSession.h`: new option
  field at the trailing end (ABI additive).
* `Plugins/FrameSolver/Source/FrameCore/Private/SnSession.cpp`: gates the resize +
  per-element loop on `!opts.skipForceRecovery`; diagnostic carries `[lazy-recover]`.
* Standalone fixture F65 (8 checks) verifies `lazy.u == recover.u` bit-equivalence
  and the new size + diagnostic contracts.

### 2.3 Patches H + I — RHS fastpath + nodeIdx cache

Sub-stage timing revealed two unexpected costs in the RHS assembly:

1. **`FrameModel::nodeIndex` is O(N) linear scan.** The nodal-load loop called it
   per load — on the 90k tower that's 14,822 calls × 15,067 nodes ≈ 110 million
   `id == query` iterations per frame, costing ~74 ms.
2. **The sparse-K column iteration runs `O(nnz)` per frame regardless of whether
   any prescribed displacement is non-zero.** For the base-fixed-at-zero scenario
   (the educational-game common case), every `presc[c] == 0` so the loop body
   wrote `0 * K` to `Ff` — pure waste, ~12 ms at 90k.

The patches:

* `SnSession::Impl` now caches `std::unordered_map<NodeId, int>`, populated lazily
  on first `solveFrame()` and re-used (the fingerprint guard keeps it valid for the
  session lifetime).
* The sparse-K loop is gated on `hasNonZeroPresc` (computed in the same pass that
  fills `presc[]`). Bit-equivalent skip: when every prescribed value is zero the
  loop body did nothing.
* The same two patches were propagated to `FrameSolver::solveLoad` so every
  `PreparedSystem`-based caller (PDelta refactor, Reanalysis Tier-2/3, Modal
  initial step, …) rides the same speedup.

### 2.4 Measured speedup (LAZY mode, frame-tower fixture)

| nf      | Before R2 | After R2 fastpath | Speedup | 100 ms target |
|---------|----------:|------------------:|--------:|:--------------|
| 90,402  | 134.0 ms  | **55.8 ms**       | 2.4×    | PASS (+44.2)  |
| 120,042 | 218.3 ms  | **67.7 ms**       | 3.2×    | PASS (+32.3)  |
| 163,296 | 389.2 ms  | **108.0 ms**      | 3.6×    | -7.9 ms       |

Speedup grows with size: the `nodeIndex` linear scan is `O(loads × nodes)` so the
larger the model, the more time the cache saves. At 160 k DOF the user-facing
`solveFrame` is now 3.6× faster and only 8 ms short of interactive.

### 2.5 What's still out of reach

* **60 fps @ 150 k**: the backsub itself (`sn::solveSuper`) is now ~91 ms at 160 k
  and ~47 ms at 90 k. Pushing backsub below the 16.67 ms 60 fps budget is a hard
  physics wall on the current single-precision CPU lane.
* **30 fps @ 90 k**: 55.8 ms LAZY is 22 ms over the 33 ms 30 fps budget. The next
  attack vector is mixed-precision IR backsub (FP32 factor + FP32 backsub + FP64
  Neumaier residual); the `Ap/Ai/Ax` cache structure is already there from the
  v2.7 IR work, so the patch is bolt-on. Deferred to v2.10.

### 2.6 Fixtures

* **F65** (8 checks) — lazy force recovery: `lazy.u == recover.u (rel<1e-12)`,
  `lazy.reactions == recover.reactions`, `lazy.memberForces.empty()`,
  `lazy.shellForces.empty()`, diagnostic carries `[lazy-recover]`.
* **F66** (8 checks) — RHS fastpath + nodeIdx cache equivalence on prescribed != 0:
  a tip-prescribed-rotation cantilever (F45/F53b workhorse) solved through BOTH
  SnPrimary SnSession and the default LDLT `solveLoad` oracle, asserting
  `SnSession.u == LDLT.u (rel<1e-10)` and `SnSession.reactions == LDLT.reactions`,
  plus a second `solveFrame()` call to verify the nodeIdx cache survives reuse.

## 3. A4 — dyn_collapse.event live channel (closes v2.6 deferred)

`DynCollapseOptions::onEventEmitted` is the events-side mirror of v2.7's
`onFrameEmitted`. `runDynamicCollapse` invokes it every time it appends an event to
`H.events` (the initial scenario event, every per-step brittle-failure event, and
the post-fragment energy-after annotation), so a transport can push each event live
the moment it lands.

* The v2 dispatcher wires the callback into a `dyn_collapse.event` channel emit when
  the new body field `liveEvents` is true (default).
* `liveEvents=false` opts back into the v2.7 post-run loop (kept for backward
  compat); switching `streamEvents=false` opts out of events entirely.
* `hello.capabilities` advertises **`dyn_collapse.live.events`** (the existing
  `dyn_collapse.live` capability remains and refers to the frame channel).
* Wire ABI unchanged: same `packDynEvent` payload, same `dyn_collapse.event`
  channel name; only the emit time changed.

The same 6 `H.events.push_back(...)` sites in `DynamicCollapse.cpp` now flow through
a single `emitEvent` closure that calls `onEventEmitted(H.events.back())`. Default-
empty callback means every existing standalone caller (F-fixtures, frame_cli, v1
dispatcher) sees zero overhead.

### 3.1 Fixture (v2_roundtrip.py)

A new end-to-end check exercises a mass-bearing cantilever with `initialRemovals=[0]`
through the v2 dispatcher: the dispatcher must push at least one `dyn_collapse.event`
WHILE `runDynamicCollapse` is running, the count equal to the final summary's
`nEvents`. Measured: `live_events=1 nEvents=1 final=response`. A second check
verifies `dyn_collapse.live.events` is in the hello capability set.

## 4. v2.8.2 audit closeouts (also included in this release)

The night-shift's first commit (`a307a0c`) finished three v2.8.1-deferred audit
items before the perf work started:

* **A1** — `v2_roundtrip.py` engine_version upgraded from "non-empty" to "semver-ish"
  (`^\d+\.\d+(\.\d+)?$`) plus an optional literal pin via
  `FRAMECORE_EXPECTED_ENGINE_VER` env var. Catches the v2.5-era `kEngineVer` drift
  (4 audit agents flagged "2.5.0" frozen in `Dispatcher.h` after v2.8 tag) at
  gate-time, not at post-mortem-time.
* **A2** — `DynamicCollapse.cpp:475` adds an `isCancelled` poll right after the
  v2.8.1 (A-04) post-event NaN guard. Mirrors the inner-loop cancel poll on
  line 408 so a client cancelling DURING re-configuration wakes within one event
  boundary instead of one frameStride-of-Newmark later.
* **A5** — `Standalone/main.cpp` documents the F41 / F60 intentional-absent
  numbering policy in source (was only in `docs/HANDOFF_v2.8.1.md`).

## 5. Wire / ABI summary

* **Engine ABI**: additive. `SnSessionOptions::skipForceRecovery` and
  `DynCollapseOptions::onEventEmitted` are new trailing fields; existing callers
  compile unchanged. `SnSession::lastTimings()` is a new const method; the
  `SnSessionTimings` struct is new but all-zero unless the engine was built with
  `-DSN_SESSION_TIMING=1`.
* **Wire ABI**: additive. New body field `liveEvents` on `solve.dyn_collapse`
  (default true) and new capability `dyn_collapse.live.events`. Existing
  payloads (frame channel, event channel, response summary) unchanged.
* **`kEngineVer`** bumped 2.8.1 → 2.9.0; `FrameSolver.uplugin VersionName`
  bumped accordingly.

## 6. Gates (all-green)

* standalone F1-F66 ALL PASS (failures=0; F65 + F66 new in v2.9)
* UE automation 57 / 57
* OpenSees offline cross-validation PASS
* linear-deep-audit 104 / 104
* `frame_cli` round-trip ALL PASS
* `v2_roundtrip` ALL PASS (new R2.3 fixture + new capability check; engine_version
  semver-ish + env-pin check from A1)

## 7. Deferred to v2.10 / beyond

* **Mixed-precision IR backsub** (Candidate A from `Research/R2_realtime_150k/
  CANDIDATES.md`) — the next leverage point for 30 fps @ 90 k and ≤ 100 ms @ 200 k.
* **B1** — `ReSolveSession` cache routing inside the v2 dispatcher
  (`HandleReanalysis` still rebuilds the ladder per call).
* **B2** — `model.patch` schema + dispatcher wire (still NOT_IMPLEMENTED).
* **C-09 / C-10 widening** — analysis.modal / analysis.buckling on SnPrimary
  sessions still refuses with NOT_IMPLEMENTED; widening to a transient LDLT
  side-system is the next-decade scope.
* **A3** — `abortReason` micro-perf to `std::optional<std::string>` (not landed;
  shared_ptr is fine for now — the IR work showed lifetime sharing across two
  callbacks is the right pattern).

## 8. Research lane status

`Research/R2_realtime_150k/` remains untracked (consistent with `Research/WS_B_solver`
HpFEM lane that produced the supernodal direct lane), but `CANDIDATES.md`,
`RESULTS_round1.md`, `RESULTS_round2.md`, `r2_bench.cpp`, and `build_r2.bat` capture
the full reasoning trail for anyone picking the lane back up. The bench is
self-contained: `Research/R2_realtime_150k/build_r2.bat` + `r2_bench.exe --preset 90k
--compare --repeat 20`, with the conda `framecore-direct` env on PATH.
