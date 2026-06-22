# FrameCoreUE Quick-Start (v3.2.x)

> 5-minute guide for using the `FrameCoreUE` reflection module (introduced v3.2.0,
> hardened in v3.2.1) in Blueprint, editor utility code, or your own UE module.
> Engine numerics are NOT covered here — see [`docs/ARCHITECTURE.md`](ARCHITECTURE.md)
> for the analytical foundations. Test surface area: 10 `FrameCore.UE.*` tests across
> the marshal layer, robustness, theta-range, zero-load, axial-column, and editor tab
> spawner (v3.2.1 Phase 6 a-h additions on top of v3.2.0's two smoke tests).

## Prerequisites (before any of the use cases below works)

1. **UE 5.7 install** at `$env:UE_ENGINE_ROOT` (or edit the example commands to point
   at your install).
2. **Editor module built**:
   ```bat
   "%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development ^
       -project="%cd%\ArchSim.uproject" -waitmutex
   ```
   `run_gate.ps1` does NOT build UE — adding a new test or pulling a new commit
   requires this incremental rebuild first; otherwise the count guard short-falls.
3. **conda env `framecore-direct`** for native deps (OpenBLAS / METIS, optionally cuDSS).
   See [`docs/HANDOFF_v3.2.0.md` §2](HANDOFF_v3.2.0.md) for the PATH-prepend recipe.

## What FrameCoreUE is

`Plugins/FrameSolver/Source/FrameCoreUE/` is a thin UE-side reflection layer over the
existing `FRAMECORE_API computeStressField` engine entry point. It is purely
consumer-side: nothing under `Plugins/FrameSolver/Source/FrameCore/` was touched.

The module exposes three things:

1. **USTRUCT(BlueprintType) mirrors** of `frame::StressField` POD types —
   `FFrameStressField`, `FFrameMemberStressTrace`, `FFrameStressFieldSample`,
   `FFrameShellStressLayer`, `FFrameShellStressPoint`. All fields are
   `BlueprintReadOnly` (one-way marshal); floats are `float32` (the engine's
   `frame::real` is `double`; lossy cast is intentional for visualisation tolerance,
   see *Precision budget* below).
2. **`UFrameCoreStressFieldLibrary`** — a `UBlueprintFunctionLibrary` with one BP
   demo entry (`ComputeCantileverFixture`) plus five `BlueprintPure` accessors
   (`GetGoverningMemberId`, `GetGoverningShellId`, `GetGlobalMaxFiberSigma`,
   `GetGlobalMaxVonMises`, `GetMemberSamples`).
3. **`SFrameCoreStressFieldPanel`** (Editor-only, `#if WITH_EDITOR`) — a Slate
   utility panel registered as a nomad tab under the editor's WorkspaceMenu →
   Tools section. Click "Compute Cantilever Stress Field" to populate.

## Use case 1 — Blueprint graph

```
[Compute Cantilever Fixture] -- P=1000 N, L=2000 mm, SamplesPerSpan=11 → [FFrameStressField result]
       result → [Get Global Max Fiber Sigma] → [Print Text]
       result → [Get Governing Member Id]   → [Print Text]
       result → [Get Member Samples] → ForEach → [FFrameStressFieldSample.X / SigmaCompMax / Vy / Mz]
```

The fixture is hard-coded as a 2 m, 100x100 mm rectangular section cantilever with a
tip load along local +z. For now there is **no** "load JSON model" entry (that is
the v3.3 U-01 deferred item).

## Use case 2 — Editor utility panel

Open Unreal Editor → Window menu → look under "FrameCore Stress Field" (added under
the standard WorkspaceMenu Tools group by `FFrameCoreUEModule::StartupModule`). The
panel has a Compute button, a result text block, and a `SListView` of the 11
samples. Useful for verifying engine output without writing test code.

`FFrameCoreUEEditorTabSpawnerTest` (UE automation) sanity-checks the spawner
registration. If WorkspaceMenuStructure ever moves API, the test fails (rather than
the menu silently disappearing).

## Use case 3 — Native UE module / native C++ test

Add `"FrameCoreUE"` to your module's `Build.cs` `PublicDependencyModuleNames` and
include `"FrameCoreUE/FrameCoreUELibrary.h"`. Then:

```cpp
const FFrameStressField Field =
    UFrameCoreStressFieldLibrary::ComputeCantileverFixture(1000.f, 2000.f, 11);
const int32 GovMemberId = Field.GoverningMemberId;
const float MaxSigma    = Field.GlobalMaxFiberSigma;
for (const FFrameStressFieldSample& Sample : Field.Members[0].Samples)
{
    UE_LOG(LogTemp, Display, TEXT("x=%.1f sigComp=%.3f"),
           Sample.X, Sample.SigmaCompMax);
}
```

If you want the engine POD directly (no marshal copy), add `"FrameCore"` as a
public dep and include `"FrameCore/StressField.h"` / `"FrameCore/FrameSolver.h"`:

```cpp
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/StressField.h"

frame::FrameModel m = /* build inline */;
frame::SolveResult r  = frame::solve(m);
frame::StressField fld = frame::computeStressField(m, r, 11);
// Then marshal to USTRUCT yourself if you need BP exposure:
#include "FrameCoreUE/FrameCoreUETypes.h"
namespace FrameCoreUE { FFrameStressField ToBlueprint(const frame::StressField&); }
const FFrameStressField bp = FrameCoreUE::ToBlueprint(fld);
```

The five existing marshal tests in
`Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/` are good templates:

- `FrameCoreUEBlueprintSmokeTest.cpp` — cantilever / Library entry
- `FrameCoreUEMarshalSSBeamTest.cpp` — simply-supported beam under UDL
- `FrameCoreUEMarshalShellPlateTest.cpp` — clamped plate (shells-only model)
- `FrameCoreUEMarshalMultiMemberTest.cpp` — 3-member frame with user IDs 100/200/300
- `FrameCoreUERobustnessTest.cpp` — negative inputs / scaling / 100x repeat

## Precision budget

The marshal layer copies engine `double` values into USTRUCT `float` fields. Cross-check
against the engine POD at a representative sample:

| Path | Tolerance | Why |
|------|-----------|-----|
| Engine POD (`frame::computeStressField`) | rel<1e-12 vs analytic (F68 / F70) | bit-exact under EB / Timoshenko |
| USTRUCT vs engine POD bit-pattern | rel<1e-5 | only divergence is the `double -> float` cast |
| USTRUCT vs analytic via float math | rel<1e-4 | adds analytic roundoff |
| USTRUCT at near-zero stress (P<10 N) | absolute diff fallback | float underflow region; rel measure not meaningful |

For visualisation, rel<1e-5 is far better than human eye / 8-bit colour can resolve.
If you ever need higher precision (forensic comparison, regression detection), call
`frame::computeStressField` directly and stay in `double`.

## What is NOT in v3.2.0

The following are deferred to v3.3 or later (see
[`HANDOFF_v3.2.0.md`](HANDOFF_v3.2.0.md) §3.2 for first-actions on each):

- **U-01** — Real "load JSON model" BP entry. Today's only entry is
  `ComputeCantileverFixture`.
- **U-02** — Slate panel fixture dropdown (cantilever / plate / cross / truss).
- **U-03** — Real renderer (spline mesh / Niagara / colour band). v3.2 only
  emits data; v3.3 paints it.
- **U-05** — Double-precision USTRUCT variant for high-precision use cases.
- **U-07** — Engine 0-sentinel vs USTRUCT -1-sentinel mismatch for
  `governingMemberId` / `governingShellId`. Behaviour: when nothing governs, the
  engine writes 0; the USTRUCT marshals it through as 0; consumers can confuse this
  with member ID 0 if such an ID actually exists. **Mitigation today**: check
  `Field.GlobalMaxFiberSigma > 0` first to confirm a real governing member exists.

## Verification matrix

5-leg gate at v3.2.0 + Phase 6 post-tag strengthening: **all green on the integrator
host**.

- Standalone F1..F70 ALL PASS
- **UE 70/70 tests** (60 base + S11 + 2 GPU smoke + 2 v3.2 Phase 2/3 + 3 Phase 6a
  marshal + 2 Phase 6e spawner+robustness + 2 Phase 6f theta+zero-load + 1 Phase 6
  closeout axial column = 70)
- OpenSees PASS
- linear deep audit 104 checks PASS
- CLI roundtrip 13 checks PASS
- v2_roundtrip CPU ALL PASS (kEngineVer=3.2.0 pin enforced)

Stability stress (3x repeat 五腿 gate): timing drift 0.7% across 353.9 / 352.0 / 351.4 s.
No flaky tests.

## See also

- [`docs/RELEASE_v3.2.0.md`](RELEASE_v3.2.0.md) — formal release notes
- [`docs/HANDOFF_v3.2.0.md`](HANDOFF_v3.2.0.md) — handoff for next session (U-01..U-07)
- [`docs/NIGHT_SHIFT_2026-06-22.md`](NIGHT_SHIFT_2026-06-22.md) — implementation log
- [`docs/specs/S11_stress_field.md`](specs/S11_stress_field.md) — engine layer spec
- [`docs/ARCHITECTURE.md`](ARCHITECTURE.md) §5b — `computeStressField` + FrameCoreUE
  in the architecture map
