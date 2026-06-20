# FrameCore v2.8.1 — Audit-hardening release (7-agent adversarial pass on v2.6 + v2.7)

**Tag:** `v2.8.1`
**Branch:** `main`
**Date:** 2026-06-20
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v2.7` at `448c25f`

## 1. Why this release exists

The maintainer explicitly stated low confidence in `v2.6` and `v2.7` and asked for a
**release-hardening audit pass** before further engine work. v2.8.1 is exactly that pass:
seven specialised auditors ran in parallel against the v2.5 → v2.7 source / docs / wire
delta, every finding was triaged, oracle-backed small fixes (<30 LOC each) landed, and
the docs that were lying about state got resynced. The FrameCore engine algorithm is
unchanged; the engine .cpp delta is 12 additive lines of NaN-sentinel guard.

## 2. Audit-driven fixes (cross-referenced to finding IDs)

### 2.1 4-agent confirmed CRITICAL: engine version drift

`kEngineVer` had been frozen at `"2.5.0"` through v2.6 **and** v2.7. Any client calling
`frame_v2_engine_version()` or reading `hello.response.version` was told `"2.5.0"` for
two full releases. Independently flagged by **Agent A (A-01) / Agent B (B-04) / Agent E
(E-03) / Agent F (F-01)** — four parallel auditors converging on the same finding is the
highest-confidence signal in this audit framework.

- `Plugins/FrameSolver/Standalone/v2/Dispatcher.h:75`: `kEngineVer` → `"2.8.1"`. Inline
  comment cross-references all four audit IDs and the wire-visibility impact.
- `Plugins/FrameSolver/FrameSolver.uplugin:4`: `VersionName` → `"2.8.1"` (B-05 / E-04).

The `abi_version = 2` integer is unchanged; only the human-facing engine string moved.

### 2.2 Engine-side NaN sentinel (A-02 / A-03 / A-04)

`runDynamicCollapse` Newmark integrator did **not** check `uN` / `vN` for finiteness
inside its `storeFrame` lambda. v2.7's `onFrameEmitted` dispatcher callback caught NaN
on the wire side, but any standalone caller (the F-fixtures, the linear deep audit, any
direct C++ user of `runDynamicCollapse`) silently received a `DynCollapseHistory` whose
frames contained NaN values, and whose `outcome` was eventually overwritten by the
loop-end branch (`H.outcome = MaxSteps; H.diagnostic = "time horizon reached"`) at
line ~473. Engine-side, no diagnostic, no flag.

`DynamicCollapse.cpp` storeFrame lambda now does `isFinite.all()` on both vectors before
push_back, sets `H.outcome = Invalid` + `H.diagnostic = "non-finite state in
DynamicCollapse Newmark integrator"`, raises a local `nanAbort` flag, and the three
`storeFrame` call sites (initial t=0 frame, in-loop frameStride snapshot, post-event
inheritance snapshot) all check the flag and bail before the MaxSteps overwrite runs.

A-04's secondary observation — the post-event `storeFrame` at line ~473 was not polling
`isCancelled` — is partially addressed (NaN bail-out is now there; cancel poll is on the
v2.9 first-action list, single 4-line patch).

### 2.3 Dispatcher inbound queue cap (C-02)

`frame_v2_send` parses-and-queues unconditionally. The worker thread is blocked inside
`Dispatcher::Submit(HandleDynCollapse)` for the entire Newmark integrator duration —
potentially many seconds. A fast client (a Grasshopper slider drag, an automated test
spamming session.open + solve.linear pairs, an SDK retry loop on a transient error)
can pile up hundreds or thousands of inbound frames during that window with zero
backpressure. C-02 cap: 256 frames in the inbound deque. Anything beyond returns
`FRAME_V2_OUT_OF_MEMORY` with a clear `lastError` string. 256 is comfortably above any
realistic burst (~10 frames during a continuous slider drag) and well below per-process
heap / thread exhaustion. `frame_capi_v2.cpp:160`.

### 2.4 Dead-field trio removed (C-11)

`Dispatcher::lastError_` (`Dispatcher.h:297`), its companion mutex `errMtx_`
(`Dispatcher.h:296`), and the getter `Dispatcher::LastError()` (`Dispatcher.cpp:187`)
were all declared but never written. `grep 'lastError_ =' Dispatcher.cpp` → 0 hits.
`frame_v2_last_error` (`frame_capi_v2.cpp:229`) reads only the per-context
`ctx->lastError` string under the ctx mutex (the documented A-05 / F-06 fix from v2.6
verified correct in this audit), it never falls through to `Dispatcher::LastError()`.
The entire trio was a maintenance hazard: any future contributor adding dispatcher-side
error tracking would have wired into `lastError_` and silently never surfaced through
the C ABI. Removed (10 lines, declaration + definition + comment).

### 2.5 CApiV2Transport.DisposeAsync UAF (D-10b)

`CApiV2Transport.DisposeAsync` called `_closeDelegate?.Invoke(_ctx)` and then
`NativeLibrary.Free(_libHandle)` **without first** waking any thread potentially blocked
inside `ReceiveFrameAsync` (which sits in native `frame_v2_recv(blockingMs: -1)`). On a
Rhino session running long polls, a Dispose racing with an in-flight recv would Free the
DLL while the recv thread still held `_ctx` as a P/Invoke argument — classic interop
UAF (or, more commonly, an unwind through a freed delegate trampoline producing a
non-reproducible crash on plugin teardown).

Fix at `CApiV2Transport.cs:166`: `_cancelRecvDelegate?.Invoke(_ctx)` before
`_closeDelegate?.Invoke(_ctx)`. `frame_v2_cancel_recv` is non-blocking (sets a flag +
`cv.notify_all()` under the ctx mutex), so the recv thread wakes up and unwinds via
`FRAME_V2_INVALID_CTX` on its own. Once the recv path has stopped touching `_ctx`, Free
is safe.

### 2.6 Docs / release-hygiene fixes

| ID | File:line | Fix |
|---|---|---|
| E-01 / E-02 | `docs/HANDOFF_v2.8.1.md` (new) | v2.6 and v2.7 shipped without HANDOFF docs (skill rule violation). v2.8.1 HANDOFF covers all three cycles' deferred items with first-action sketches. |
| E-05 | `README.md:16-34` | Status block had been frozen at v2.5; rewritten to v2.8.1 truth, including the F-fixture clarification (B-01) and the audit cross-reference. |
| E-06 | `E:\project\CLAUDE.md:13` (not in git) | Project-level CLAUDE.md still said "現況(2026-06-19, tag v2.4 + v2.5 build-up 7 commits)"; updated for future sessions. |
| E-07 | `docs/RELEASE_v2.5.md:4` + `docs/VERIFICATION.md:62` | Two dead-link sites cited `POST_V2_5_HARDENING.md`, a file that was never authored. Both now cite the real `RELEASE_v2.6.md` / `v2.7` / `v2.8.1` chain. |
| E-09 / G-01 | `docs/RELEASE_v2.6.md:145-152` | Workspace-notes section leaked the hardcoded `E:\project\v2-audit` absolute path and referenced a `stash@{0}` that has since been dropped. In-place errata: content moved to `HANDOFF_v2.8.1.md §6 workspace state` with portable wording. (Same in-place errata model as commit 770b23c's edit to `RELEASE_v2.5.md` from the v2.6 cycle.) |
| E-10 | `docs/README.md:58-71` | Rhino bridge index said "B3-B7 待辦(見 HANDOFF_v2.4)" — four releases stale. Five RELEASE/HANDOFF docs were missing from the index entirely. Section rewritten with current B1-B7 phase status and full release/handoff list. |
| B-01 | `README.md:23` + `docs/HANDOFF_v2.8.1.md §1` + this file | "F1..F64" claim implies 64 fixtures; the real `main.cpp` ships 62 individual cases — F41 and F60 are intentionally absent (historical numbering preserved). Docs now read "62 individual F-fixtures spanning F1..F64". The F41/F60 backfill is a v2.9 deferred decision item. |
| G-02 | `environment.yml` (new) | Conda env recipe deferred since the v2.5 audit (G-08/G-09 in the historical chain). 74-line `environment.yml` exported from the live `framecore-direct` env (`conda env export --no-builds`), with hand-edited tail pinning OpenSeesPy / numpy / scipy upper bounds and stripping the machine-local `prefix:` line. |

### 2.7 Acknowledged-but-not-fixed (deferred to v2.9)

- **G-07 wire-order observable change** — v2.7 `streamFrames=true` semantics moved
  `dyn_collapse.frame` events from post-final-response to mid-run. RELEASE_v2.7.md
  declared "Breaking changes: None"; for clients that buffered all frames after the
  final response, that is misleading. v2.8.1 acknowledges it; the canonical fix is
  `gh release edit v2.7` to add a "Client-visible wire-order change" paragraph
  (without retagging) — deferred so v2.7 stays unchanged on disk.
- **G-06 RELEASE Repo URL** — `RELEASE_v2.6.md` and `RELEASE_v2.7.md` both omit the
  Repo URL line that v2.3/v2.4/v2.5 had. v2.8.1 includes it; v2.6/v2.7 will be patched
  via `gh release edit` in the v2.9 cycle.
- **G-04 / G-05 env-var docs** — `SUPERNODAL_CONDA` / `FRAMECORE_LIB_DIR` overrides
  exist in `build_capi_v2.bat:23` and `Tools/v2_roundtrip.py:50` but are not in any
  README Setup section. v2.8.1 documents them in `environment.yml` comments and
  `HANDOFF_v2.8.1.md §2`; README Setup section is a v2.9 micro-task.
- **F41 / F60 fixture backfill** — defer the policy decision (backfill new cases or
  keep gap with explicit comment) to v2.9.
- **B5.2 ReSolveSession session-cache routing**, **model.patch schema**, **C-09/C-10
  widening**, **live `dyn_collapse.event` channel**, **C# dotnet build CI** — carried
  over from v2.6/v2.7 deferred lists, first-action sketches in HANDOFF_v2.8.1 §4.

## 3. Verification

Five-leg gate green on 2026-06-20, post-fix:

| Leg | Status | Reproduce |
|---|---|---|
| [1/5] standalone FrameCore | ALL PASS, 0 failures | `Plugins\FrameSolver\Standalone\build.bat` (62 individual F-fixtures spanning F1..F64; F41 and F60 intentionally absent) |
| [2/5] UE headless automation | 57 / 57, exit 0 | `Scripts\run_gate.ps1 -RequireOpenSees` (no UE source delta from v2.7) |
| [3/5] OpenSees offline cross-validation | PASS | `python Tools\opensees_compare.py` in active `framecore-direct` env |
| [4/5] linear-analysis deep audit | PASS, **104** independent checks | `Plugins\FrameSolver\Standalone\build_linear_audit.bat && linear_deep_audit.exe` (runtime checks=104 confirmed; Agent B's static-count 108 included declaration + early-exit branches) |
| [5/5] CLI round-trip | ALL PASS, 0 failures | `python Tools\cli_roundtrip.py` |

Plus optional 6th leg (v2 dispatcher round-trip), covering every audit-relevant
fixture:

- `python Tools\v2_roundtrip.py` — ALL PASS, 0 SKIP, 0 FAIL. Includes:
  - `kEngineVer` advertised on `hello` (auto-asserts non-empty; v2.8.1 audit B-04 follow-up will
    upgrade this to a literal-match assertion in v2.9 to gate future drift)
  - `transport.async` advertised, `transport.sync` absent (v2.6 wire change)
  - `dyn_collapse.live` advertised + live_frames == nFrames (v2.7 P1-3 fixture)
  - C-09 / C-10 supernodal-session modal/buckling reject
  - native D/C (`solve.linear wantDC=true`) shape match
  - duplicate-id reject (C-05)
  - cancel tombstone consume-on-match

## 4. Breaking changes

**Public ABI: None.** `abi_version = 2` unchanged.

**Client-visible behavior changes:**

- `frame_v2_engine_version()` now returns `"2.8.1"` (was `"2.5.0"` through v2.6 + v2.7).
  Any client that hard-coded an equality check against `"2.5.0"` now sees a mismatch.
  Recommended: use semver-style version-range matching, not equality.
- `frame_v2_send` may now return `FRAME_V2_OUT_OF_MEMORY` when the inbound queue is at
  256 frames; v2.6 / v2.7 never returned this code on queue depth (only on `bad_alloc`).
  Recommended: client retry-with-backoff on this error code.
- `runDynamicCollapse` (engine API; not over the wire) may now return earlier with
  `outcome = Invalid` and `diagnostic = "non-finite state in DynamicCollapse Newmark
  integrator"` instead of running to completion with NaN-laden frames. Pre-v2.8.1
  callers that ignored `outcome` or treated `MaxSteps` as success will now see the
  early failure surface correctly.

## 5. Release asset

`framecore-v2.8.1-win64.zip` — `frame_capi.dll`, `frame_capi_v2.dll`, `frame_cli.exe`,
`frametest.exe`, plus `LICENSE` / `NOTICE`. Same packaging convention as v2.6 / v2.7.

## 6. Tag plan

```bash
# Stage explicit files (CLAUDE.md project rule: never -A / .)
git add Plugins/FrameSolver/Standalone/v2/Dispatcher.h \
        Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp \
        Plugins/FrameSolver/Standalone/frame_capi_v2.cpp \
        Plugins/FrameSolver/Source/FrameCore/Private/DynamicCollapse.cpp \
        Plugins/FrameSolver/FrameSolver.uplugin \
        Plugins/FrameSolver/Grasshopper/v2/Bridge/CApiV2Transport.cs \
        docs/RELEASE_v2.8.1.md docs/HANDOFF_v2.8.1.md docs/RELEASE_v2.5.md \
        docs/VERIFICATION.md docs/RELEASE_v2.6.md docs/README.md README.md \
        environment.yml

git commit -m "..."
git tag -a v2.8.1 -m "FrameCore v2.8.1 -- 7-agent audit pass on v2.6/v2.7"
git push origin main
git push origin v2.8.1
gh release create v2.8.1 --title "FrameCore v2.8.1 -- audit-hardening" \
                  --notes-file docs/RELEASE_v2.8.1.md \
                  framecore-v2.8.1-win64.zip
```
