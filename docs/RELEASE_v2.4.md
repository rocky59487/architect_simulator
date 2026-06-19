# Release v2.4 — Rhino bridge v2 + B2 dispatcher + 6th gate leg

**Date**: 2026-06-19 · **Branch**: `main` · **Tag**: `v2.4` · **Repo**: https://github.com/rocky59487/architect_simulator

v2.4 is a **transport-layer release**: it ships the v2 external bridge (designed in
`docs/specs/S6b_rhino_bridge_v2.md` + `docs/specs/S6c_rhino_ux_commercial.md`), a
B2 stub-level dispatcher binary (`frame_capi_v2.dll`, ~105 KB after the release-hardening
MiniJson / FrameWire safety edits; was 109 KB pre-edits per `docs/PROGRESS_B2.md` snapshot),
a 53-file C# SDK + Rhino 8 GHA skeleton, and a 6th *manual-only* gate leg
(`Tools/v2_roundtrip.py`, 13 PASS / 1 SKIP).

A 26-lesson whiteboard course (`docs/learning/`, ~46 k lines incl. deep course) is
bundled alongside as courseware — independently readable, not part of any gate.

**The FrameCore engine source is unchanged from v2.3.** The five-leg gate that gated
v2.3 still gates the engine bit-for-bit. The new B2 dispatcher is a separate transport
binary; it does NOT link FrameCore and **all engine-level analysis methods are
stubbed at NOT_IMPLEMENTED until B3** (release notes section *Scope / Honest
limitations* below).

LevelSim is unchanged from v1.0.0; the bundled-release tag continues to identify
this release as one source-of-truth (FrameCore v2.4 + LevelSim v1.0.0).

---

## FrameCore v2.4 — what's new (transport line)

| Area | Change |
|---|---|
| C ABI v2 | New `Plugins/FrameSolver/Standalone/frame_capi_v2.{h,cpp}` — 8 exports (`frame_v2_open/close/send/recv/cancel_recv/cancel_request/last_error/pending_count` + `abi_version/build_sha/engine_version`). Documented RPC thread-safety contract. |
| v2 dispatcher | `Plugins/FrameSolver/Standalone/v2/Dispatcher.{h,cpp}` + `FrameWire.h` + `MiniJson.h`. Hand-written zero-dep JSON + framed wire (magic `FC` + 16-bit flags + two LE u32 length fields). 21 registered method handlers (hello + 4 session-mgmt **wired**, `model.set` / `solve.linear` shape-correct stubs, the rest `NOT_IMPLEMENTED` for B3). `hello.capabilities` returns 6 truly-usable: `{cancel, profile.advanced, profile.simple, session, model.set, solve.linear}`. |
| Build recipe | `Plugins/FrameSolver/Standalone/build_capi_v2.bat` — standalone, does **not** link the engine, does **not** depend on the conda env. ~10 s compile. |
| C# Layer 3 SDK | `Plugins/FrameSolver/Grasshopper/v2/FrameCore.Bridge.csproj` — net7.0, zero Rhino dep. `Bridge/` (FrameSession, ITransport, CApiV2Transport, FrameProtocol, BridgeOptions, Profiles, ...), `Model/` (immutable FrameModel + FrameModelBuilder), `Result/` (typed POCOs incl. AdvancedDiagnostics). |
| C# Layer 4 Rhino GHA | `Plugins/FrameSolver/Grasshopper/v2/Rhino/FrameCore.Gh.csproj` — net7.0, Rhino 8 GA SDK. 10 representative components (Setup / Analyze / Inspect / Display / Material / Section / Advanced), Common helpers (AsyncComponent, PreviewPipeline, RhinoBaker, Units, ...), MaterialLibrary (11 steels), SectionLibrary (40+ H-sections). |
| 6th gate leg (manual) | `Tools/v2_roundtrip.py` — ctypes-load `frame_capi_v2.dll`, drive `hello`/`session.open`/`model.set`/`solve.linear`/`session.close` plus the advanced-profile reject path. **13 PASS / 1 SKIP / 0 FAIL** (SKIP = `solve.linear bit-exact vs v1`, deferred to B3 when the dispatcher wires the engine). Not in `run_gate.ps1`; run by hand. |
| Spec docs | `docs/specs/S6b_rhino_bridge_v2.md` (wire protocol + dual profile + 19 method catalogue + forward-compat rules) + `docs/specs/S6c_rhino_ux_commercial.md` (80 GH component catalogue, Display patterns, default libraries, Bake). |
| Progress / handoff | `docs/PROGRESS_B2.md` + `docs/HANDOFF_rhino_bridge_v2.md` (B1, marked SUPERSEDED) + `docs/HANDOFF_rhino_bridge_v2_final.md` (B2 + 3-round P0/P1/P2 fixes). New `docs/HANDOFF_v2.4.md` wraps the cycle. |
| Whiteboard courseware | `docs/learning/framecore_v2_course_lesson1.md` (998 lines) + `docs/learning/deep_course/` (25 lessons + `framecore_v2_deep_course_combined.md` 39 645 lines + generators). Standalone, not exercised by any gate. |
| `.gitignore` | Patched: `*.dll`, `*.lib`, `*.exp`, `obj_capi_v2/` excluded so build artefacts of v1 / v2 C-ABI never leak to the tree. |

### Release-hardening fixes folded into the v2.4 commit-prep

The Phase-1 audit on the v2 surface drove a small batch of correctness / safety / doc
fixes before tagging. Citations refer to audit IDs from the 7-agent sweep
(consolidated in `docs/HANDOFF_v2.4.md`).

- **C-01 / C-02 / C-05 — DoS guards.** `MiniJson` gains `kMaxJsonDepth = 64` (rejects
  deeply-nested adversarial input with stack-overflow risk); `FrameWire` gains
  `kMaxHeaderLen = 16 MiB` / `kMaxPayloadLen = 256 MiB` guards before any allocation.
- **A-04 — silent overflow-to-Inf.** `MiniJson::parseNumber` now rejects non-finite
  results from `strtod` (e.g. legal-JSON `1e400` silently parsed as `+Inf` and would
  poison every downstream double once B3 wires the engine).
- **C-03 — parser diagnostics.** `parseBool` / `parseNull` now fill `err` on the
  miss path so failure messages aren't blank.
- **D-15 — Grasshopper cache fingerprint precedence bug.** `AssembleModelComponent.
  ComputeFingerprint` had `h = h*31 + X ^ Y ^ Z` which `+` bound tighter than `^`,
  silently losing the `h` accumulation when only Y/Z node coordinates changed → GH
  slider drags on Y/Z hit a stale cached engine session. Parenthesised the XOR group.
- **B-03 — engine_version reports v2.4.0.** `Dispatcher.h::kEngineVer = "2.4.0"`
  (was `"2.3.0"`); the captured `v2_roundtrip` output now reports `v=2.4.0`.
- **B-04 — spec example.** S6b § 2.3 hello example now uses `"2.4.0"` + a note that
  the 16-capability list is the *roadmap* (B2 actually wires 6).
- **D-06 / D-07 — net7.0 documentation alignment.** v2 README and Gh csproj header
  comment now consistently say net7.0 (post-P0.2 fix).
- **B-08 / B-09 — capability and method count alignment.** HANDOFF_rhino_bridge_v2_final
  + PROGRESS_B2 now read `6 capabilities` (post-P2.1, was stale `10`) and `21 handlers
  / 19 method catalogue + hello` (was stale `22`).
- **E-01 / E-02 / E-03 — README + docs/README.md sync.** README banner bumps to v2.4
  with the new transport-line narrative; docs/README.md indexes the v2 bridge docs +
  the learning courseware. `HANDOFF_rhino_bridge_v2_final.md § ⑪` updated from
  "patch not applied" to "patch applied in 10b767c".
- **E-05 — bundled-release line.** docs/README.md L56 LevelSim bundled-release blurb
  bumps to v2.4.
- **E-06 / E-09 — history banners.** B1 handoff marked SUPERSEDED; OpenSees mega
  benchmark agent-prompt marked HISTORY (written for v2.1.0 cycle).
- **B-13 — chain forward pointer.** HANDOFF_v2.3.md now points forward to v2.4 docs.
- **B-14 — cross-reference.** CLI_PROTOCOL.md (v1 bridge) now points at S6b
  (v2 bridge) so a reader picking either entry can discover the other.
- **H-01 — privacy.** `docs/learning/deep_course/generate_deep_course.py:10` used
  to hardcode `C:\Users\<user>\Downloads\...` (the integrator's local machine path).
  Replaced with an `os.environ.get("SAMPLE_LESSON1", …)` fallback that resolves
  to the bundled `lesson_01_*.md` neighbour. No more username leakage.
- **H-08 — env-var doc.** README's standalone-gate section now mentions the
  `SUPERNODAL_CONDA` override for contributors with conda installed off
  `%USERPROFILE%\anaconda3`.

Total source-line delta from v2.3:
- **Engine code: 0 lines changed** (engine source under
  `Plugins/FrameSolver/Source/FrameCore/` is bit-identical to v2.3).
- v2 transport (`Plugins/FrameSolver/Standalone/v2/` + `frame_capi_v2.{h,cpp,bat}`):
  ~1 600 LOC added across 8 files; release-hardening edits ~30 LOC.
- v2 C# (`Plugins/FrameSolver/Grasshopper/v2/`): ~5 800 LOC across 35 files +
  18 component files; release-hardening edits ~25 LOC.
- Tooling + docs: ~2 200 LOC (`Tools/v2_roundtrip.py`, two HANDOFF docs, PROGRESS_B2,
  S6b + S6c spec docs).
- Courseware: ~45 600 lines (`docs/learning/`, intentional bundle, not gated).

---

## Verification matrix

| Gate | Status this cycle | Reproduce |
|---|---|---|
| L1. Standalone (`frametest.exe`, F1–F64) | **ALL PASS (failures=0)** | `Plugins\FrameSolver\Standalone\build.bat` (needs VS preview + `framecore-direct` conda env; set `SUPERNODAL_CONDA=<conda>\envs\framecore-direct\Library` if conda is off `%USERPROFILE%\anaconda3`) |
| L2. UE automation (57 `FrameCore.*` tests) | **NOT RUN** this cycle (engine unchanged from v2.3; no UE source change in `Plugins/FrameSolver/Source/FrameCore/` since `6be1dac`) | `set UE_ENGINE_ROOT=E:\project\UE_5.7` (or your engine root) `&& Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project=…\ArchSim.uproject && powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees` (this runs L1–L5 in one go after a UE rebuild) |
| L3. OpenSees compare (`Tools/opensees_compare.py` + `pdelta_compare.py`) | **NOT RUN** this cycle (no `openseespy` installed in any conda env on the integrator's machine; `pip install openseespy` and rerun) | `pip install openseespy>=3.5` then `python Tools\opensees_compare.py` and `python Tools\pdelta_compare.py` |
| L4. Deep audit (`linear_deep_audit.exe`) | **PASS failures=0 checks=104** | `Plugins\FrameSolver\Standalone\build_linear_audit.bat` (same conda-env prereq as L1) |
| L5. CLI round-trip (`Tools/cli_roundtrip.py`) | **ALL PASS (failures=0)** 13 checks; build SHA `10b767c`, byte-identical C-ABI vs CLI | `python Tools\cli_roundtrip.py` (auto-builds `frame_cli.exe` + `frame_capi.dll`) |
| L6. v2 round-trip (manual; new in v2.4) | **13 PASS / 1 SKIP / 0 FAIL**; `v=2.4.0`, build SHA `10b767c` | `Plugins\FrameSolver\Standalone\build_capi_v2.bat && python Tools\v2_roundtrip.py` |
| LevelSim standalone (`level_gate.exe`) | **ALL PASS (failures=0)** 115/115 | `Plugins\LevelSim\Standalone\build.bat` (or run pre-built `level_gate.exe`) |
| OpenSees mega benchmark (`benchmarks/opensees_mega/rerun.ps1`) | **NOT RUN** this cycle (depends on `openseespy`; v2.3 results `results/20260619-001/` show 0 CRITICAL / 0 MAJOR and are unaffected by the v2 transport line) | `pip install openseespy>=3.5` then `powershell -ExecutionPolicy Bypass -File benchmarks\opensees_mega\rerun.ps1` |

### Honest scope on what was not run

L2 / L3 / mega benchmark are listed `NOT RUN` because:
- L2 needs a fresh UE 5.7 rebuild (~5–10 min); engine source is untouched from v2.3,
  and the `$ExpectedUeTests = 57` guard in `Scripts\run_gate.ps1` will catch a
  silently-missing test on the next CI run.
- L3 and mega benchmark need `openseespy` and the integrator's machine does not have
  it installed in any conda env at v2.4 release time. The v2 transport line does not
  touch any code path L3 / mega benchmark exercises (the dispatcher does not link
  FrameCore at B2), so they cannot regress.

---

## Honest scope / limitations (unchanged from v2.3 for engine; new for v2 bridge)

The engine-level honest-scope list from `docs/VERIFICATION.md` §3.x and the
README "scope boundaries" section is unchanged in v2.4 (engine code untouched).

**New B2-level honest scope on the v2 transport line:**
- B2 is **stub-level dispatcher**: only `hello`, `session.open/close`,
  `cancel`, and `model.set` are end-to-end wired. `solve.linear` returns a
  shape-correct response with `_stub: true` set in the body. All other analysis
  methods (`solve.pdelta`, `solve.tension_only`, `solve.size_opt`,
  `solve.dyn_collapse`, `solve.corotational`, `solve.arclength`, `analysis.modal`,
  `analysis.buckling`, `inspect.*`) are registered handlers that return
  `NOT_IMPLEMENTED`. The 6th gate leg verifies this contract — the SKIP it carries
  is intentional and named (`solve.linear bit-exact vs v1`).
- B2's `engineVersion = "2.4.0"`, `schemaVer = "2026.06"`, `abi_version = 2`,
  `build_sha = "10b767c"` (build-time `git rev-parse --short HEAD`).
- C# Layer 3 + Layer 4 are **not `dotnet build`-verified on the integrator's
  machine** (only Rhino 8 .NET SDK can build them; this machine has only the .NET 8
  runtime). They were reviewed structurally + statically by the Phase-1 D-agent
  audit. The release ships them as source; the .gha binary build is the
  publisher's step (Rhino 8 + Yak environment).
- Concurrency model is documented in `frame_capi_v2.h` ("RPC pattern: single recv +
  concurrent send + concurrent cancel"). Two known architectural concerns are
  deferred to B3+ (see Deferred section): a narrow `frame_v2_close` → `delete`
  window if a `recv` is mid-wait, and a TOCTOU between `IsCancelled` and the
  handler call for streaming methods. Neither is a v2.4-blocker because B2's
  handlers are sub-millisecond and the C# SDK serialises send/recv per session.

---

## Breaking changes

**None.** The v1 bridge (`frame_cli.exe` + `frame_capi.dll`, CLI_PROTOCOL.md) is
preserved as-is; v2 is a separate transport binary with a separate header. Clients
pick one — no migration cost for existing Grasshopper users on `frame_cli`.

The v2 dispatcher does NOT link the FrameCore engine yet, so the engine has the
exact same binary footprint as v2.3.

The `.gitignore` patch adds `*.dll`, `*.lib`, `*.exp`, `obj_capi_v2/`. No tracked
file is orphaned by the new rules (`git ls-files | grep -iE '\.(dll|lib|exp)$'`
returns empty).

---

## Deferred / not in this release (with audit-ID traceability)

All items below have a "First action on day 1" sketch in
`docs/HANDOFF_v2.4.md`. They are deliberately deferred because each is either
architectural (B3+), out of scope for a B2 stub release, or a docs-only follow-up.

**Architectural / B3+ (engine-wire required)**

- **A-01 — `frame_v2_close` race window.** Close `delete`s the ctx while a `recv`
  may be in `cv.wait`. Fix shape: shared-ptr ctx or atomic refcount. Lands in B3.
- **A-04 (B3-tier) — engine-side propagation.** Once B3 wires the engine,
  the MiniJson finite-check (already in place) ensures B3 cannot silently consume
  a `+Inf`; B3 needs to mirror the check on `disp` / `force` outputs before
  serialising back, to catch overflow on the engine side too.
- **C-06 — cv/outbound mutex split.** `Submit()` enqueues to `outbound_` under
  `outMtx_`, but the recv loop signals on `ctx->cv` guarding a *different* state.
  Benign at B2 (sub-ms handlers); MUST be redesigned at B4 streaming.
- **C-07 — IsCancelled TOCTOU.** Cancel between `IsCancelled` check and handler
  dispatch is not detected; handler runs to completion. Acceptable at B2 (handlers
  are sub-ms); B4 streaming handlers must re-check `IsCancelled` inside their
  event loops.
- **D-03 — GH OpenFrameCore generation guard race.** A Reset that races between
  `Interlocked.Read` and the field-write can commit a stale `FrameSession`.
  Narrow window; existing P1.2 double-check catches most cases. Tighten when
  B7 puts real load on the path.
- **D-09 — P/Invoke signature audit.** `CApiV2Transport.cs` delegate signatures
  were not line-by-line cross-checked against `frame_capi_v2.h` in this audit;
  this MUST happen before any external `dotnet build` of the GHA.
- **B3 method wiring** — `Dispatcher.cpp` carries `[TODO B3]` markers; first
  action is `HandleSolveLinear`.
- **B4 streaming + binary + per-handler cancel** — `solve.dyn_collapse` needs the
  framed binary payload path + a per-handler cancel poll.
- **B5 session factor-reuse** — wrap `ReSolveSession` / `SnSession` per session id.
- **B7 Rhino 8 GHA actual build** — Rhino 8 .NET SDK / NuGet packages outside the
  engine CI; deferred to publisher.

**Engine-level / per-stage tightening**

- **F65 / F66** — standalone fixtures for the v2.3 warped-shell CLI fix
  (already gated indirectly by the mega benchmark 24 → 0 CRITICAL regression).
- **`build.bat` conditional supernodal skip** — let contributors on a vanilla MSVC
  machine (no conda env) build the first 56 fixtures by setting a `FC_NO_SUPERNODAL`
  flag. Patch sketch is in the Phase-A subagent transcript of the v2.3 cycle.
- **LevelSim D-08** — player-elev propagation doc; unchanged from v2.3 deferred.

**Docs follow-ups (low priority)**

- **B-12** — README DOF figure `61.5k` vs `62k` (perf_sn output uses `62k`). One
  character to align; deferred to next docs touch.
- **E-07 (partial)** — `HANDOFF_v2.4.md` (this release) is the wrapper that links
  the v2.4 cycle into the HANDOFF chain. E-10 (S6b method table `[B3]`/`[B4]`/`[B5]`
  per-row reserved markers) is deferred to the B3 cycle (which actually unlocks
  those rows).
- **H-02 / H-03 / H-04** — `E:\project\ArchSim` and `C:\Users\<user>\.claude\...`
  literals in `docs/HANDOFF_rhino_bridge_v2_final.md`, `docs/AGENT_PROMPT_OPENSEES_MEGA_BENCHMARK.md`
  and `docs/AGENT_PROMPT_S2_S4.md` are context-illustrative lines (command examples
  in a hand-off note), not executable code. The OpenSees prompt already got a
  HISTORY banner (E-09). Replacing every literal with `<repo-root>` placeholders
  is a v2.4.1 docs touch.
- **H-09 / H-10** — `generate_framecore_whiteboard_course.py` Windows font
  hardcode, and `obj_capi/` / `obj_linear_audit/` not yet in `.gitignore` (the
  existing `*.obj` rule covers their content, no risk today).
- **F-1..F-10** — C++ / C# code-smell cleanups (noexcept annotations, reserve
  before push_back, MemoryStream → ArrayBufferWriter, etc.) — defer to a
  `code-quality-sweep` cycle.

---

## Tag plan

```bash
# After Phase 4.5 final-integrator pass is green, Phase 5 runs:
git add <files-changed-this-cycle>          # explicit, never -A / .
git commit -m "release: v2.4 — Rhino bridge v2 + B2 dispatcher hardening" -m "..."
git tag -a v2.4 -m "FrameCore v2.4 — Rhino bridge v2 + B2 dispatcher + 6th gate leg"
git push origin main
git push origin v2.4
gh release create v2.4 \
    --title "FrameCore v2.4 — Rhino bridge v2 + B2 dispatcher + 6th gate leg" \
    --notes-file docs/RELEASE_v2.4.md
```

The release commit will sit on top of `10b767c` ("feat: add Rhino bridge v2 and
benchmark docs"). It carries only release-hardening edits (~80 LOC across docs +
the four code safety fixes A-04 / C-01 / C-02 / D-15 + the doc index updates +
the privacy fix H-01) plus this RELEASE_v2.4.md + HANDOFF_v2.4.md.
