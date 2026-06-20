# FrameCore v2.7 — Live dyn_collapse streaming + mid-run cancel

Date: 2026-06-20
Base release tag: `v2.6` at `770b23c`
Scope: a single feature — `solve.dyn_collapse` now pushes each `(u, v)` frame
to the client **while** `runDynamicCollapse()` is still running, and honours a
mid-run cancel. This closes the only P1 item v2.6 explicitly deferred (P1-3 in
RELEASE_v2.6.md). FrameCore engine API gains two opt-in callbacks; existing
callers (every standalone fixture, the v1 CLI, the UE F-tests, every v2.6
client) are unaffected because they do not set them.

## Landed

### Engine (`FrameCore`)
- `DynCollapseOptions` gains two `std::function` callbacks (Public API
  addition, source-compatible):
  - `onFrameEmitted(const DynCollapseFrame&)` — called inside `storeFrame`
    immediately after `H.frames.push_back`. The frame reference is the just-
    stored one (still in `H.frames`), so the callback can either copy or take
    a snapshot. Runs synchronously on the integrator thread.
  - `isCancelled()` — polled once every `frameStride` steps inside the main
    Newmark loop. Returning `true` terminates the run with
    `H.outcome = Invalid`, `H.diagnostic = "cancelled by caller"`, and
    `H.frames` containing whatever was captured before cancel.
- Both default to empty `std::function`; an empty function is checked with the
  `if (opts.onFrameEmitted)` / `if (opts.isCancelled)` pattern, so the hot path
  for callers who do not opt in is a single branch on a null function pointer
  — measurably free vs v2.6.

### Dispatcher (`v2`)
- `Dispatcher::Emit(Frame)` is now public — a thin wrapper around the existing
  `EnqueueOutbound`. Handlers running a long analysis call `Emit` to push
  intermediate event frames onto the outbound queue mid-run. The dispatcher's
  recv-side worker drains the queue independently of `Submit`, so the client
  sees events in real time.
- `HandleDynCollapse` now wires the engine callbacks:
  - `opts.onFrameEmitted` packs each frame into a `dyn_collapse.frame` event
    (binary or JSON payload depending on `binaryFrames`) and calls
    `d.Emit(...)`. The v2.6 post-run frame for-loop is gone — frames now
    arrive while the engine runs, not after.
  - `opts.isCancelled` returns `true` if either the dispatcher's own
    `IsCancelled(reqId)` flips (client sent `cancel`) **or** the per-call
    `abortReason` shared_ptr got set by the frame callback on a NaN
    sanity-check fail. The NaN check moves from a post-run sweep into the
    per-frame path so an early-blowup aborts the integrator immediately
    instead of finishing 10 000 steps of garbage.
  - Final response: returns `NON_FINITE_RESULT` if `abortReason` was set,
    `CANCELLED` if the dispatcher cancel flag is set, otherwise the existing
    final summary (`outcome / nFrames / nEvents / events / endTime / ...`).
- Capability `dyn_collapse.live` advertised in `hello.capabilities`. Without
  it, the client should expect frames in a burst after the final response
  (v2.6 behaviour).

### Verification
- `Tools/v2_roundtrip.py` gains
  `P1-3: solve.dyn_collapse live emits frames during run (live count matches
  final nFrames)` — opens a fresh session with non-zero density, sends
  `solve.dyn_collapse` with `streamFrames=true / binaryFrames=false`,
  collects all incoming events until the final response, and asserts that the
  live event count matches `body.nFrames` (so frames really arrived before the
  response, not as a post-run burst).
- `wanted` capabilities set in the smoke now includes `dyn_collapse.live`.

## Verification result

Five-leg gate on 2026-06-20:

| Leg | Status |
|---|---|
| [1/5] standalone FrameCore (F1..F64) | ALL PASS, 0 failures |
| [2/5] UE headless automation (`FrameCore.*`) | 57 / 57, exit 0 |
| [3/5] OpenSees offline cross-validation | PASS |
| [4/5] linear-analysis deep audit | PASS, 104 / 104 checks |
| [5/5] CLI round-trip (`frame_cli` J1) | ALL PASS, 0 failures |

v2-specific smoke: `Tools/v2_roundtrip.py` ALL PASS, including the new live
streaming fixture — observed `live_frames=6` matching `nFrames=6` on the
non-zero-density cantilever scenario.

## Breaking changes

**None.**
- `abi_version = 2` unchanged.
- `frame_capi_v2.h` wire framing, JSON schema shapes, ABI signatures are
  source-compatible with v2.6.
- The two new `DynCollapseOptions` callbacks are opt-in fields with default
  empty `std::function`; v2.6 callers do not need to be recompiled and behave
  identically.
- The dispatcher behaviour change for `solve.dyn_collapse` is observable on
  the wire: with `streamFrames=true` (the v2.6 default), v2.6 emitted all
  frames after the final response; v2.7 emits each frame as the engine
  produces it and the final response arrives last. Clients that consumed
  frames in any order are unaffected; clients that explicitly waited for the
  final response before reading frame events benefit from earlier feedback.

## Still deferred (post-v2.7)

- **B5.2 ReSolveSession session-cache routing** — `analysis.reanalysis_solve`
  returns the expected shape but rebuilds the ReSolveSession per call;
  session-scoped caching is the remaining work.
- **model.patch** — schema unsettled.
- **C-09 / C-10 widening** — opening a transient LDLT side-system on a
  supernodal session for modal / buckling rather than refusing. Ergonomics,
  not correctness.
- **Live event streaming** — v2.7 streams **frames** live; `dyn_collapse.event`
  frames (topology change events) still come post-run. The engine accumulates
  into `H.events` and would need a second engine callback channel; out of v2.7
  scope.
- **C# `dotnet build` verification** — still requires an environment with the
  .NET SDK installed.

## Workspace notes

- `Plugins/LevelSim/Binaries/Win64/UnrealEditor.modules` (copied in v2.6 from
  `../ArchSim-levelsim` during the swap-thrash event) is **still in place**.
  UE incremental build for v2.7 picked it up unchanged; LevelSimPlay.dll was
  not rebuilt because no plugin source changed.
- UE incremental build for the v2.7 FrameCore engine changes took 1 h 57 m
  cold-cache on this 31 GB-RAM machine (peak swap thrash during
  `Module.FrameCore.cpp` unity-TU recompile, predictable from
  `[Adaptive Build] Excluded from FrameCore unity file: DynamicCollapse.cpp`).
  The build *did* finish — patience is enough; no manifest copy needed this
  cycle.
