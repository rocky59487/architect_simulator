# HANDOFF v4.0.0 — FrameCore stable long-term anchor

> v4.0.0 ships on 2026-06-23. Tag `v4.0.0` re-seals `v3.6.0` (commit `c3e70e5`,
> "FrameCore FINAL — engine source FROZEN") as the **stable long-term anchor**.
> The primary handoff is `docs/HANDOFF.md`; v3.6.0 HANDOFF (`docs/HANDOFF_v3.6.0.md`)
> is frozen history. This file only documents what v4.0.0 adds.

## 1. What v4.0.0 is

- A **"policy major bump"**: engine source delta vs v3.6.0 = **0 lines** under
  `Plugins/FrameSolver/Source/FrameCore/`. Wire ABI, capability list, USTRUCT
  layout, public C API and engine numerics all bit-identical with v3.6.0.
- The first **stable** anchor. The v3.x series was an evolving engine; v4.0.0
  seals the engine algorithms. No v3.7 will ship.
- A formal **FROZEN marker** in `CLAUDE.md` 鐵則 #1 and this handoff: future PRs
  touching `Plugins/FrameSolver/Source/FrameCore/` must first amend the marker
  with explicit rationale before the change is accepted.
- A **lockstep version-pin bump** (`Dispatcher.h kEngineVer 3.6.0 → 4.0.0`,
  `FrameSolver.uplugin Version 35 → 36 / VersionName 3.6.0 → 4.0.0`,
  `FRAMECORE_EXPECTED_ENGINE_VER 3.6.0 → 4.0.0` in `run_gpu_gate.ps1` and
  `release-gate.yml`).

`git diff --stat v3.6.0..v4.0.0` covers:
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` — kEngineVer bump + v3.6 / v4.0 changelog blocks + B-06 wording
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp` — B-06 wording
- `Plugins/FrameSolver/FrameSolver.uplugin` — Version 35 → 36, VersionName 3.6.0 → 4.0.0
- `Scripts/run_gpu_gate.ps1` — FRAMECORE_EXPECTED_ENGINE_VER 3.6.0 → 4.0.0
- `Scripts/run_gate.ps1` — stale `-ExpectedUeTests 68` comment → 133 + v4 footnote
- `Scripts/run_exit_tests.ps1` — D2/D4 "v3.6.1 follow-up" → "v4.0.x follow-up"
- `.github/workflows/release-gate.yml` — FRAMECORE_EXPECTED_ENGINE_VER 3.6.0 → 4.0.0
- `README.md` — v4.0.0 stable seal status block + file-tree UE count refresh
- `docs/VERIFICATION.md` — `$ExpectedUeTests` text + gate table
- `docs/ARCHITECTURE.md` — fixture / UE test counts
- `docs/RELEASE_v4.0.0.md` (new)
- `docs/HANDOFF_v4.0.0.md` (this file, new)

## 2. How to reproduce the gate

The 5-leg + 6th-leg gate was verified on the integrator host on 2026-06-23
(Windows 11, MSVC vs2026 preview, UE 5.7, `framecore-direct` conda env at
`%USERPROFILE%\anaconda3\envs\framecore-direct`).

```powershell
# One-click 5-leg
$env:SUPERNODAL_CONDA = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library"
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees

# 6th leg (v2 dispatcher round-trip; verifies kEngineVer pin)
cmd /c Plugins\FrameSolver\Standalone\build_capi_v2.bat
$env:FRAMECORE_EXPECTED_ENGINE_VER = '4.0.0'
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"
python Tools\v2_roundtrip.py
```

Expected results (verified this session):
- Leg 1 standalone: `ALL PASS  (failures=0)`
- Leg 2 UE: `Automation Test Queue Empty 135 tests performed` (133 on non-cuDSS)
- Leg 3 OpenSees: `OPENSEES GATE: PASS` (shallow-arch von Mises 6.42e-3 rel)
- Leg 4 linear_deep_audit: `PASS failures=0 checks=104`
- Leg 5 CLI round-trip: `ALL PASS  (failures=0)`
- Leg 6 v2_roundtrip: `=== summary: ALL PASS ===`

Not run this session (carry-forward from v3.6.0 evidence; engine delta = 0
lines makes them bit-identical):
- GPU 6th-leg (`Scripts\run_gpu_gate.ps1 -Strict`)
- Exit-test (`Scripts\run_exit_tests.ps1`)

## 3. New token / flag / API

**None.** v4.0.0 introduces no new wire methods, no new capabilities, no new
USTRUCTs, no new env vars. The only user-visible surface change is the value
string `kEngineVer` reports in `hello.response.version`:

```
3.6.0 → 4.0.0
```

Clients that pin to the version string with an exact equality check should
update the expected value to `"4.0.0"`. Clients that range-match on the major
(`>= 3.0`, `< 5.0`) continue to work without changes.

## 4. Permanently deferred items (FROZEN-final, carried forward from v3.6.0)

These items remain permanently deferred. The "first action on day 1" for each
is the same: **none**. The FROZEN contract locks the engine source; the items
either don't apply to UE consumer-side patches or were explicitly deferred by
the user during the v3.x series.

| ID | Item | First action under v4.0.0 |
|---|---|---|
| S11 | MITC9i high-order shells | None (locked; needs 9 engine modifications; deferred by user). Re-evaluation requires CLAUDE.md amendment removing FROZEN marker. |
| U-08 | Showcase map + BP examples | None (Designer in-Editor work). `Tools/build_v3_5_showcase_map.py` is the starter; v4.0.x may evolve it. |
| U-09 | Chaos POD GeometryCollection | None (UE 5.7 Chaos churn). `AFrameFragmentClusterActor` StaticMesh thin slice is the v4 contract. UE 5.8+ upgrade is consumer-side only (no engine touch). |
| Ph10 | Live DynCollapse callback channel | None (multi-cycle UE work; `AFrameDynCollapseReplayActor` already delivers the user-visible effect). `DynCollapseOptions::onFrameEmitted` callback is in the engine, so the UE-side wire-up is a v4.0.x candidate without engine impact. |
| D2 | Exit-test bench ladder placeholder | None (FROZEN engine doesn't need it). `run_exit_tests.ps1` D1 (property sweep) + D3 (strict re-run) are the binding dimensions. |
| D4 | Exit-test fuzz placeholder | None (same as D2). |
| B-06 | `model.patch` schema | None (permanent defer). If a v4.0.x author writes `docs/specs/S6c_model_patch.md`, the dispatcher wire path can land without touching engine source. |
| G-01/02 | `build_sn_cuda.bat` / `build_capi_v2_cuda.bat` last-resort CUDA_ROOT hardcode | Optional. `SUPERNODAL_CONDA` env override path takes precedence; fresh-clone-without-override convenience only. A v4.0.x patch can replace the last-resort with a named-error exit. |
| G-03 | `Research/R2_realtime_150k/build_*.bat` unconditional CUDA_ROOT hardcode | None (research-only path; not in production gates). |

## 5. Lessons from this release-hardening cycle (durable)

The findings here are specific to v4.0.0 work, not the global lessons in
`CLAUDE.md` 踩雷 section.

- **Background `cmd /c <relative-path>.bat` does not always inherit the
  shell's cwd**. In this session, `Bash` with `run_in_background: true` running
  `cmd /c Plugins\FrameSolver\Standalone\build_capi_v2.bat` returned exit 0
  without running the build (the `cmd` shell could not resolve the relative
  path under the bash cwd). The fix is to call PowerShell with the **absolute
  path**:
  ```powershell
  $bat = 'E:\project\ArchSim\Plugins\FrameSolver\Standalone\build_capi_v2.bat'
  cmd /c "$bat"
  ```
  Without this, `frame_capi_v2.dll` keeps its previous `kEngineVer` and
  `v2_roundtrip.py` correctly fails its `engine_version == 4.0.0` pin check.
  **Always verify the DLL has the new `kEngineVer` string before declaring
  Leg 6 green**:
  ```powershell
  Select-String -Pattern '4\.0\.0' Plugins\FrameSolver\Standalone\frame_capi_v2.dll -Encoding default
  ```
- **`linear_deep_audit` static grep vs runtime print can diverge.** Lane B
  Phase 1 flagged 108 `addRow(` calls vs the documented 104. The runtime print
  is `checks=104`; the four delta is conditional branches the default build does
  not execute. Trust the runtime print, not the static count.
- **UE `IMPLEMENT_(SIMPLE|COMPLEX)_AUTOMATION_TEST` static grep vs distinct
  test class count can diverge.** Lane B flagged grep=136, distinct class=135.
  The single delta is a comment line in
  `FrameCoreUESolveResultMarshalTest.cpp:10` that mentions the macro name in
  prose. Runtime UE automation prints
  `Automation Test Queue Empty 135 tests performed` — trust that.
- **CLAUDE.md (`E:\project\CLAUDE.md`) sits one level above the git repo
  root.** It is a local instruction file, not commit-able from the repo at
  `E:\project\ArchSim`. The FROZEN marker text should ALSO live in this
  HANDOFF (current section + section 7) so the FROZEN contract is visible to
  anyone with just the repo.
- **Tag-time `git status` should be `## main...origin/main` with no listed
  files** before the release commit. The previous release (v3.6.0) had
  pushed exactly v3.6.0=HEAD before this hardening cycle started; the v4.0.0
  commit is the first new commit on `main` since then.

## 6. Direction after v4.0.0 (unordered)

- **v4.0.x patches** — UE consumer-side fixes (G-01 / G-02 CUDA_ROOT
  convenience; live DynCollapse channel; `model.patch` wire if a spec is
  authored). FROZEN engine ensures these stay zero-impact on Leg 1 / Leg 4 /
  Leg 6 numerics.
- **v4.1.x minors** — UE-side new actors / Blueprints / new visualisation
  surfaces on top of the FROZEN engine. The C6/C7/C8 along-span data line in
  v3.6 is the model: read existing engine outputs, render in UE.
- **Risk areas worth monitoring**:
  - UE 5.8+ Chaos destruction API stabilisation → opens U-09 GeometryCollection
    path (consumer-side only).
  - cuDSS / OpenBLAS / METIS DLL ABI churn on conda-forge updates → re-run
    `Scripts\run_gpu_gate.ps1 -Strict` after any conda upgrade.
  - GitHub Actions runner image churn → `.github/workflows/release-gate.yml`
    expects MSVC + UE; CI evidence can drift even when the engine doesn't.

## 7. The FROZEN contract (binding text)

> Effective with `v4.0.0`, the engine source under
> `Plugins/FrameSolver/Source/FrameCore/` is **FROZEN**. The following rules
> apply:
>
> 1. Any PR that modifies a file under that path REQUIRES a prior amendment to
>    `CLAUDE.md` 鐵則 #1 and to this `docs/HANDOFF_v4.0.0.md` section 7
>    removing the FROZEN marker, with explicit rationale.
> 2. The amendment is reviewed against the same standard as a major-version
>    bump (independent oracle, full 5-leg gate, audit pass).
> 3. Files under `Plugins/FrameSolver/Source/FrameCoreUE/` are **NOT FROZEN**
>    and may evolve under v4.0.x patch / v4.1.x minor releases.
> 4. The Standalone harness (`Plugins/FrameSolver/Standalone/`) and the v2
>    dispatcher (`Plugins/FrameSolver/Standalone/v2/`) are **NOT FROZEN** for
>    consumer-side improvements (DLL packaging, env-var fallbacks, new
>    dispatcher capabilities that don't change wire ABI). Changes here still
>    require the 5-leg gate and the version-pin lockstep rule.
> 5. The FROZEN contract is a **release-policy promise**, not a technical
>    impossibility. Removing the marker is allowed; doing so silently is not.

---

Questions: read `docs/HANDOFF.md` → `docs/HANDOFF_v3.6.0.md` →
`docs/V3_SERIES_RETROSPECTIVE.md` → this file. Subsystem-specific reads:
`docs/ARCHITECTURE.md` for module map, `docs/VERIFICATION.md` for oracle ladder,
`docs/CLI_PROTOCOL.md` for the text/daemon/C-API wire, `docs/specs/` for the
S1..S10 oracle specs.
