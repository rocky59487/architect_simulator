# FrameCore v2.6 — Async dispatcher + Grasshopper hardening

Date: 2026-06-20
Base release tag: `v2.5` at `a0491d6`
Scope: post-v2.5 hardening shipped as a tagged minor release. Engine-side
(FrameCore module, the 8-strand linear suite, S1–S10 main lines) is **unchanged
vs v2.5**; everything in this release lives in the v2 dispatcher transport, the
Grasshopper C# bridge, and the verification tooling.

## Landed

### Dispatcher transport (B4)
- `frame_capi_v2` transport is now asynchronous: `frame_v2_send` parses and
  queues, a per-context worker runs `Dispatcher::Submit`, and
  `hello.capabilities` advertises `transport.async` instead of `transport.sync`.
- `frame_v2_last_error` returns a `c_str()` to a thread-local copy taken under
  the per-context mutex, closing the A-05 / F-06 last-error race that v2.5 left
  deferred.

### Engine-backed handlers
- `solve.dyn_collapse` is wired to `frame::runDynamicCollapse`; it returns a
  final summary and can emit event/frame stream records, with frame payloads
  packed as `u_then_v_f64_le`.
- `analysis.reanalysis_solve` is wired to `frame::ReSolveSession` for
  same-topology member/shell active toggles, including tier/rank/PCG
  diagnostics.
- `solve.linear` accepts `wantDC: true` and returns native `memberUtilization`,
  `shellUtilization`, and aggregate `utilization` from `ElasticAllowable`.
- `solve.size_opt` keeps `finalAreas` / `finalDC` and adds compatibility fields
  `areas`, `sizeOptSingular`, and `weightVolume`.
- `ModelBuilder` rejects duplicate node/member/shell ids during JSON build with
  `VALIDATION_FAILED`.
- `analysis.modal` and `analysis.buckling` refuse a session opened with
  `mode=supernodal` with `NOT_IMPLEMENTED` (the modal / buckling subspace
  iterators need the LDLT primary, not the supernodal factor — silently
  building a parallel LDLT would defeat the supernodal factor-once contract
  and mis-tag `advancedDiagnostics`). Closes C-09 / C-10.

### Grasshopper C# bridge
- Serializes the native schema names: `ref`, `nodes`, `memberUDLs`, `Mp`.
- Parses native member/shell force object shapes.
- Parses nested `finalState` for tension-only results.
- Sends `Amin` for size optimization.
- Handles zero-size recv probes without allocating a zero-length fetch buffer
  (D-06).
- Returns `ValueTask.CompletedTask` from `CApiV2Transport.DisposeAsync` (D-10).
- Keeps `OpenFrameCoreComponent._opening = null` under `_openGate` (D-02).
- Marks `AssembleModelComponent._cachedFs` volatile (D-07).
- `UtilizationFringeComponent` no longer computes fake axial-only `|N| / 1.0`
  D/C. It uses native utilization data, otherwise emits `NaN` plus a warning
  (D-01).

### Tooling
- `.gitignore` now covers local agent/transcript artifacts: `.codex/`,
  `.cursor/`, `*.transcript`, `*.jsonl` (G-13).
- `build_capi_v2.bat` appends `-dirty` to `FRAMECORE_BUILD_SHA` when tracked
  files differ from `HEAD`, so local dirty DLLs do not masquerade as tag
  builds.
- `Tools/v2_roundtrip.py` covers: `transport.async` advertised,
  `transport.sync` absent, `solve.linear` bit-exact vs v1, native D/C output,
  duplicate id rejection, every v2.5 wired analysis, `reanalysis_solve` shape,
  `solve.dyn_collapse` final summary shape, cancel tombstone consume-on-match,
  and the new C-09 / C-10 supernodal+modal/buckling refuse fixtures.

## Verification

Five-leg gate passed on 2026-06-20:

| Leg | Status | Notes |
|---|---|---|
| [1/5] standalone FrameCore (F1..F64) | ALL PASS, 0 failures | `Plugins\FrameSolver\Standalone\build.bat` |
| [2/5] UE headless automation | 57 / 57, exit 0 | `FrameCore.*` |
| [3/5] OpenSees offline cross-validation | PASS | `Tools\opensees_compare.py` |
| [4/5] linear-analysis deep audit | PASS, 104 / 104 checks | `Plugins\FrameSolver\Standalone\build_linear_audit.bat` |
| [5/5] CLI round-trip (frame_cli J1) | ALL PASS, 0 failures | `Tools\cli_roundtrip.py` |

Plus v2-specific smoke:
- `cmd /c Plugins\FrameSolver\Standalone\build_capi_v2.bat` — OK (FRAMECORE_SUPERNODAL=1).
- `python Tools\v2_roundtrip.py` — ALL PASS (including the new C-09 / C-10
  fixtures).

## Breaking changes

**None.**
- `abi_version = 2` unchanged.
- `frame_capi_v2.h` signatures, wire framing, JSON schema shapes are
  source-compatible with v2.5.
- The only observable client-visible change: a session opened with
  `mode=supernodal` that subsequently calls `analysis.modal` /
  `analysis.buckling` now receives a `NOT_IMPLEMENTED` error frame instead of
  silently running on a parallel LDLT factor. v2.5 clients that relied on the
  silent fallback should split the supernodal session from the modal /
  buckling session (the LDLT path is still the default for either method when
  called on a session opened with the default mode).

## Deferred to v2.7 (with audit-ID traceability)

- **P1-3 (live `solve.dyn_collapse` streaming)** — currently streams recorded
  history frames after the engine run returns. True mid-integrator live
  progress/cancel polling is not advertised as a capability. Implementation
  sketch lives at `DynamicCollapse.cpp:363` (`storeFrame` lambda) — add an
  `onFrameEmitted` callback to `DynCollapseOptions`, poll `isCancelled` every
  `frameStride` steps, surface `EnqueueOutbound` as a public `Emit` on
  `Dispatcher`, advertise a new capability string.
- **B5.2** — `analysis.reanalysis_solve` shape returns today, but the
  long-running `ReSolveSession` ladder (Woodbury → stale-LDLT PCG → rebaseline)
  is not yet routed through a session-bound `ReSolveSession` cache; each call
  rebuilds. (Routing requires session lifetime ownership of the ladder, which
  is a follow-on to C-06 / C-07 worker-thread cycle.)
- **model.patch** — still registered but not advertised; schema unsettled.
- **C-09 / C-10 widening** — the refuse path is in place; a future cycle could
  let `analysis.modal` / `analysis.buckling` open a transient LDLT side-system
  on a supernodal session on-demand (still re-factors once) without dropping
  the supernodal factor for `solve.linear`. Optional ergonomics, not a
  correctness gap.

## Known follow-ups (operations / release engineering)

- **C# `dotnet build`** was not run in this environment because `dotnet --info`
  reports no installed SDKs. The project files target `net7.0` and keep
  `TreatWarningsAsErrors=true`; rerun on a machine with the .NET SDK and
  Rhino/Yak package access.
- **GitHub release binary assets** — v2.4 and v2.5 ship with zero assets. This
  v2.6 release publishes `frame_capi.dll`, `frame_capi_v2.dll`,
  `frame_cli.exe`, `frametest.exe`, plus `LICENSE` / `NOTICE` as a zip.

## Workspace notes (non-shipping context for future sessions)

These do not affect what is published, but are recorded so the next session
does not chase ghosts:

- **`Plugins/LevelSim/Binaries/Win64/UnrealEditor.modules` was copied from the
  sibling worktree `../ArchSim-levelsim`** during the gate run, after the
  in-tree UE rebuild stalled on `Module.FrameCore.cpp` unity TU (cl.exe held
  ~1.6 GB RSS at 1 % CPU for 1.5 h, with system pagefile at 47 GB — a swap
  thrash, not a compile failure). The copied manifest's `BuildId` (`47537391`)
  matches the in-tree `FrameSolver/Binaries/Win64/UnrealEditor.modules`, and
  `Plugins/FrameSolver/Binaries/Win64/UnrealEditor-FrameCore.dll` (`Jun 18`)
  is the v2.5-release-time build of an unchanged `FrameCore` engine
  (working-tree changes are dispatcher + C# + tooling only — FrameCore engine
  `.cpp` files are byte-identical to v2.5). The five-leg gate is green on
  exactly this configuration. A clean UE rebuild on a machine with adequate
  RAM headroom will regenerate the manifest in-place; no source change
  required.
- **(v2.8.1 audit E-09 / G-01: machine-local absolute path and ephemeral git
  state moved to `HANDOFF_v2.8.1.md` §workspace-state where they belong.
  Originally this bullet leaked an `E:\project\…` absolute path into the
  published release notes, and named a `stash@{0}` reference that has since
  been dropped.)**
