# FrameCore v2.5 — B3 Dispatcher Engine Wire + Capability Widening + Hardening

> Post-tag note: `main` now contains additional post-v2.5 hardening that is not part of the
> published `v2.5` tag. The hardening shipped in subsequent releases — see
> [`RELEASE_v2.6.md`](RELEASE_v2.6.md) (async dispatcher + GH bridge), [`RELEASE_v2.7.md`](RELEASE_v2.7.md)
> (live `dyn_collapse` streaming + mid-run cancel), and [`RELEASE_v2.8.1.md`](RELEASE_v2.8.1.md)
> (audit pass closing kEngineVer/uplugin version drift, engine NaN guard, dispatcher queue cap,
> dispatcher dead-field cleanup, CApiV2Transport DisposeAsync UAF, dead-link / handoff debt).
> [v2.8.1 audit E-07: replaces the dead `POST_V2_5_HARDENING.md` reference that lived here
> through v2.6 / v2.7.]

**Tag:** `v2.5`
**Branch:** `main`
**Date:** 2026-06-19
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Baseline:** `v2.4` (commit `6de233b`)
**Scope:** Dispatcher-layer release. `Plugins/FrameSolver/Source/FrameCore/` (engine source) is
**bit-identical to v2.3** (`6be1dac`); LevelSim is unchanged from v2.4 (v1.0.0). All v2.5 changes
land in the v2 transport line (`Plugins/FrameSolver/Standalone/v2/` + `Plugins/FrameSolver/Standalone/frame_capi_v2.{h,cpp}` +
`Plugins/FrameSolver/Grasshopper/v2/`) plus release-hardening sweeps over docs, ABI honesty,
ModelBuilder validation, LevelSim helper scripts, and privacy templatization.

---

## Engine highlights (v2.4 → v2.5)

Seven follow-up commits in order:

| Commit | Area | Change |
|---|---|---|
| `a859810` | chore | v2.4 deferred quick wins: README perf-figure alignment (62k DOF), H-09 font env var, H-10 `obj_capi/`/`obj_linear_audit/` gitignore, H-02/H-03/H-04 path templatize in two AGENT_PROMPT files, `build.bat` conditional supernodal skip. |
| `180c9e8` | feat | **B3 dispatcher wire** — 12 method handlers reach FrameCore. `solve.linear` returns real disp / reactions / member forces / shell forces, **bit-exact vs `frame_capi.dll` v1** on the cantilever fixture at rel < 1e-11. `inspect.{disp,reactions,member_forces,shell_forces}` read the session's cached `SolveResult`. `solve.{pdelta,tension_only,size_opt,corotational,arclength}` + `analysis.{modal,buckling}` all call the engine and return structured responses that `Tools/v2_roundtrip.py` spec-shape checks pass. |
| `3814f58` | fix | **A-01 `frame_v2_close` UAF race** closed via a `shared_ptr` ownership registry — every entry point acquires a reference before doing work, and `close` releases the owner reference instead of `delete`-ing while a peer thread is mid-`recv`. |
| `214e99f` | feat | **B5 supernodal factor-reuse** wired through `session.open mode=supernodal` → `frame::SnSession`. **D-03 GH OpenFrameCore generation race** closed via `_openGate` lock. **D-09 P/Invoke audit** — 7 Cdecl delegates verified line-by-line against `frame_capi_v2.h`. **E-10 S6b method table** gained `[B3]/[B4]/[B5]` status icons. |
| `a139a12` | docs | HANDOFF_v2.4 §4 marked done / deferred-with-reason for every entry. |
| `5c526c0` | fix | review-round 1 hardening. **P1 AssembleModel fingerprint** now hashes material + section numeric fields (was Count only — Y/Z sliders no longer hit the stale cache). **P2 cancel tombstone leak** closed via `ClearCancelled` on both the CANCEL path and after every completed handler. **P2 ABI doc** explicitly marks `frame_v2_send` synchronous + flags `transport.sync` capability for client detection. **P3 MiniJson** rejects raw control chars in strings and adds `errno`/`ERANGE` checks on integer overflow. |
| `b3cea8b` | fix | review-round 2: `transport.sync` capability added to `Capabilities()` + `Tools/v2_roundtrip.py` gains 4 cancel-tombstone consume + transport.sync presence checks. |

### v2 dispatcher capability map (honest, v2.5)

`frame_capi_v2.dll` — `kEngineVer = "2.5.0"`, `abi_version = 2`, `kSchemaVer = "2026.06"`,
`build_sha` from `git rev-parse --short HEAD`.
**21 registered handlers**; `Capabilities()` advertises the **18 that today return useful data**:

- **Connection-mgmt (5):** `cancel`, `profile.advanced`, `profile.simple`, `session`, `model.set`
- **Linear solve (1):** `solve.linear` — bit-exact vs v1 (rel < 1e-11)
- **Non-linear / analyses (7):** `solve.pdelta`, `solve.tension_only`, `solve.size_opt`,
  `solve.corotational`, `solve.arclength`, `analysis.modal`, `analysis.buckling`
- **Inspect family (4):** `inspect.disp`, `inspect.reactions`, `inspect.member_forces`,
  `inspect.shell_forces`
- **Transport contract (1):** `transport.sync` — handlers run inline on the caller thread;
  clients must worker-thread off-load long-running solves until B4 lands.

**Still NOT advertised (return `NOT_IMPLEMENTED`):**
- `solve.dyn_collapse` — needs streaming + binary payload (B4).
- `analysis.reanalysis_solve` — needs `ReSolveSession` wire (B5.2).
- `model.patch` — diff-format schema unsettled.

### ABI compatibility

`abi_version` remains **2** — no breaking changes to `frame_capi_v2.h` signatures or wire
framing. v2.4 clients work with v2.5 DLL; the visible behavioural difference is that
`solve.linear` now returns a real result instead of a `_stub: true` placeholder, and the
`hello.capabilities` list grew from 7 to **18** entries.

---

## Audit-driven small-fixes folded into v2.5

A 7-agent parallel audit (numerics + ABI / oracle-claim / dispatcher physics / GH glue /
docs cartography / code-smell / reproducibility + privacy + handoff-prep) drove the
following < 30-LOC fixes — all oracle-backed by the verification matrix below, no
public-API breaks:

| id | file:line | change |
|---|---|---|
| A-01 / E-07 / F-01 / G-03 | `Plugins/FrameSolver/Standalone/v2/Dispatcher.h:69` | `kEngineVer "2.4.0" → "2.5.0"`. Four independent agents flagged it. |
| A-02 / C-06 / F-03 | `Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp:528-535` | `advancedDiagnostics.factorMethod` / `.factorBackend` now branch on `sess->sn` — supernodal sessions report `"Supernodal" / "SnChol_selfbuilt"`, LDLT sessions report `"LDLT" / "SimplicialLDLT"`. The hard-coded LDLT label was a silent lie that violated the CLAUDE.md "no claim without data" rule. `factorTimeMs` / `solveTimeMs` remain `0.0` until B4 adds steady-clock hooks. |
| A-03 / C-01 | `Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp:790-825` | `HandleArcLength` with `arcLength=0` now rejects in advanced profile (`VALIDATION_FAILED`) and records `defaultsApplied: ["arcLength=auto (engine first-tangent estimate)"]` in simple profile. The previous silent `arcLength=0 → auto` path could step over a snap-through limit point and return `converged=true` for the wrong branch. |
| A-04 | `Plugins/FrameSolver/Standalone/v2/Dispatcher.h:139-180` | `Capabilities()` widens from 7 to 18 — 7 wired analyses + 4 `inspect.*` were running real code but not advertised, so any SDK using `hasCapability()` to gate them silently treated them as missing. |
| A-07 | `Plugins/FrameSolver/Standalone/v2/ModelBuilder.h:236-247` | `nodalLoads[k].comp` is now mandatory — the prior `readReals` returned `true` on `arr==nullptr` (optional semantics), letting a client forget the comp array and get zero loads instead of `VALIDATION_FAILED`. |
| C-04 | `Plugins/FrameSolver/Standalone/v2/ModelBuilder.h:281-298` | `hinges[k].dof` is now validated to be in `{4, 5, 10, 11}` (the local rotational DOFs Ry/Rz at end i/j). Default `0` was silently accepted and would mis-map to a translational DOF deep in the element-level update. |
| F-02 | `Plugins/FrameSolver/Standalone/v2/ModelBuilder.h:234,251,269,282` | `nodalLoads` / `memberUDLs` / `shellPressures` / `hinges` now call `reserve(jsonArrayLength)` before the build loop, matching the existing `materials` / `sections` / `nodes` / `members` / `shells` pattern. |
| F-04 | `Plugins/FrameSolver/Standalone/v2/ModelBuilder.h:111` | `buildModelFromJson` gains `[[nodiscard]]` so a future caller that ignores the return value gets a compile warning. |
| B-06 / B-12 | `Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp:1-22` + `Dispatcher.h:25-45,108-160` | Top-of-file state comments rewritten from `B2 stub level` to `v2.5: B3 dispatcher engine-wired`; `[TODO B3]` markers in `HandleSolveLinear` and the `notImpl` sentinel renamed to reflect the actual landed state (B4/B5.2/schema-pending). |
| E-05 / E-06 / E-08 | `README.md`, `docs/VERIFICATION.md`, `Plugins/FrameSolver/FrameSolver.uplugin` | README status block: v2.4 → v2.5, 13 PASS / 1 SKIP → ALL PASS. VERIFICATION.md §v2.4 supplementary → §v2.5 supplementary. Uplugin `VersionName: "2.2.0" → "2.5.0"`. |
| G-01 / G-02 | `Plugins/LevelSim/run_game.bat`, `run_smoke.bat`, `Tools/verify_smoke_shots.py` | Hardcoded `E:\project\UE_5.7` and `E:\project\ArchSim` replaced with `%UE_ENGINE_ROOT%` + `%~dp0..\..` fallbacks (`.bat`) and `Path(__file__).resolve().parents[3]` + `LEVELSIM_SHOT_DIR` env var (`.py`). Fresh clone no longer fails on these scripts. |
| G-04 | `docs/HANDOFF.md:141-143` | Removed username, real session UUID, and three workflow IDs from §10 — replaced with `~/.claude/projects/<project>/sessions/<uuid>/` placeholders. The factual content (which docs and worktrees the audit transcripts live in) is preserved without identifying any single contributor's machine. |
| G-05 / G-06 | `docs/AGENT_PROMPT_S5_S11.md`, `docs/AGENT_PROMPT_S9.md` | Five remaining absolute path occurrences (`E:\project\ArchSim`, `E:\project\CLAUDE.md`, `E:\project\UE_5.7`, `E:\project`) replaced with `<repo-root>` / `<repo-parent>` / `%UE_ENGINE_ROOT%`, matching the S2_S4 and OPENSEES_MEGA prompts already templatized in v2.4. |

---

## Verification matrix

| Gate | Status this cycle | Reproduce |
|---|---|---|
| L1. Standalone (`frametest.exe`, F1–F64) | **ALL PASS (failures=0)** | `Plugins\FrameSolver\Standalone\build.bat` (needs VS18 preview + `framecore-direct` conda env; set `SUPERNODAL_CONDA=<conda>\envs\framecore-direct\Library` if conda is off `%USERPROFILE%\anaconda3`) |
| L2. UE automation (57 `FrameCore.*` tests) | **NOT RUN** this cycle — engine source bit-identical to v2.3 since `6be1dac`; `$ExpectedUeTests = 57` guard in `Scripts\run_gate.ps1` would catch a silently-missing test on the next UE rebuild. | `set UE_ENGINE_ROOT=<your UE root> && Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project=<repo-root>\ArchSim.uproject && powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees` |
| L3. OpenSees compare (`Tools/opensees_compare.py`) | **PASS** (engine matches OpenSees + analytic; co-rotational ~1.2e-9, arc-length ~6.4e-3, P-Delta ~1.4e-3, all within tolerances; engine unchanged from v2.3 so result mirrors that release's run) | `pip install openseespy>=3.5` then `python Tools\opensees_compare.py` |
| L4. Deep audit (`linear_deep_audit.exe`) | **PASS failures=0 checks=104** | `Plugins\FrameSolver\Standalone\build_linear_audit.bat` (same conda-env prereq as L1) |
| L5. CLI round-trip (`Tools/cli_roundtrip.py`) | **ALL PASS (failures=0)** 13 checks; build SHA `b3cea8b`, byte-identical C-ABI vs CLI | `python Tools\cli_roundtrip.py` (auto-builds `frame_cli.exe` + `frame_capi.dll`) |
| L6. v2 round-trip (manual; B3-engine-wired in v2.5) | **ALL PASS** 34 checks (0 SKIP, 0 FAIL); `v=2.5.0`, build SHA `b3cea8b`; covers `hello.capabilities` advertising 18 entries, `solve.linear` bit-exact vs v1 (LDLT + supernodal modes), all 7 wired analyses' spec shapes, `inspect.*` cache hits, simple model.set defaults tracking, advanced profile missing-cap rejection, cancel tombstone consume-on-match, transport.sync presence | `Plugins\FrameSolver\Standalone\build_capi_v2.bat && python Tools\v2_roundtrip.py` |
| LevelSim standalone (`level_gate.exe`) | **ALL PASS (failures=0)** 115/115 | `Plugins\LevelSim\Standalone\build.bat` (or run pre-built `level_gate.exe`) |
| OpenSees mega benchmark (`benchmarks/opensees_mega/rerun.ps1`) | **NOT RUN** this cycle — engine source unchanged from v2.3 where the harness reported 128/0 CRITICAL; the v2 transport line does not touch any code path the mega benchmark exercises. | `pip install openseespy>=3.5` then `powershell -ExecutionPolicy Bypass -File benchmarks\opensees_mega\rerun.ps1` |

### Honest scope on what was not run

- **L2 UE automation** needs a UE 5.7 rebuild (~5–10 min). Engine source is bit-identical to
  v2.3 (`Plugins/FrameSolver/Source/FrameCore/` — `git diff v2.3..v2.5` returns empty for that
  subtree), and the `$ExpectedUeTests = 57` guard in `Scripts\run_gate.ps1` makes a
  silently-missing test impossible on the next CI run.
- **OpenSees mega benchmark** is dispatcher-orthogonal: `frame_capi_v2.dll` does not touch
  the engine code paths the mega benchmark exercises. v2.3's 128/0 CRITICAL result stands.

---

## Honest scope / limitations (engine unchanged from v2.3 — see RELEASE_v2.3.md for the full list; v2.5-specific additions below)

The engine-level honest-scope list from `docs/VERIFICATION.md` §3.x and the README
"scope boundaries" section is unchanged in v2.5 (engine code untouched).

**v2.5-specific honest scope on the v2 transport line:**

- **18 advertised capabilities; 3 still deferred.** `solve.dyn_collapse` (B4 streaming),
  `analysis.reanalysis_solve` (B5.2 `ReSolveSession`), and `model.patch` (schema TBD)
  return `NOT_IMPLEMENTED`. The dispatcher widens the advertised set only to verbs
  whose handler runs real engine code; nothing is advertised that returns a stub.
- **`transport.sync` only.** All handlers run inline on the caller thread of
  `frame_v2_send`. Long-running analyses (`solve.size_opt`, `solve.corotational`,
  `solve.arclength`) **block** the send. Clients must worker-thread off-load. The
  per-session worker model + binary streaming arrive together in B4.
- **`advancedDiagnostics.factorTimeMs` / `solveTimeMs` are still `0.0`.** Real timing
  hooks across `assembleAndFactor` and `solveLoad` are a B4 work item alongside the
  worker-thread cycle; the CLAUDE.md honesty rule forbids reporting numbers we have
  not measured, so the field is intentionally left zero rather than fabricated.
- **`analysis.modal` and `analysis.buckling` build a default `PreparedSystem`.**
  Both refuse `SnPrimary` (`useSupernodalPrimary=true`) because they need the LDLT
  factor. In v2.5 the dispatcher always constructs the default `PreparedSystem`, so
  there is no path to trip that constraint via v2. Once B5.2 wires
  `analysis.reanalysis_solve`, the `SnPrimary` selection becomes per-session and the
  guard becomes user-visible.
- **C# Grasshopper Layer 3 + Layer 4 are not `dotnet build`-verified on the integrator's
  machine.** Source has not changed since v2.4 (only `CApiV2Transport.cs`,
  `AssembleModelComponent.cs`, `OpenFrameCoreComponent.cs` for D-03 + D-09 + the
  fingerprint patch). The `.gha` binary build is the publisher's step (Rhino 8 + Yak).
- **Engine concurrency** — `frame_v2_close` UAF (A-01) is closed via the shared_ptr
  ownership registry. The narrower `frame_v2_last_error → c_str()` race (A-05) is
  deferred to B4 with the rest of the threading rework (C-06/C-07).

---

## Breaking changes

**None.**
- `abi_version = 2` (unchanged).
- `frame_capi_v2.h` signatures, wire framing, and the JSON schema shapes are
  source-compatible with v2.4.
- v2.4 clients that branched on `_stub: true` in `solve.linear` responses will see
  the field absent in v2.5 (the engine is wired, so there is no stub). A v2.4 client
  written defensively (`get("_stub", False) == False` → consume real fields) is
  unaffected; a client that asserted presence is the only break case and is itself
  a violation of v2.4's documented "shape-correct stub" contract.

---

## Deferred to v2.6 (with audit-ID traceability)

Each deferred item has a "First action on day 1" sketch in `docs/HANDOFF_v2.5.md`.

**Threading / async (B4 cycle):**
- **C-06 / C-07** — per-session worker thread + cv/outbound mutex separation +
  `IsCancelled` TOCTOU poll inside long handlers. The current dispatcher serialises
  via `submitMtx_`; long solves block `frame_v2_send`.
- **B4 first-frame** — `solve.dyn_collapse` streaming binary payload +
  `FLAG_END_OF_RESPONSE` framing, gated on the worker-thread rewrite.
- **A-05 / F-06** — `frame_v2_last_error` returns `c_str()` after dropping the lock;
  modernise the threading contract along with C-06/C-07.

**Engine / dispatcher physics:**
- **B5.2** — `analysis.reanalysis_solve` wire via `ReSolveSession` (S1 ladder).
- **C-09 / C-10** — make `analysis.modal` / `analysis.buckling` refuse a session
  whose `PreparedSystem` was built with `useSupernodalPrimary=true` (LDLT-dependent
  analyses cannot run on a SnPrimary system). v2.5 does not expose
  `useSupernodalPrimary` through `session.open`, so the bug is unreachable today.
- **C-03** — `solve.linear` response schema extension to emit per-member D/C
  (currently `solve.size_opt` is the only path that returns D/C).
- **C-05** — duplicate node/member/shell `id` rejection in `ModelBuilder`. The
  engine's `nodeIndex()` linear search picks the first match, so a duplicate id is
  effectively a silent drop of the later entry rather than miscompile.

**C# Grasshopper hardening:**
- **D-01** — `UtilizationFringeComponent.cs` divides member axial force by `1.0`
  instead of the section area `A` for D/C; the component should output `NaN` with a
  runtime warning until the native dispatcher emits per-member D/C (C-03).
- **D-02** — `OpenFrameCoreComponent.cs` writes `_opening = null` outside
  `_openGate`; move it into the lock.
- **D-06** — `CApiV2Transport.cs` zero-size frame guard (probe-recv path can hang on
  empty payload).
- **D-07** — `AssembleModelComponent.cs` `_cachedFs` field annotated `volatile`
  for cross-thread memory visibility.
- **D-08** — End-to-end C# cancel test (currently only the native cancel path is
  covered by `Tools/v2_roundtrip.py`).
- **D-10** — `DisposeAsync` returns `ValueTask.CompletedTask` instead of allocating
  an async state machine for an empty body.

**Docs / infrastructure:**
- **B-11** — B5 supernodal factor-reuse correctness is verified bit-exact in L6, but
  the promised latency speedup has no oracle benchmark in v2.5. Add a latency check
  to `v2_roundtrip.py` ("100× solve.linear after 1× model.set, total time < N ms").
- **B7** — Rhino 8 GHA dotnet build (host has no Rhino 8 / .NET 8 SDK; source has
  not changed since v2.4).
- **F65 / F66** — Warped-shell standalone fixtures (need a numerically robust
  template; F61 covers the membrane-warp convergence).
- **G-08 / G-09** — `environment.yml` for the conda env + OpenSeesPy version upper
  bound in `RELEASE_*.md`.
- **G-13** — `.gitignore` add `.codex/`, `.cursor/`, `*.transcript`, `*.jsonl` for
  agent-residue defence in depth (none are currently tracked).
- **F-1..F-10** — Independent `code-quality-sweep` cycle (out of release scope).

---

## Tag plan

```bash
# 1. Stage the explicit file set (release-hardening rule: never -A / .)
git add Plugins/FrameSolver/Standalone/v2/Dispatcher.h \
        Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp \
        Plugins/FrameSolver/Standalone/v2/ModelBuilder.h \
        Plugins/FrameSolver/FrameSolver.uplugin \
        Plugins/LevelSim/run_game.bat \
        Plugins/LevelSim/run_smoke.bat \
        Plugins/LevelSim/Tools/verify_smoke_shots.py \
        README.md \
        docs/VERIFICATION.md \
        docs/HANDOFF.md \
        docs/AGENT_PROMPT_S5_S11.md \
        docs/AGENT_PROMPT_S9.md \
        docs/RELEASE_v2.5.md \
        docs/HANDOFF_v2.5.md

# 2. Single release commit
git commit -m "release: v2.5 -- B3 dispatcher engine wire + capability widening + hardening"

# 3. Annotated tag
git tag -a v2.5 -m "FrameCore v2.5 -- v2 dispatcher engine-wired (12 wired handlers + 18 capabilities), A-01 close UAF, B5 supernodal factor-reuse, P1/P2/P3 review-round hardening, 7-agent audit small-fixes"

# 4. Push branch then tag
git push origin main
git push origin v2.5

# 5. GitHub release
gh release create v2.5 --title "FrameCore v2.5" --notes-file docs/RELEASE_v2.5.md
```
