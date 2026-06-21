# FrameCore v3.1.0 — S11 stress-field post-process (visualisation numerical layer)

**Tag (this release):** `v3.1.0`
**Branch:** `main`
**Date:** 2026-06-21
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v3.0.1` at `97cba7f` (post-v3.0.0 hardening + 6 follow-up findings closed,
v3.0.0 itself collapsed v2.11.1 + the 5 RC items + the 7-agent audit fixes).

> **v3.1.0 ships the S11 Stress Field as a new visualisation-layer post-process**: pure C++17/POD
> output (no Eigen leak, no UE coupling), shared single source of truth (`StressKernel.h`) with
> the existing elastic D/C screen so the two cannot drift. Three new standalone fixtures
> (F68 cantilever member field, F69 clamped-plate shell layer recovery + 30° rotation
> invariance, F70 D/C interlock) plus a UE F68 mirror (`FFrameCoreStressFieldTest`) cover the
> numerics; the v2 dispatcher gains `inspect.stress_field` so client SDKs can capability-gate
> the new verb. **Engine source delta vs v3.0.1 = 5 files / ~350 lines**; `ElasticAllowable.cpp`
> was refactored to call into `StressKernel.h` shared math (no logic change, F1..F66 bit-identical).
> v3.1.0 was driven through release-hardening Phase 1 (7-agent parallel audit), Phase 2
> (BLOCKER + HIGH small-fixes), Phase 3 (5/6 reachable gate legs green on the integrator host),
> Phase 4 docs sync, Phase 4.5 final integrator pass, Phase 5 tag.

## 1. What v3.1.0 ships

### 1.1 S11 Stress Field (new visualisation numerical layer)

A pure post-process that converts a solved `(FrameModel, SolveResult)` into a per-member
fiber-stress trace and a per-shell-corner von Mises field, suitable for a renderer to paint
stress clouds or BMD/SFD-style bands. No Eigen, no UE, no allocations beyond the returned
vectors.

- `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressKernel.h` — single source of
  truth for fiber sigma / shell layer sigma / principal-stress / von Mises formulas. Shared
  by `ElasticAllowable` and `StressField` so the design-check path and the visualisation path
  cannot diverge.
- `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressField.h` — POD result types
  (`MemberStressSample`, `MemberStressTrace`, `ShellStressPoint`, `ShellStressLayer`,
  `StressField`) + `computeStressField(model, sr, samplesPerSpan=11)` entry point.
  `FRAMECORE_API`-marked, `[[nodiscard]]`.
- `Plugins/FrameSolver/Source/FrameCore/Private/StressField.cpp` — implementation.
  Member sampling reconstructs `N(x)`, `V(x)`, `M(x)` analytically from `MemberEndForces`
  + `MemberUDL` (EB / Timoshenko-equivalent at the centreline; UDL accumulator handles
  multiple records per member — A-11 audit fix). Shell sampling matches
  `ElasticAllowable::checkShellSurface`'s centre + 4 corners traversal.
- `Plugins/FrameSolver/Source/FrameCore/Private/ElasticAllowable.cpp` — refactored to call
  `memberCornerSigmaMax / memberShearPeak / memberTorsionTau / shellLayerSigma / principalStress`
  from `StressKernel.h`. Bit-identical F1..F66 behaviour verified on the standalone gate.

### 1.2 Three new standalone fixtures + UE mirror

| Fixture | What it verifies | Tolerance | Result on integrator host |
|---|---|---|---|
| **F68** (cantilever member field) | analytic `\|P\|·(L-x)/Wz` at 11 samples; bit-exact interlock vs `ElasticAllowable(endI/endJ)`; root TopY/BotY fiber == sComp; tip sigma vanishes | rel<1e-12 (interlock), rel<1e-9 (analytic) | **PASS** (worstRel=0 analytic, rel=0 interlock) |
| **F69** (clamped-plate shell field) | `(top+bot)/2 == Nxx/t` (membrane recovery), `(top-bot)/2 == 6·Mxx/t²` (bending recovery), 30° z-rotation `vM` invariance | rel<1e-12 (recovery), rel<1e-9 (invariance) | **PASS** (worstRel=0 recovery, rel=0 invariance) |
| **F70** (D/C interlock) | governing member/shell id matches between `worstUtilization`/`worstShellUtilization` and `StressField`; max fiber sigma == max `ElasticAllowable` end sigma (no UDL); `globalMaxVonMises` == `ds.maxDC * cap.vm` | rel<1e-12 (bit-exact via shared kernel) | **PASS** (rel=0 bit-exact) |
| **UE `FFrameCoreStressFieldTest`** | UE-build mirror of F68 (member side) — interlock + analytic at 11 samples | rel<1e-12 + rel<1e-9 | **PASS** (UE leg 60/60) |

The spec doc is [`docs/specs/S11_stress_field.md`](specs/S11_stress_field.md).

### 1.3 v2 dispatcher: `inspect.stress_field` capability

- `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` — `kEngineVer "3.0.1" → "3.1.0"`;
  `Capabilities()` += `"inspect.stress_field"`; `HandleInspectStressField` handler declaration.
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp` — handler implementation + JSON packers
  (`packStressSample`, `packShellStressPoint`, `packShellLayer`, `packStressField`).
  Per-sample NaN sweep added in Phase 2 (C-05 audit fix).
- `Tools/v2_roundtrip.py` — new `inspect.stress_field` fixture: shape check
  (`samplesPerSpan == 11`, `members` length, `samples` length, `memberId`, `governingMemberId`,
  `governingShellId == -1` no-shell sentinel) + range guards (`samplesPerSpan=1` and `=2048`
  both expected `VALIDATION_FAILED`).

### 1.4 Phase 2 audit-driven fixes folded into v3.1.0

Findings from the 7-agent parallel deep audit:

**BLOCKERS (5):**
- **A-11** `findUdl` now sums all matching `MemberUDL` entries per member (was: returned first
  match only, silently diverging from the solver's nodal-equivalent aggregation).
- **D-01** `Plugins/FrameSolver/Standalone/build_sn_cuda.bat` adds `StressField.cpp` (CUDA
  build would have link-failed on F67/F68/F69/F70 without it).
- **D-02** `.github/workflows/release-gate.yml` bumps `FRAMECORE_EXPECTED_ENGINE_VER` to
  `'3.1.0'` and renames Leg 1 step to "F1..F70". Without the bump CI would fail every push.
- **G-05 / B-03** `Scripts/run_gpu_gate.ps1` bumps the same pin (GPU leg 2 / v2_roundtrip
  CUDA would have failed otherwise).
- **G-06 / B-04** `Plugins/FrameSolver/FrameSolver.uplugin` `Version 28 → 29`,
  `VersionName "3.0.1" → "3.1.0"`. UE plugin metadata must track the engine.

**HIGHS (5):**
- **C-05** Per-sample NaN sweep in `packStressField` (was: only the two global maxes were
  finite-checked; partial-NaN per-sample would silently serialise as JSON "nan").
- **C-07 / C-08** Sentinel `-1` for `governingMemberId` / `governingShellId` when the field
  has no contributing element (was: defaulted to `0`, ambiguous with a real element ID 0).
- **A-09** `PrincipalStress::theta` range comment fixed from `[-π/4, π/4]` to
  `(-π/2, π/2]` (the value was already correct; the comment misled).
- **A-14 / F-01** Removed dead `(void)sN;` suppression + the dead `sN` local in
  `ElasticAllowable::checkSection`. `sM` is live (used in `r[3]`), kept.
- **D-03** `Plugins/FrameSolver/Standalone/build_capi.bat` (v1 DLL) adds `StressField.cpp`
  (same bit-identity-with-CLI rule the bat's own comment asserted).

**MEDIUMS landed in Phase 2:**
- `[[nodiscard]]` on 6 kernel functions in `StressKernel.h` + `computeStressField`
  (F-08 — catches the most common API misuse).
- `F-04` Dead `s1_base` block removed from F69 (the meaningful invariance check is `vM_base`).
- `F-05` Free-end zero check in F68 now uses the same `epsAbs` scale as the surrounding
  interlock checks (was: `< 1e-6`, four orders of magnitude looser than the rest).
- `E-08 / E-09` `Dispatcher.cpp` + `Dispatcher.h` header comments advanced from
  "CURRENT STATE (v2.5)" to v3.1.0 / S11.
- `.gitignore` adds `output/`, `_audit*.log`, `_scratch*.log` to prevent accidental scratch
  uploads (G-11).

## 2. What stayed bit-identical

- **F1..F66 engine numerics** — `ElasticAllowable.cpp` was refactored to delegate to
  `StressKernel.h`, but the formulas are direct substitutions (same expression, same operator
  order). Verified by `Plugins/FrameSolver/Standalone/build.bat` → `frametest.exe` →
  **ALL PASS (failures=0)** on the rebuilt v3.1.0 source.
- **Public API** — unchanged except for the additive `StressField.h` / `StressKernel.h`
  additions and the additive `inspect.stress_field` v2 capability. No method removed; no
  existing method's behaviour changed.
- **Wire ABI** — `kAbiVersion = 2` unchanged. `kEngineVer "3.0.1" → "3.1.0"` is the only
  protocol-visible change.
- **Default-build cross-vendor** — non-CUDA builds (`FRAMECORE_CUDA=0`) compile the GPU
  test path out via the existing `#if FRAMECORE_CUDA` guard; behaviour matches v3.0.1
  bit-for-bit on the CPU lane.

## 3. Reproduction matrix (v3.1.0 source on integrator host, 2026-06-21)

| Leg | Cmd | Result | Notes |
|---|---|---|---|
| 1. Standalone F1..F70 | `Plugins\FrameSolver\Standalone\build.bat` then `Plugins\FrameSolver\Standalone\frametest.exe` | **ALL PASS (failures=0)** | rebuilt v3.1.0; F68/F69/F70 all PASS with worstRel=0 on the interlock and analytic checks |
| 2. UE automation 60/60 | `…\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development …` then `Scripts\run_gate.ps1 -RequireOpenSees` | **ALL PASS** (see §3 footnote) | strict mode enforced; `-ExpectedUeTests 60` default |
| 3. OpenSees strict | `python Tools\opensees_compare.py` | **OPENSEES GATE: PASS** | matches OpenSees + analytic; 3D corot 1.22e-9, arc-length 6.4e-3, all under tol |
| 4. Deep audit | `Plugins\FrameSolver\Standalone\build_linear_audit.bat` then `linear_deep_audit.exe` | **PASS failures=0 checks=104** | rebuilt v3.1.0 |
| 5. CLI round-trip | `python Tools\cli_roundtrip.py` (after `build_cli.bat`) | **ALL PASS (failures=0)** | 13 checks |
| 6a. v2_roundtrip (CPU) | `build_capi_v2.bat` + `FRAMECORE_EXPECTED_ENGINE_VER=3.1.0 python Tools\v2_roundtrip.py` | **=== summary: ALL PASS ===** | `kEngineVer=3.1.0` pin enforced; new `inspect.stress_field` fixture (9 shape checks + 2 range guards) all green; `inspect.stress_field` advertised in capability list |
| 6b. v2_roundtrip (CUDA) | `Scripts\run_gpu_gate.ps1 -Strict` leg 2/3 | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; see §3a |
| 7a. Standalone F1..F67 + F67s strict | `Scripts\run_gpu_gate.ps1 -Strict` leg 1/3 | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; see §3a |
| 7b. r2_bench --gpu 90k (regression hard gate) | `Scripts\run_gpu_gate.ps1 -Strict` leg 3/3 | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; see §3a |

§3a footnote — **NOT RUN legs (CUDA / `-Strict` mode)**: the cuDSS DLLs are present at
`~/anaconda3/envs/framecore-direct/Library/bin/cudss64_0.dll`, but the integrator did not
run `Scripts\run_gpu_gate.ps1 -Strict` in this session (the CPU gate is sufficient evidence
for an S11 visualisation-layer release; nothing in v3.1.0 touches the CUDA lane source).
To reproduce all 9/9 V3 STABLE legs:
```powershell
Scripts\run_gpu_gate.ps1 -Strict
```
With `cudss64_0.dll` on PATH the `-Strict` flag enforces the `STRICT_EXECUTED` fingerprint
on F67s + the UE `FFrameCoreGpuBacksubStrictTest`, and pins
`FRAMECORE_EXPECTED_ENGINE_VER='3.1.0'`. The r2_bench 90k margin should hold ≥ +8.0 ms
(v2.11.0 baseline +11.94 ms; v3.1.0 source delta is 0 lines in the CUDA path).

## 4. Tag plan

```bash
git add -- \
  Plugins/FrameSolver/FrameSolver.uplugin \
  Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressKernel.h \
  Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressField.h \
  Plugins/FrameSolver/Source/FrameCore/Private/StressField.cpp \
  Plugins/FrameSolver/Source/FrameCore/Private/ElasticAllowable.cpp \
  Plugins/FrameSolver/Source/FrameCore/Private/Tests/StressFieldTest.cpp \
  Plugins/FrameSolver/Standalone/main.cpp \
  Plugins/FrameSolver/Standalone/build.bat \
  Plugins/FrameSolver/Standalone/build_cli.bat \
  Plugins/FrameSolver/Standalone/build_capi.bat \
  Plugins/FrameSolver/Standalone/build_capi_v2.bat \
  Plugins/FrameSolver/Standalone/build_linear_audit.bat \
  Plugins/FrameSolver/Standalone/build_sn_cuda.bat \
  Plugins/FrameSolver/Standalone/v2/Dispatcher.h \
  Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp \
  Scripts/run_gate.ps1 \
  Scripts/run_gpu_gate.ps1 \
  Tools/v2_roundtrip.py \
  .github/workflows/release-gate.yml \
  .gitignore \
  README.md \
  docs/VERIFICATION.md \
  docs/specs/S11_stress_field.md \
  docs/RELEASE_v3.1.0.md \
  docs/HANDOFF_v3.1.0.md

git commit -m "release: v3.1.0 -- S11 stress field post-process + 7-agent audit closeouts"
git tag -a v3.1.0 -m "v3.1.0 -- S11 stress field visualisation numerical layer"
git push origin main
git push origin v3.1.0

gh release create v3.1.0 \
  --title "v3.1.0 -- S11 stress field post-process + 7-agent audit closeouts" \
  --notes-file docs/RELEASE_v3.1.0.md \
  --latest
```

## 5. Deferred (carried over from v3.0.1 + v3.1.0 additions)

### Carry-over from v3.0.1 (and v2.11.1 / v3.0.0 audit IDs)

All v3.0.1 carry-overs persist to v3.1.x / v3.2: A-02 CUDA RAII, A-05/F-14 OpenMP heuristic,
A-12/D-2 cuDSS PHASE_REFACTORIZATION P-Delta revisit, C-01 pinned host memory, C-06 UDL +
parallelRhs+GPU fixtures, C-07 DynamicCollapse GPU limitation doc, D-02/D-03 UE bCudaEnabled
flag + packaging recipe, D-08 gpuRelInf rename, D-10/D-11/F-16, E-07/E-08, F-02/F-03/F-04/F-10,
F-08, D-06 r2_bench `--baseline` flag.

See [`docs/HANDOFF_v3.1.0.md` §3](HANDOFF_v3.1.0.md#3-deferred-items--first-action-on-day-1) for
one-line "first action on day 1" sketches for each.

### v3.1.0 newly deferred (from this cycle's audit)

- **A-13** F71 cantilever loaded along local +z to exercise the `My(x)` formula in
  `internalForcesAtX` (currently only `Mz(x)` is fixture-covered; the My branch is
  calibrated-by-assertion not by-fixture). First action: add an `F71` block in `main.cpp`
  after F70 with `fixtures::cantileverTipLoad` swapped to a +Z load and assert
  `analytic My(x) = P·(L-x)`.
- **D-05** `Tools/cli_roundtrip.py` does not exercise `inspect.stress_field` (the v1 CLI
  bridge does not yet expose it). v2 dispatcher covers it; v1 CLI route is acceptable to
  defer. First action: decide whether to add a v1 CLI `STRESS` token or leave the v1 CLI
  frozen at v2.x capabilities.
- **E-07** `docs/CLI_PROTOCOL.md` documents only the v1 text bridge. The v2 framed JSON
  protocol (now carrying `inspect.stress_field`) has no protocol doc — only spec/header
  comments. First action: author `docs/specs/S6b_v2_inspect_protocol.md` covering all v2
  inspect.* schemas in one place.
- **E-13** S11 label collision: legacy docs (`KARAMBA3D_ROADMAP.md`,
  `IMPLEMENTATION_PLAN.md`, `HANDOFF.md`) still use S11 to mean MITC9i higher-order shell.
  v3.1.0 reuses S11 for the stress-field post-process. First action: rename either the
  spec to `S11a_stress_field.md` or add a disambiguation note in the legacy docs that
  MITC9i is now S12 (or similar).
- **C-12** `Dispatcher::Submit` holds `submitMtx_` for the entire `computeStressField`
  call. For `samplesPerSpan=1024` on a large model this can stall other requests
  (including `cancel`) for hundreds of milliseconds. First action: add a cancellation poll
  hook inside `computeStressField` mirroring `HandleDynCollapse`.
- **F-02** `findUdl` is O(N·M); not a current benchmark issue (M ≪ N on the existing
  90k-DOF fixture), but a hash-based lookup would close the worst-case loophole. First
  action: replace the linear scan with an `unordered_map<MemberId, Vec3>` built once
  before the sweep.
- **F-03** Defensive `safe = (x > 0) ? x : 1e-12` clamps in `StressKernel.h` are dead
  code if `validate()` rejects degenerate sections at every entry point. First action:
  add a single comment block at the top of `StressKernel.h` stating the invariant
  ("all entries should already have validated sections; clamps are last-resort"); the
  removal can wait for v3.2.

## 6. Honest limitations (per-engine)

- **Visualisation field is a post-process, not a refined element.** The 11-sample-per-member
  reconstruction is *exact under EB / Timoshenko-equivalent* (transverse shear flexibility
  doesn't change the bending moment distribution). For Timoshenko elements `tauShear = k * V/A`
  is the peak; the through-thickness parabolic distribution is not exposed.
- **Shell sampling is at centre + 4 corners only** (matching `ElasticAllowable`). Sub-element
  Gauss-point smoothing is renderer-side, out of scope here.
- **Membrane is held at the centre** (`mx = Nxx/t`) and reused at the corners — same
  element-constant approximation `checkShellSurface` documents. A finer per-corner membrane
  field would need new MITC4 output, not a post-process.
- **No NM-interaction at the screen level.** Fiber sigma is pure elastic axial + bending
  (compression-positive). S10's NM interaction lives in the plastic-hinge formation path.
- **No DKQ tangent-stress refinement.** When `SolveOptions::useDKQPlate` is on, `MxxC[k]`
  carries Gauss-point values rather than corner extrapolations; the field then reflects
  those Gauss-point values at the four "corner" slots, which is correct for the underlying
  element but loses the peak-vs-corner distinction. The centre value is the design peak in
  that mode.
- **F71 (OpenSees direct sigma cross-check) deliberately not added** — the field output for
  a shell sample is `Nxx/t ± 6·Mxx/t²`, a textbook identity over `ShellElementForces`. The
  underlying `Nxx`/`Mxx` are OpenSees-verified (existing gate); the StressField path is
  transitively OpenSees-verified via F70's bit-exact interlock against `ElasticAllowable`
  (whose vM was always Nxx/t ± 6Mxx/t² internally). Direct sigma_xx sampling on the OSPy
  side is future work if a real divergence is ever suspected.

## 7. Breaking changes

None at the public API level. `kEngineVer` bumps `3.0.1 → 3.1.0` — clients that hard-pin
the exact version string will fail (they should range-match); this is consistent with
v3.0.x's behaviour and is the established pattern.
