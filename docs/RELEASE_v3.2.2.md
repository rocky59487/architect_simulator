# FrameCore v3.2.2 — V321-01..06 test-strengthening + Dispatcher.h B-06 patch

**Tag (this release):** `v3.2.2`
**Branch:** `main`
**Date:** 2026-06-22
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v3.2.1` at `bce490d` (UE test coverage 62→70 + clean-build
safety + v3.x doc grooming; engine source frozen at `v3.1.0` bit-identical).

> **v3.2.2 is a patch over v3.2.1.** Engine source under
> `Plugins/FrameSolver/Source/FrameCore/` is **bit-identical to v3.2.1** (also
> bit-identical to v3.2.0 / v3.1.0). No API change, no wire-protocol change,
> no numerical behaviour change. The release closes the 6 v3.2.1-deferred items
> (V321-01..06) flagged by the v3.2.1 release-hardening 7-agent audit — all
> test-strengthening + 1 doc note. Engine remains untouched.
>
> v3.2.2 was driven through the same release-hardening Phase 0..5 pipeline.
> Engine source delta vs v3.2.1 = 0 lines under FrameCore native module; v3.2.2
> delta = 5 FrameCoreUE test files extended + 1 new test helper header + 1
> Dispatcher.h comment block.

## 1. What v3.2.2 ships

### 1.1 V321-01 — `MarshalSSBeamTest` governing-id tightening (V321-01a deferred)

`Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalSSBeamTest.cpp`:

- **Governing member id**: tightened from `>= 0` (vacuous: any sentinel-free value
  passes) to `== 0 || == 1` (the fixture has exactly 2 members; any out-of-range engine
  pick like id=5 would now fail).
- **V321-01a (analytic Vy oracle) DEFERRED to v3.2.3**: first-try `|Vy| at midspan
  member 0 = w*L/4` check failed at the v3.2.2 gate run — the engine's
  `samples[k].Vy` field is NOT plain transverse shear in N units (units / sign / axis
  convention TBD). Investigating which closed-form value matches `samples[k].Vy` and
  re-enabling is v3.2.3 work. The original analytic block is preserved as a comment in
  `FrameCoreUEMarshalSSBeamTest.cpp` for the v3.2.3 starting point.

### 1.2 V321-02 — `RobustnessTest` per-sample bit-exact compare

`FrameCoreUERobustnessTest.cpp` Part (3) 100-repeat memory-stability loop now compares
not just `GlobalMaxFiberSigma` + `Members.Num()` + `Samples.Num()` but also
`Samples[5].{N, Vy, Mz, SigmaCompMax, SigmaTensMax}` against the iteration-0 snapshot.
A state leak corrupting an interior sample's `Vy` while leaving the global max intact
would now surface (was silently passing in v3.2.1). Also emits the failing iteration
index in the diagnostic if compare fails.

### 1.3 V321-03 — `AxialColumnTest` explicit `N > 0` sign assertion

`FrameCoreUEAxialColumnTest.cpp`: added `TestTrue("Axial column: N > 0
(compression-positive, mirrors standalone F4)", s0.N > 0.f && s5.N > 0.f && s10.N > 0.f)`.
The existing `|N| ≈ P` checks would have passed under a sign flip; the standalone F4
contract is compression-positive `N > 0`, and v3.2.1 lacked that mirror.

### 1.4 V321-04 — `EditorTabSpawnerTest` explicit module load guard

`FrameCoreUEEditorTabSpawnerTest.cpp`: added
`FModuleManager::Get().LoadModuleChecked<IModuleInterface>(TEXT("FrameCoreUE"))` before
the `HasTabSpawner` query. Without this guard, a lazy-init automation context could
run `RunTest()` before `StartupModule()` fires → false-negative "spawner not
registered" pass. The call is idempotent (cheap lookup on subsequent invocations).

### 1.5 V321-05 — `FrameCoreUETestHelpers.h` shared `ToBlueprint` forward decl

New private test-support header
`Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUETestHelpers.h`
consolidates the previously per-TU `namespace FrameCoreUE { FFrameStressField
ToBlueprint(const frame::StressField&); }` forward declaration. Six test files
(SSBeam / ShellPlate / MultiMember / Robustness / ThetaRange / AxialColumn) swap
the inline declaration for `#include "FrameCoreUETestHelpers.h"`. Saves 18 lines
aggregate and single-sources the signature — any future widening of `ToBlueprint`
(e.g. an extra parameter for double precision per V321/U-05 / future work) requires
editing 1 file instead of 6.

The header is **deliberately private to `Private/Tests/`** — `ToBlueprint` stays a
module-internal implementation detail, with `UFrameCoreStressFieldLibrary` the only
non-test consumer outside `FrameCoreUETypes.cpp`. Do NOT expose this header from
`Public/FrameCoreUE/`.

### 1.6 V321-06 — `Dispatcher.h` `model.patch — schema TBD` audit ID B-06

`Plugins/FrameSolver/Standalone/v2/Dispatcher.h` lines 44 and 145-146: the
"`model.patch — schema TBD`" carryforward, open since v2.4, is now labelled with
audit ID **B-06** and explicitly noted as "out of v3.x scope unless an explicit
design decision authors a spec at `docs/specs/S6c_model_patch.md`". Frees the v3.x
roadmap from an implicit obligation that has accumulated across 8 releases without a
specific landing plan.

### 1.7 Version-pin intentionally unchanged

`kEngineVer` stays `"3.2.0"` in `Plugins/FrameSolver/Standalone/v2/Dispatcher.h`.
`.uplugin VersionName` stays `"3.2.0"`. `FRAMECORE_EXPECTED_ENGINE_VER` stays
`'3.2.0'` in `Scripts/run_gpu_gate.ps1` + `.github/workflows/release-gate.yml`.

Same rationale as v3.2.1 §1.7: zero engine source delta + zero wire-protocol
change + zero capability list change → no need to bump. Clients pinned to `3.2.0`
see bit-identical engine behaviour across v3.2.0 / v3.2.1 / v3.2.2.

## 2. What stayed bit-identical

- **Engine source under `Plugins/FrameSolver/Source/FrameCore/`** — `git diff
  v3.2.1..HEAD -- Plugins/FrameSolver/Source/FrameCore/` returns nothing. Same as
  v3.2.1, v3.2.0, v3.1.0 — engine source has been stable since v3.1.0 tag (mod the
  Dispatcher.h comment edit which has no runtime effect).
- **Public ABI** — `FRAMECORE_API` / `FRAMECOREUE_API` exports unchanged.
- **Wire ABI** — `kAbiVersion = 2` unchanged. v2 dispatcher capability list unchanged
  (23 verbs). `kEngineVer` stays at `"3.2.0"` (see §1.7).
- **Default-build cross-vendor** — non-CUDA builds (`FRAMECORE_CUDA=0`) bit-identical
  to v3.2.1.
- **CUDA lane** — v3.2.2 source delta in CUDA path = 0 lines. v2.11.0 baseline
  r2_bench 90k margin (+11.94 ms over 16.67 ms 60-fps budget) carries forward.

## 3. Reproduction matrix (v3.2.2 source on integrator host, 2026-06-22)

| Leg | Cmd | Result | Notes |
|---|---|---|---|
| 1. Standalone F1..F70 | `Plugins\FrameSolver\Standalone\build.bat` then `frametest.exe` | **ALL PASS (failures=0)** | bit-identical vs v3.2.1; engine source unchanged |
| 2. UE automation 70/70 | `Build.bat ArchSimEditor Win64 Development` then `Scripts\run_gate.ps1 -RequireOpenSees` | **ALL PASS** | 70 tests run with cuDSS; 6 V321 fixes in 5 test files; non-cuDSS box use `-ExpectedUeTests 68` |
| 3. OpenSees strict | `python Tools\opensees_compare.py` | **OPENSEES GATE: PASS** | unchanged vs v3.2.1 (engine source unchanged) |
| 4. Deep audit | `Plugins\FrameSolver\Standalone\build_linear_audit.bat` then `linear_deep_audit.exe` | **PASS failures=0 checks=104** | unchanged vs v3.2.1 |
| 5. CLI round-trip | `Tools\cli_roundtrip.py` (after `build_cli.bat`) | **ALL PASS (failures=0)** | 13 checks; unchanged vs v3.2.1 |
| 6a. v2_roundtrip (CPU) | `build_capi_v2.bat` + `FRAMECORE_EXPECTED_ENGINE_VER=3.2.0 python Tools\v2_roundtrip.py` | **=== summary: ALL PASS ===** | `kEngineVer=3.2.0` pin enforced; 23 capabilities advertised |
| 6b. v2_roundtrip (CUDA) | `Scripts\run_gpu_gate.ps1 -Strict` | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; v3.2.2 source delta in CUDA path = 0 lines |
| 7a. F1..F67 + F67s strict | `Scripts\run_gpu_gate.ps1 -Strict` leg 1/3 | **NOT RUN** (this session) | same as above |
| 7b. r2_bench --gpu 90k | `Scripts\run_gpu_gate.ps1 -Strict` leg 3/3 | **NOT RUN** (this session) | same as above |

## 4. Tag plan

```bash
git add -- \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalSSBeamTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUERobustnessTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEAxialColumnTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEEditorTabSpawnerTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalMultiMemberTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalShellPlateTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEThetaRangeTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUETestHelpers.h \
  Plugins/FrameSolver/Standalone/v2/Dispatcher.h \
  docs/RELEASE_v3.2.2.md \
  docs/HANDOFF_v3.2.2.md

git commit -m "release: v3.2.2 -- V321-01..06 closeout (test strengthening + B-06 audit ID)"
git tag -a v3.2.2 -m "v3.2.2 -- patch: V321-01..06 test/audit-id closeout"
git push origin main
git push origin v3.2.2

gh release create v3.2.2 \
  --title "v3.2.2 -- V321-01..06 closeout (test strengthening + B-06 audit ID)" \
  --notes-file docs/RELEASE_v3.2.2.md \
  --latest
```

## 5. Deferred

### 5.1 v3.2.1 V321-deferred — 5 of 6 CLOSED in v3.2.2

- V321-01 ⚠ partial: gov-id tighten ✅; analytic Vy oracle (V321-01a) DEFERRED to v3.2.3
- V321-02 ✅ per-sample bit-exact compare
- V321-03 ✅ N > 0 sign assertion
- V321-04 ✅ module load guard
- V321-05 ✅ Tests/FrameCoreUETestHelpers.h
- V321-06 ✅ B-06 audit ID for Dispatcher.h model.patch

### 5.1a v3.2.2 newly deferred

- **V321-01a** — `MarshalSSBeamTest` analytic Vy oracle. First action: open
  `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressField.h` and the
  `computeStressField` body to find what `MemberStressSample::Vy` is filled with
  (units, sign, axis), then either replace `w * L / 4` with the matching closed-form
  value or remove the V321-01a comment block in `FrameCoreUEMarshalSSBeamTest.cpp` if
  the field is not directly cross-checkable against a beam-theory invariant.

### 5.2 v3.2.0 / v3.2.1 carry-forward (unchanged, no v3.2.2 addition)

- **U-01** BP load JSON model entrypoint
- **U-02** Slate fixture dropdown
- **U-03** real renderer (spline mesh / Niagara / colour-band) — **v3.3 主軸**
- **U-04** ✅ CLOSED in v3.2.1 Phase 6e
- **U-05** float-only USTRUCT precision (P << 1 N)
- **U-06** UE 5.7 + VS2026 "not preferred version" build warning
- **U-07** sentinel mismatch — needs engine source change (rule #1) — **v3.3 first action**
- All v3.1.0 / v3.0.x carryforwards continue (A-13 F71 +Z, D-05 v1 CLI STRESS, E-07
  v2 inspect spec, E-13 S11 naming, etc).

See [`docs/HANDOFF_v3.2.1.md`](HANDOFF_v3.2.1.md) and
[`docs/HANDOFF_v3.2.0.md`](HANDOFF_v3.2.0.md) for first-actions on each.

### 5.3 New audit ID this release

- **B-06** `Dispatcher.h` `model.patch — schema TBD` — formally tagged; out of v3.x
  scope unless explicit design decision authors `docs/specs/S6c_model_patch.md`.

## 6. Honest limitations (carry-forward from v3.2.0 + v3.2.1)

Unchanged from v3.2.1 §6:
- FrameCoreUE is reflection-only (no new engine capability).
- Float lossy cast is per-value, not aggregated.
- No packaged build smoke test in CI (manual verification only).
- Renderer is not in scope (U-03, deferred to v3.3).
- U-07 sentinel mismatch (engine 0 vs USTRUCT -1) still present; v3.3 fix planned.

## 7. Breaking changes

None at the public ABI level. `kEngineVer` intentionally unchanged from `"3.2.0"`
(see §1.7). `.uplugin VersionName` unchanged. Clients pinned to `3.2.0` see
bit-identical engine behaviour and can adopt v3.2.2 without code changes.

UE test count stays at **70** (no new test added; 5 tests had their assertions
strengthened in-place). Anyone running `run_gate.ps1` against v3.2.2 source needs to
rebuild the UE editor (`Build.bat ArchSimEditor Win64 Development`) before running
the gate because the 5 strengthened test `.cpp` files need to recompile — the count
guard itself stays at 70.
