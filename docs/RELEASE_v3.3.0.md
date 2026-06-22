# Release v3.3.0 — U-07 schema break + UE renderer + BP JSON load

**Tag:** `v3.3.0` (2026-06-22)
**Type:** **BREAKING (wire protocol + UE BP layer)** + new features
**Source delta vs v3.2.2:** 4 engine files / ~30 lines + UE/dispatcher/docs/test wire-up.

## TL;DR

The v3.3 release fixes a long-standing schema bug (an id-0 sentinel collision in
`StressField`) by renaming the governing pointer from a user-assigned **id** to a
0-based **internal index** with `-1` sentinel — across the engine struct, the v2
dispatcher JSON wire, and the FrameCoreUE USTRUCT BP layer. It also lands the
v3.2.2-deferred UE renderer (`AFrameCoreStressFieldActor`) and BP JSON load
entrypoint (`UFrameCoreStressFieldLibrary::ComputeFromJsonModel`), and reopens the
v3.2.2-deferred SS-beam Vy analytic oracle with sign-agnostic conservation checks.

**This is a BREAKING release**: pre-v3.3 GH / Rhino clients reading
`body.stressField.governingMemberId` will get a missing-key result rather than a
silently-aliased value. The migration is a one-line key rename + a lookup
adjustment — see § Migration below.

## What changed

### BREAKING — U-07: governing pointer is now an INDEX, not a user ID

Engine `frame::StressField` (header `FrameCore/StressField.h`):

```cpp
// Before (v3.1.0 .. v3.2.2)
int governingMemberId = 0;     // 0 = no governing member
int governingShellId  = 0;     // 0 = no governing shell

// After (v3.3)
int governingMemberIdx = -1;   // -1 = no governing, else index into FrameModel::members
int governingShellIdx  = -1;   // -1 = no governing, else index into FrameModel::shells
```

The pre-v3.3 schema was ambiguous when a model used `Member::id == 0`: the engine
wrote 0 to the governing pointer in both "no governing element" and "id 0 governs"
cases, which a client could not disambiguate. v3.3 separates the two by emitting
an index (always ≥ 0 for a real element) with -1 as the sentinel. The per-element
record (`MemberStressTrace::memberId`, `ShellStressLayer::shellId`) still carries
the user id for downstream rendering, so the migration is a one-shot lookup.

The dispatcher v2 JSON wire renames the response keys in lockstep:

```jsonc
// inspect.stress_field response body, pre-v3.3
{ "stressField": {
    "governingMemberId": 0,    // ambiguous: "no governing" or "id 0 governs"?
    "governingShellId":  -1    // (-1 came in via v3.1.0 C-07 fix on the cpp side, but
                               //  the header default 0 was never updated)
    ...
}}

// inspect.stress_field response body, v3.3
{ "stressField": {
    "governingMemberIdx": 0,   // unambiguous: slot 0 of model.members
    "governingShellIdx":  -1   // -1 = no governing shell
    ...
}}
```

The FrameCoreUE USTRUCT layer renames `FFrameStressField::GoverningMemberId /
GoverningShellId → GoverningMemberIdx / GoverningShellIdx` and the BP accessor
helpers `Get GoverningMemberId / Get GoverningShellId → Get GoverningMemberIdx /
Get GoverningShellIdx`.

`solve.linear` `summary.governingMember` is **unchanged** — that path is a D/C
screen, where `0 == no element exceeds allowable` is a true zero, not a sentinel.

`kEngineVer` bumps `"3.2.0" → "3.3.0"`. Wire ABI version 2 is unchanged
(transport / framing untouched); capability list of 23 is unchanged.

### New — U-03: AFrameCoreStressFieldActor (UE)

`Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameCoreStressFieldActor.h`
+ `Private/FrameCoreStressFieldActor.cpp`: a `Blueprintable` actor that takes a
`FFrameStressField` + per-member `FFrameMemberGeometry` and emits a sigma-band box
mesh via `UProceduralMeshComponent`. Per-member box has `samplesPerSpan` × 4 = 44
vertices for a default field, with per-vertex colour from a blue → green → red
ramp normalised against `Field.GlobalMaxFiberSigma`. Triangle count = 4 sides ×
2 tris × (samplesPerSpan - 1) segments + 2 × 2 end caps = 84 for the default
11-sample case (252 indices).

Static helper `AFrameCoreStressFieldActor::MakeCantileverDemoGeometry(L, Width,
Depth)` builds a single-member geometry list for the cantilever fixture, so a BP
graph wiring `Make Cantilever Demo Geometry` + `Compute Cantileve Fixture` +
`Build Mesh` is the one-line demo path.

`FrameSolver.uplugin` adds a `Plugins` block enabling `ProceduralMeshComponent`
as a transitive dep, so host `.uproject` files don't need to be touched (engine
rule #5).

**Thin slice — NOT in v3.3**: shell heat-map rendering, Niagara particle field,
collapse replay actor, camera control, overlay HUD, demo Map asset. These land
in v3.4+.

### New — U-01: ComputeFromJsonModel (UE)

`UFrameCoreStressFieldLibrary::ComputeFromJsonModel(const FString& JsonPath, int32
SamplesPerSpan = 11)` parses a JSON file in the dispatcher's `model.set` schema
subset (materials / sections / nodes / members / nodalLoads / memberUDLs), calls
`frame::solve`, then `computeStressField`, and returns the marshalled
FFrameStressField. Schema parts NOT supported (logged but ignored): shells,
member release / tensionOnly / prescribed displacements, solver options. On
parse / solve failure: emits a `UE_LOG(Warning)` and returns a default
FFrameStressField (governing indices == -1) so a BP graph never crashes.

### Closed — V321-01a: SS-beam Vy analytic oracle

The v3.2.2-deferred test (`FrameCoreUEMarshalSSBeamTest` block 4b) is re-enabled
with **sign-agnostic** assertions:

1. `Vy(endI) - Vy(endJ) = w_local.y * member_span` (engine's own
   integration formula, rel<1e-9).
2. `|Vy(endI, member 0)| = w * L / 2` (pin-support reaction magnitude,
   classical SS-beam closed form, rel<1e-9).

The v3.2.2 attempt (`|Vy| at midspan member 0 == w*L/4`) failed because (a)
"midspan of member 0" is the structure's quarter-span (not midspan), and (b)
`samples[k].Vy` sign depends on whether `endI.Vy` stores the joint reaction or
its negation — a convention F-fixtures do not pin. The replacement assertions
are convention-independent.

### New oracle — F71 sentinel edge fixture

`Plugins/FrameSolver/Standalone/main.cpp` adds F71 with three sub-cases:

1. **Empty model** (no members, no shells): `governingMemberIdx == -1`,
   `governingShellIdx == -1`, `globalMaxFiberSigma == 0`, `globalMaxVonMises == 0`.
2. **All-inactive members**: engine sees populated forces but skips them due to
   the `active = false` filter; same -1 sentinels.
3. **id-0 governing**: the cantilever fixture's member has `id == 0`, which used
   to alias the no-governing sentinel; v3.3 asserts `governingMemberIdx == 0`
   (slot index) AND `Members[0].MemberId == 0` (user id), proving the two are
   independently recoverable.

Standalone gate count moves F1..F70 → F1..F71. CUDA strict tests F67/F67s
unchanged.

## Migration guide

### Wire-protocol clients (GH / Rhino / SDK)

```csharp
// v3.2 — broken when a real element has id 0:
int gid = body["stressField"]["governingMemberId"].AsInt();
Member? gov = model.Members.FirstOrDefault(m => m.Id == gid);

// v3.3 — correct:
int gix = body["stressField"]["governingMemberIdx"].AsInt();
Member? gov = (gix >= 0) ? model.Members[gix] : null;
int govId = gov?.Id ?? -1;
```

Pin `FRAMECORE_EXPECTED_ENGINE_VER='3.3.0'` (or whatever the client uses for the
hello-version check) so v3.2 clients fail loudly rather than silently miss the
renamed key.

### Blueprint designers (FrameCoreUE)

The BP nodes rename:

| Pre-v3.3 | v3.3 |
|---|---|
| Break Frame Stress Field → Governing Member Id | Break Frame Stress Field → Governing Member Idx |
| Get Governing Member Id (library func) | Get Governing Member Idx |
| Get Governing Shell Id (library func) | Get Governing Shell Idx |

To recover the user id from the new index, look up the member array:
`Field.Members[GoverningMemberIdx].MemberId`. Same pattern for shells via
`Field.ShellsTop` / `Field.ShellsBot`.

### Server pin

`FRAMECORE_EXPECTED_ENGINE_VER` env var bumps `'3.2.0' → '3.3.0'` in
`Scripts/run_gpu_gate.ps1`, `.github/workflows/release-gate.yml`. Host scripts
that hard-pin the previous value will fail their `v2_roundtrip` step rather
than silently passing on a broken response shape.

## Reproduction matrix

| Path | Command | Expected |
|---|---|---|
| 5-leg (cuDSS host) | `Scripts\run_gate.ps1 -RequireOpenSees` | standalone F1..F71 ALL PASS, **UE 72/72**, OpenSees PASS, audit 104, CLI 13 ALL PASS |
| 5-leg (non-cuDSS) | `Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 70` | same minus the 2 `#if FRAMECORE_CUDA` UE tests |
| v2 dispatcher CPU | `build_capi_v2.bat` + `python Tools\v2_roundtrip.py` with `$env:FRAMECORE_EXPECTED_ENGINE_VER='3.3.0'` | `=== summary: ALL PASS ===`, includes the 4 new U-07 schema assertions |
| GPU 6th leg | `Scripts\run_gpu_gate.ps1 -Strict` | frametest_cuda F1..F71 + F67 / F67s + v2_roundtrip CUDA + r2_bench 90k ≤ 16.67 ms (engine source CUDA-path delta = 0, evidence carries forward from v3.0.0 / v3.1.0) |

**Verified on the integrator's host (2026-06-22):** legs 1, 2, 4, 5 +
v2_roundtrip CPU all green. CUDA legs NOT RUN this session (carry-forward
evidence; see HANDOFF_v3.3.0.md § 2 for the honest table).

## Bundle

`framecore-v3.3.0-win64.zip` (TBD MB) — `frame_capi.dll` (v1) +
`frame_capi_v2.dll` (CPU dispatcher) + `frame_cli.exe` + `frametest.exe` +
`openblas.dll` runtime + `LICENSE` + `README.txt`. (CUDA bundle published only
when the GPU gate is re-exercised on a cuDSS host.)

## Files added / removed

**Added:**
- `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameCoreStressFieldActor.h`
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/FrameCoreStressFieldActor.cpp`
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEActorStressMeshTest.cpp`
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalJsonTest.cpp`
- `docs/specs/S11_v3.3_schema_migration.md`
- `docs/HANDOFF_v3.3.0.md`
- `docs/RELEASE_v3.3.0.md` (this file)

**Removed:** none.

## Deferred to v3.4+

See `docs/HANDOFF_v3.3.0.md § 5` + § 7 for the full deferred / candidate list.
Short version: U-02 (Slate fixture dropdown), U-05 (USTRUCT double precision),
U-06 (VS2026 build warning), B-06 (model.patch schema), demo Map asset,
shell heat-map renderer, schema widen for `ComputeFromJsonModel`.

## Acknowledgements

The U-07 audit dates back to v3.2.0's release-hardening 3-agent pass which
flagged the id-0 sentinel collision; v3.2.1's audit kept the BLOCKER tag but
deferred the fix (it required engine source change, violating rule #1 for
v3.2's "thin slice, zero engine edits" framing). v3.2.2 closed five sibling
audit items but explicitly left U-07 for "next engine-source minor". v3.3 is
that minor.
