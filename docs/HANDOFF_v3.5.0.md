# HANDOFF v3.5.0 → next cycle

> What was just released, what to verify, what's deferred to v3.5.1 / v3.6.

## Z-01 (first action) — verify gate on a clean checkout

The release-hardening session that produced v3.5.0 wrote code, version-pinned everything,
and assembled docs in one sweep, but did NOT run the full 5-leg gate end-to-end on the
integrator host — UE incremental build is slow (~30-90 min on cold cache) and the session
context is shared with the user. **Step 1** for whoever picks this up:

```powershell
# Set $Root to the absolute path of your local ArchSim clone (e.g. E:\project\ArchSim),
# and $Engine to the matching UE 5.7 root (e.g. E:\project\UE_5.7).
$Root   = "<path-to-ArchSim-clone>"
$Engine = $env:UE_ENGINE_ROOT
if (-not $Engine) { $Engine = (Resolve-Path (Join-Path $Root '..\UE_5.7')).Path }
Set-Location $Root
& "$Engine\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development `
  -project="$Root\ArchSim.uproject" -waitmutex
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```

Expected:
- Leg 1 standalone: F1..F71 ALL PASS (bit-identical with v3.4.0; engine source delta = 0)
- Leg 2 UE automation: **120 / 120** PASS with cuDSS; **118 / 118** without
- Leg 3 OpenSees: PASS
- Leg 4 audit: 104 PASS
- Leg 5 CLI: 13 PASS
- Plus `build_capi_v2.bat` + `v2_roundtrip.py`: ALL PASS (kEngineVer=3.5.0 pin enforced)

If Leg 2 reports < 120 tests, check that all 7 new test files compiled:
- `FrameCoreUEDeformedShapeTest.cpp` (3 tests)
- `FrameCoreUEHeatmapTest.cpp` (3 tests)
- `FrameCoreUEModalShapeTest.cpp` (2 tests)
- `FrameCoreUEDynCollapseReplayTest.cpp` (3 tests)
- `FrameCoreUEFragmentClusterTest.cpp` (3 tests)
- `FrameCoreUEInfluenceLineTest.cpp` (1 test)
- `FrameCoreUEInteractiveSubsystemTest.cpp` (3 tests)
- `FrameCoreUEResponseDynamicTest.cpp` (4 tests)

UE adaptive non-unity build auto-includes new `.cpp`; no Build.cs edit needed.

## Deferred items

### U-08 (v3.5.1) — Showcase map + BP examples

Phase 9 of the v3.5 spec called for `Content/Maps/FrameCoreShowcase.umap` + 4-6 BP
examples (cantilever / tower-under-wind / pull-column-watch-collapse / real-time-slider).
Map assets are UE editor binary — they cannot be authored from CLI. A Python builder
script is provided:

```python
# First action: open UE Editor → Window → Output Log → Python (Script) tab →
# substitute your local repo path and run:
py "<path-to-ArchSim-clone>/Tools/build_v3_5_showcase_map.py"
```

The script spawns one of every v3.5 actor in a 3x3 grid and saves the map. The BP
parameter wiring (load JSON model, set MemberGeometry, wire ApplyPatchAndResolve to a
UMG slider, etc.) is the v3.5.1 designer iteration step.

**v3.5.1 risk note**: the script uses `unreal.EditorLevelLibrary.new_level()`, soft-deprecated since UE 5.5. If it errors on UE 5.7, try `unreal.EditorLevelUtils.new_level_from_template()` as a drop-in replacement.

### U-09 (v3.6) — Full Chaos POD destruction

`AFrameFragmentClusterActor` ships a thin slice: each cluster spawns an
`AStaticMeshActor` with `bSimulatePhysics = true` and per-chunk mass / velocity hints
from `FFrameFragmentCluster`. Full Chaos `UGeometryCollectionComponent` integration is
v3.6 because UE 5.7's Chaos destruction API has rough edges (geometry-collection
creation requires editor-time baking; runtime spawning is workable but the docs
are sparse). For v3.5 the StaticMesh path delivers the same end-user effect ("chunk
falls when member detaches") without the plugin-dep drift.

Migration path: replace `AStaticMeshActor` spawn in `SpawnOneChunk()` with
`AGeometryCollectionActor` + `UGeometryCollectionComponent::SetGeometryCollection(...)`.
The `FFrameFragmentCluster::Members` / `Shells` ID lists let you bake per-cluster
geometry sub-meshes at editor time, then load them at runtime.

### U-10 (v3.6) — Influence line polarity audit

`FFrameInfluenceLine.ReactionAtPosition[k]` is the engine's reaction at the
`(ReactNode, ReactDof)` when a unit -Z load is at `LoadNodes[k]`. `AFrameInfluenceLineActor`
paints positive values red and negative blue, with ribbon height proportional to value.
The SSBeam-midspan-moment test passes with the current convention, but a designer using
a +Z load convention may see flipped colours. Add a `bFlipPolarity` flag + audit the
test for both conventions.

### U-11 (v3.6) — Cubic Hermite member-axis interpolation

`AFrameDeformedShapeActor` linearly interpolates between the two end displacements when
sampling the 11-ring extrusion along a member. A cubic Hermite would honour the end
rotation vectors (Rx/Ry/Rz) and look smoother for curvy deflected shapes. Requires:
1. Marshal per-end rotation vector (already present in `FFrameNodalDisplacement.Rx/Ry/Rz`).
2. Map local-coord rotation back to global via member local axes (engine helper).
3. Implement Hermite basis in `BuildOneMemberSection`.

## Critical version pins (post-v3.5.0)

If a future release breaks the 5-leg gate at Leg 6 (v2 dispatcher round-trip) with a
version-pin failure, check that all four pins move together:

1. `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` — `kEngineVer`
2. `Plugins/FrameSolver/FrameSolver.uplugin` — `Version` + `VersionName`
3. `Scripts/run_gpu_gate.ps1` — `FRAMECORE_EXPECTED_ENGINE_VER`
4. `.github/workflows/release-gate.yml` — `FRAMECORE_EXPECTED_ENGINE_VER`

Pattern established by v3.0.0 audit BLOCKER 1 (stale "2.11.1" at v3.0.0 tag time) and
re-confirmed every release since. The user-facing `$ExpectedUeTests` (5th pin) lives in
`run_gate.ps1` and counts UE tests — bump it whenever a new IMPLEMENT_SIMPLE_AUTOMATION_TEST
lands or run_gate fails Leg 2 silently.

## Phase 7 PerfBaseline note

The Phase 7 `InteractiveSubsystem.PerfBaseline` test uses a 50-segment cantilever (~306
DOF) and asserts ApplyPatchAndResolve avg latency ≤ 200 ms. This is a CI-friendly
threshold, NOT the spec's 10K-DOF / 16.7 ms / 60 fps target — that one lives on the
engine R2 lane (`run_gpu_gate.ps1` r2_bench). The UE wrapper test only verifies "no
pathological marshalling overhead." If a future v3.6 changes the marshal layer and the
50-seg fixture regresses past 200 ms, that's a real signal.

## Engine-source-delta covenant

CLAUDE.md 鐵則 #1 is binding. v3.5 honoured it (0 lines under `FrameCore/`). Any future
v3.x cycle that touches engine source needs a CLAUDE.md amendment first, or the change
is a rule violation, not a release item.
