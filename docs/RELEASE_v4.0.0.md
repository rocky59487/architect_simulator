# RELEASE v4.0.0 — FrameCore stable

> Tag: `v4.0.0` · Anchor: post `v3.6.0` (FrameCore FINAL release)
> **v4.0.0 is the stable long-term anchor.** Engine source under
> `Plugins/FrameSolver/Source/FrameCore/` is FROZEN (formalised in CLAUDE.md
> 鐵則 #1 effective with this tag). Future patches (v4.0.x) and minor releases
> (v4.1.x) are UE consumer-side only.

## TL;DR

v4.0.0 re-seals v3.6.0 as the stable long-term anchor. It is a **"policy major
bump"**: engine source delta vs v3.6.0 = **0 lines** under
`Plugins/FrameSolver/Source/FrameCore/`, wire ABI / capability list / USTRUCT
layout all unchanged. The semantic shift is the stability promise — no v3.7 will
ship, and the engine algorithms are immutable from here. UE consumer code
(`Plugins/FrameSolver/Source/FrameCoreUE/`) remains evolvable under v4.0.x patch /
v4.1.x minor releases.

## What ships

### Lockstep version-pin bump

| pin | before | after |
|---|---|---|
| `Dispatcher.h kEngineVer` | `"3.6.0"` | `"4.0.0"` |
| `FrameSolver.uplugin Version` | `35` | `36` |
| `FrameSolver.uplugin VersionName` | `"3.6.0"` | `"4.0.0"` |
| `Scripts/run_gpu_gate.ps1 FRAMECORE_EXPECTED_ENGINE_VER` | `'3.6.0'` | `'4.0.0'` |
| `.github/workflows/release-gate.yml FRAMECORE_EXPECTED_ENGINE_VER` | `'3.6.0'` | `'4.0.0'` |

The rule established in v2.8.1 (audit A-01 / B-04 / E-03 / F-01) stays in force:
`kEngineVer` moves only as a lockstep with the release tag. Wire ABI
(`kAbiVersion=2`) and schema (`kSchemaVer="2026.06"`) are unchanged.

### CLAUDE.md FROZEN marker

Rule #1 of `E:\project\CLAUDE.md` carries a formal FROZEN marker sub-bullet
effective with v4.0.0: any PR that modifies a file under
`Plugins/FrameSolver/Source/FrameCore/` must first amend CLAUDE.md to remove the
FROZEN marker (with explicit rationale) before the change is accepted.
`Plugins/FrameSolver/Source/FrameCoreUE/` is **not** in the FROZEN scope.

### Audit-driven small fixes (Phase 2 landing)

- `Dispatcher.h:102` (`kEngineVer`) — added v3.6.0 + v4.0.0 changelog comment
  blocks above the constant (Lane A A-03; v3.1-v3.3 convention restored).
- `Dispatcher.h:44-46` and `Dispatcher.h:155` — "no v3.3 plan to land" /
  "no v3.3 plan" wording (Lane E E-04) rewritten to "permanent defer under the
  v4.0.0 stable seal; the spec at `docs/specs/S6c_model_patch.md` may still be
  authored as a v4.0.x UE-side follow-up". Reflects that model.patch can land
  without touching FrameCore engine source.
- `Dispatcher.cpp:23` — "carry-over from v2.5" → "permanent defer under the
  v4.0.0 stable seal; see `docs/HANDOFF_v4.0.0.md`" (Lane E E-05).
- `Scripts/run_exit_tests.ps1` D2/D4 — "v3.6.1 follow-up" → "v4.0.x follow-up;
  FROZEN engine doesn't need it" (Lane E E-01 / E-02).
- `Scripts/run_gate.ps1:29` end-of-comment — stale `-ExpectedUeTests 68`
  (pre-v3.2) → `-ExpectedUeTests 133` + v4.0.0 stable-seal footnote
  (Lane B B-07).
- `docs/VERIFICATION.md:32` — guard-rail paragraph from v3.5.0-era
  `$ExpectedUeTests = 120` → 135 with v4.0.0 stable-seal carry-forward note
  (Lane A A-01).
- `docs/VERIFICATION.md:106` — gate table from `default 98 with cuDSS — pass
  -ExpectedUeTests 96 without` (v3.4-era stale) → `default 135 with cuDSS — pass
  -ExpectedUeTests 133 without` (Lane A A-02).
- `docs/ARCHITECTURE.md:300` — `fixtures F1…F56` → `F1…F71 (F41/F60 numbering
  gaps)` (Lane B B-08-adjacent stale).
- `docs/ARCHITECTURE.md:311-313` — `120` / `118` → `135` / `133` with v3.6 C6/C7/C8
  along-span data line note and v4.0.0 stable seal footnote (Lane B B-03 / B-10).
- `README.md:22` — status block re-anchored to v4.0.0 stable seal; v3.6.0
  status block preserved as "Prior anchor".
- `README.md:459-464` — file-tree comment from `98 + 22 = 120 w/cuDSS` →
  `60 + 75 = 135 w/cuDSS (133 without; FROZEN under v4.0.0 stable seal)`
  (Lane B B-05).

### CLAUDE.md (project root, `E:/project/CLAUDE.md`)

Adds a new "現況 (HEAD `v4.0.0`)" section above the existing v3.5.1 / v3.5.0 / …
chain (preserved as history). Rule #1 receives the FROZEN marker sub-bullet.
Rule #2 numbers re-sync (F66 → F71; 57 → 135 UE tests).

> Note: CLAUDE.md sits at `E:/project/CLAUDE.md`, one level above the git repo
> root (`E:/project/ArchSim`). It is a local-instruction file, not tracked in the
> repo; the binding FROZEN-contract text for the release is the corresponding
> excerpt in `docs/HANDOFF_v4.0.0.md`.

## Verification matrix

All gates verified on integrator host 2026-06-23 (Windows 11, MSVC vs2026
preview, UE 5.7, `framecore-direct` conda env at `%USERPROFILE%\anaconda3\envs\framecore-direct`).

| Leg | Gate | Result | Reproduce |
|---|---|---|---|
| 1 | standalone F1..F71 | **ALL PASS** (failures=0) | `$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"; Plugins\FrameSolver\Standalone\frametest.exe` |
| 2 | UE automation (`FrameCore.*`) | **135 tests performed** Automation Test Queue Empty (133 on non-cuDSS) | `"E:\project\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\project\ArchSim\ArchSim.uproject" -ExecCmds="Automation RunTests FrameCore." -TestExit="Automation Test Queue Empty" -log -unattended -nopause -nullrhi` |
| 3 | OpenSees compare | **OPENSEES GATE: PASS** (shallow-arch von Mises 6.42e-3 rel) | `python Tools\opensees_compare.py --relaxed` |
| 4 | linear_deep_audit | **PASS failures=0 checks=104** | `$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"; Plugins\FrameSolver\Standalone\linear_deep_audit.exe` |
| 5 | CLI round-trip | **ALL PASS** (failures=0) | `python Tools\cli_roundtrip.py` |
| 6 | v2 dispatcher round-trip | **ALL PASS** (kEngineVer=4.0.0 pin enforced) | `cmd /c Plugins\FrameSolver\Standalone\build_capi_v2.bat; $env:FRAMECORE_EXPECTED_ENGINE_VER='4.0.0'; python Tools\v2_roundtrip.py` |

**One-click**: `powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees`
(after `set SUPERNODAL_CONDA=%USERPROFILE%\anaconda3\envs\framecore-direct\Library`
if conda is not on PATH).

Gates NOT RUN this session:
- **`Scripts/run_gpu_gate.ps1 -Strict`** (CUDA legs F1..F67s strict + v2_roundtrip
  CUDA + r2_bench 90k) — v4.0.0 engine source delta in the CUDA path = 0 lines vs
  v3.6.0; the cuDSS host evidence carries forward
  (`r2_bench --gpu 90k margin +11.94 ms`, `F67s STRICT_EXECUTED` fingerprint).
- **Exit-test suite** (`Scripts/run_exit_tests.ps1`, D1 property sweep / D3 strict
  oracle) — v3.6.0 carry-forward (engine 0 line delta).

To refresh GPU and exit-test evidence locally:

```powershell
# GPU 6th-leg
$env:SUPERNODAL_CONDA = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library"
powershell Scripts\run_gpu_gate.ps1 -Strict

# Exit-test
powershell Scripts\run_exit_tests.ps1
```

## Honest limitations

- **`linear_deep_audit` reports 104 checks**, not the 108 a static grep of
  `addRow(` returns. The four delta is conditional `addRow` calls in code paths
  the default-build does not execute. Lane B B-01 flagged 108; the **runtime
  print** at the end of `linear_deep_audit.exe` is the authoritative number.
- **UE test count 135 / 133** is the runtime count
  ("Automation Test Queue Empty 135 tests performed"). A static grep of
  `IMPLEMENT_(SIMPLE|COMPLEX)_AUTOMATION_TEST` reports 136, but
  `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUESolveResultMarshalTest.cpp:10`
  contains the macro name in a comment line that grep over-counts. Distinct
  registered test classes = 75 (FrameCoreUE) + 60 (FrameCore) = **135**, with
  the 2 cuDSS-gated tests (`FFrameCoreGpuBacksubTest` and `…Strict…`)
  conditionally compiled.
- **GPU lane evidence carries forward**. Engine source delta in `FrameCore/` is
  zero between v3.6.0 and v4.0.0; the `cuDSS host RTX 5070 Ti` numbers from
  v2.11.0 / v3.0.0 / v3.1.0 are still the authoritative cuDSS performance
  envelope. No new GPU bench was taken this session.
- **The "policy major bump" semantic** is intentional: v4.0.0 is not a
  technical breaking change. Clients that compile against `kEngineVer=4.0.0`
  receive bit-identical engine numerics to a v3.6.0 build. The reason to bump
  the major is the stability promise.

## Breaking changes

**Breaking changes: none (API / ABI / wire / numerics).**

The v2 dispatcher wire ABI (`kAbiVersion=2`), capability list (23 capabilities
in CPU builds, 24 in CUDA builds), schema (`kSchemaVer="2026.06"`), USTRUCT
layouts, public C API (v1 + v2) and engine numerics are all unchanged from
v3.6.0. All existing clients compile and run without modification.

**Release-policy change (semantic version rationale):**

v4.0.0 is a "policy major bump." It signals three things to downstream
integrators:

1. **The engine FROZEN contract is in effect.** `Plugins/FrameSolver/Source/FrameCore/`
   will not receive algorithm changes. Any future change to the engine source
   first requires removing the FROZEN marker in CLAUDE.md / HANDOFF_v4.0.0.md
   with explicit rationale.
2. **Future patch releases (v4.0.x) are UE consumer-side only** and will not
   break the wire ABI.
3. **v3.x callers can treat v4.0.0 as a drop-in.** The only user-visible
   change is the version-string bump (the `hello.response.version` reply moves
   from `3.6.0` to `4.0.0`; clients pinning to the exact version string need to
   update their expected value).

The bump follows the principle that "intent-to-freeze" is the most important
signal to downstream integrators, even when the code delta is zero. A major
bump communicates "this is the stable long-term anchor" more clearly than a
v3.6.1 patch would.

## Deferred (permanent; carried forward from v3.6.0 HANDOFF)

These items remain permanently deferred under the v4.0.0 stable seal. None
require engine source changes to address; UE-side patches are the path forward.

- **S11 MITC9i high-order shells** — requires engine algorithm work; FROZEN
  forbids it. First action under v4.0.0 = none (locked).
- **U-08 showcase map + BP examples** — Designer's in-Editor work; `Tools/build_v3_5_showcase_map.py`
  is the starter script. v4.0.x patches may evolve it without engine impact.
- **U-09 Chaos POD GeometryCollection** — UE 5.7 Chaos destruction API churn;
  `AFrameFragmentClusterActor` ships the StaticMesh thin slice. If UE 5.8+
  Chaos stabilises, the upgrade is UE consumer-side only (no engine touch).
- **Phase 10 live `DynCollapse` callback channel** — `DynCollapseOptions::onFrameEmitted`
  callback is already in the engine. Threading into a UE subsystem is multi-cycle
  UE work; v3.5 `AFrameDynCollapseReplayActor` already delivers the user-visible
  effect.
- **D2 / D4 exit-test placeholders** — `Scripts/run_exit_tests.ps1` D1 (property
  sweep) + D3 (strict-mode re-run) are the binding dimensions. D2 (bench ladder)
  and D4 (fuzz) are permanent placeholders under FROZEN engine.
- **`model.patch` schema** (audit ID B-06) — wire spec at
  `docs/specs/S6c_model_patch.md` may still be authored as a v4.0.x UE-side
  follow-up; the dispatcher returns `NOT_IMPLEMENTED` until then.

Lane G G-01 / G-02 (`build_sn_cuda.bat` + `build_capi_v2_cuda.bat` last-resort
CUDA_ROOT hardcode `%USERPROFILE%\anaconda3\envs\framecore-direct`) and G-03
(Research/ build bats unconditional CUDA_ROOT) — defer to v4.0.x patch; the
`SUPERNODAL_CONDA` env override path takes precedence so this is a
fresh-clone-without-override convenience issue, not a release blocker.

## Tag plan

```bash
# Apply remaining hardening landed in this release-hardening cycle.
git add \
  Plugins/FrameSolver/Standalone/v2/Dispatcher.h \
  Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp \
  Plugins/FrameSolver/FrameSolver.uplugin \
  Scripts/run_gpu_gate.ps1 \
  Scripts/run_gate.ps1 \
  Scripts/run_exit_tests.ps1 \
  .github/workflows/release-gate.yml \
  README.md \
  docs/VERIFICATION.md \
  docs/ARCHITECTURE.md \
  docs/RELEASE_v4.0.0.md \
  docs/HANDOFF_v4.0.0.md

git commit -m "release: v4.0.0 -- FrameCore stable (long-term anchor; engine FROZEN)"
git tag -a v4.0.0 -m "FrameCore v4.0.0 stable -- long-term anchor; engine FROZEN"
git push origin main
git push origin v4.0.0
gh release create v4.0.0 --title "FrameCore v4.0.0 — stable" --notes-file docs/RELEASE_v4.0.0.md
```

Release URL (after `gh release create`):
<https://github.com/rocky59487/architect_simulator/releases/tag/v4.0.0>
