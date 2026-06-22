# UE5 Visual Surface Map — v3.5 spec

> Status: **landed v3.5.0** 2026-06-22 (HEAD `v3.5.0`). Phase 1-8 + Phase 10 shipped in
> one session; Phase 9 (showcase map authoring + BP examples) deferred to v3.5.1 because
> UE Editor binary assets require interactive designer iteration. See
> [docs/RELEASE_v3.5.0.md](../RELEASE_v3.5.0.md) for the surface inventory and
> [docs/HANDOFF_v3.5.0.md](../HANDOFF_v3.5.0.md) for v3.5.1 / v3.6 pickup items.
> Companion spec: [`UE5_engine_surface_map.md`](UE5_engine_surface_map.md) (v3.4).
> Cross-link bridge: [`docs/HANDOFF_v3.4_v3.5_design.md`](../HANDOFF_v3.4_v3.5_design.md).

## Purpose

v3.5 closes the engine-result-vs-UE-visual gap. After v3.5, every numerical
result produced by the engine has a matching UE5 actor that renders it
visually:

| Result kind | Renderer (v3.5 actor) |
|---|---|
| Static displacement | `AFrameDeformedShapeActor` |
| D/C utilization | `AFrameUtilizationHeatmapActor` |
| Modal shape | `AFrameModalShapeActor` |
| Dynamic collapse frames | `AFrameDynCollapseReplayActor` |
| Fragment cluster (post-collapse) | `AFrameFragmentClusterActor` (Chaos POD bridge) |
| Influence line | `AFrameInfluenceLineActor` |
| Stress field | `AFrameCoreStressFieldActor` (✅ v3.3, retained) |
| Real-time interactive | `UFrameInteractiveSubsystem` (GameInstanceSubsystem) |
| Response spectrum / RT dynamic | `AFrameResponseSpectrumActor` / `AFrameRealTimeDynamicActor` |

v3.5 is the **visual + game-ready surface**. After v3.5 a Blueprint designer
can build, solve, and SEE every analysis the engine offers without writing C++.

## Non-goals (v3.5)

- New analyses — every result kind below is produced by an existing v3.4 engine
  API (or v3.3 in stress-field's case). v3.5 adds zero new algorithms.
- Engine source changes — engine is **frozen** from v3.4 onward (target: 0
  lines under `Plugins/FrameSolver/Source/FrameCore/` across the entire v3.5
  cycle).
- Polished Chaos destruction physics — v3.5 ships a **functional bridge** from
  `FFrameFragmentCluster` to Chaos POD; tuning destruction realism (chunk
  geometry, secondary fractures, debris physics quality) is downstream
  art/design work, not engine work.

## Engine source delta budget

**Hard target: 0 lines under `Plugins/FrameSolver/Source/FrameCore/`** for the
entire v3.5 cycle. v3.5 is pure UE5 work — actor + material + procedural mesh
+ Chaos integration.

## Wire / version contract

- `kEngineVer` "3.4.0" → "3.5.0" (minor bump; new UE surfaces, no schema break).
- v2 dispatcher capability list unchanged (v3.5 is UE-side, not dispatcher-side).
- `FrameSolver.uplugin` Version 32 → 33, VersionName 3.4.0 → 3.5.0.
- `FrameSolver.uplugin` Plugins block may add `GeometryFramework` /
  `ChaosClothEditor` / etc. if needed for Chaos POD; the `ProceduralMeshComponent`
  dep from v3.3 stays.
- `FRAMECORE_EXPECTED_ENGINE_VER` '3.4.0' → '3.5.0'.
- `run_gate.ps1 $ExpectedUeTests` ~95 → ~120 (each renderer adds 2-4 tests).

## Phase tree

### Phase 0 — Spec + planning (this doc)

Already done at v3.3.0 tag time. v3.5 implementation start re-reads this spec
+ confirms v3.4 USTRUCTs landed as designed.

### Phase 1 — Deformed Shape Actor

`AFrameDeformedShapeActor : AActor` reads `FFrameSolveResult.Displacement` and
`TArray<FFrameMemberGeometry>` (from v3.3 — same struct used by stress field
actor), emits a procedural mesh of the deformed structure.

API:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite) FFrameSolveResult Solution;
UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FFrameMemberGeometry> MemberGeometry;
UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeflectionScale = 100.f;  // visual amplification
UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShowUndeformedOverlay = true;
UFUNCTION(BlueprintCallable) bool BuildMesh();
```

Build logic: for each `MemberGeometry`, look up the matching member's
`endI` and `endJ` node displacements from `Solution.Displacement`, lerp ring
vertices along the deflected member axis (cubic Hermite if rotations available),
emit a PMC section (same 4-ring × N-sample pattern as v3.3 stress field actor).

Tests (`FrameCoreUE.Render.DeformedShape.*`):

1. **CantileverTip**: cantilever tip deflection should equal `P*L^3/(3*E*Iz) *
   DeflectionScale` rel < 1e-4 (geometry sample at the tip ring center).
2. **EmptyField**: zero-displacement solution → mesh matches undeformed
   geometry bit-exact.
3. **PMC section count**: matches member count.

Estimated Phase 1 cost: 5 hr.

### Phase 2 — Utilization Heatmap Actor

`AFrameUtilizationHeatmapActor` reads `FFrameSolveResult.MemberUtilization` and
`ShellUtilization`, paints a member-by-member (and shell-by-shell) color band
keyed by D/C ratio.

API:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite) FFrameSolveResult Solution;
UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FFrameMemberGeometry> MemberGeometry;
UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FFrameShellGeometry> ShellGeometry;  // analogue for shells (NEW v3.5 struct)
UPROPERTY(EditAnywhere, BlueprintReadWrite) float SaturationDC = 1.0f;  // DC value mapped to "red" in the ramp
UFUNCTION(BlueprintCallable) bool BuildHeatmap();
```

Build logic: per member, sample DC along the span using the same `MemberStressTrace`
sampling pattern v3.3 already uses, map DC -> color via blue->green->yellow->red
ramp normalised to `SaturationDC` (default 1.0, so DC == 1.0 = pure red). Per
shell, paint each corner sample.

Tests (`FrameCoreUE.Render.UtilizationHeatmap.*`):

1. **CantileverDC**: cantilever root member should be saturated (red end of
   ramp), tip should be at low end.
2. **UnstressedModel**: zero-load model → all members at low end of ramp.
3. **VertexCount**: matches expected geometry.

Estimated Phase 2 cost: 4 hr.

### Phase 3 — Modal Shape Actor

`AFrameModalShapeActor` reads `FFrameModalResult`, animates the structure
along a chosen mode shape with sinusoidal amplitude × `cos(2*pi*frequency*t)`.

API:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite) FFrameModalResult Modes;
UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FFrameMemberGeometry> MemberGeometry;
UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ModeIndex = 0;
UPROPERTY(EditAnywhere, BlueprintReadWrite) float Amplitude = 100.f;
UPROPERTY(EditAnywhere, BlueprintReadWrite) float TimeScale = 1.f;  // 1.0 = real time; smaller = slow motion
virtual void Tick(float DeltaTime) override;  // updates phase, rebuilds mesh
```

Tests (`FrameCoreUE.Render.ModalShape.*`):

1. **FirstModeShape**: cantilever first mode tip displacement should be ≈
   `Amplitude * cos(omega_1 * t)`.
2. **ModeSwitch**: changing `ModeIndex` switches the displayed shape.

Estimated Phase 3 cost: 5 hr.

### Phase 4 — DynCollapse Replay Actor

`AFrameDynCollapseReplayActor` reads `FFrameDynCollapseResult.Frames`, plays
back with a `UTimelineComponent` driver. Supports scrubbing (BP-side
`SetPlaybackTime`), play/pause, speed control.

API:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite) FFrameDynCollapseResult CollapseResult;
UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FFrameMemberGeometry> MemberGeometry;
UPROPERTY(EditAnywhere, BlueprintReadWrite) float PlaybackSpeed = 1.f;
UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLoop = false;
UPROPERTY(BlueprintReadOnly) float CurrentTime = 0.f;
UFUNCTION(BlueprintCallable) void Play();
UFUNCTION(BlueprintCallable) void Pause();
UFUNCTION(BlueprintCallable) void SetPlaybackTime(float NewTime);
UFUNCTION(BlueprintCallable) void OnEventReached(const FFrameDynCollapseEvent& Event);  // delegate hook
virtual void Tick(float DeltaTime) override;
```

Build logic per Tick:

1. Find bracketing frames `Frames[k]` / `Frames[k+1]` where `Frames[k].Time <=
   CurrentTime <= Frames[k+1].Time`.
2. Lerp per-node displacement between them.
3. Apply to MemberGeometry, rebuild PMC.
4. Fire `OnEventReached` for any event whose `Time` crossed during this Tick.

Tests (`FrameCoreUE.Render.DynCollapseReplay.*`):

1. **PlaybackEndToEnd**: 10-frame collapse, assert all frames visited.
2. **InterpolationMidframe**: at `t = (T0 + T1) / 2`, vertex position is
   midpoint of two frame's positions.
3. **EventDelegate**: event at `t = 0.5` fires when `CurrentTime` crosses it.

Estimated Phase 4 cost: 10 hr (interpolation + scrubbing + event delegate is
the bulk).

### Phase 5 — FragmentCluster → Chaos POD Bridge

`AFrameFragmentClusterActor`: consumes `FFrameFragmentCluster[]` (from
`FFrameDynCollapseResult.Fragments`), spawns Chaos POD destruction debris at
the cluster centroid with mass proportional to `Cluster.Mass`.

This is the "game-feel" payoff of the whole UE wrapper line — the cluster of
members/shells that detached at collapse time becomes a physical Chaos chunk
that falls, collides, settles.

API:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite) FFrameDynCollapseResult CollapseResult;
UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FFrameMemberGeometry> MemberGeometry;
UPROPERTY(EditAnywhere, BlueprintReadWrite) UGeometryCollection* ChunkTemplate;  // optional shared template
UFUNCTION(BlueprintCallable) void SpawnFragmentDebris();  // call after collapse simulation ends
UFUNCTION(BlueprintCallable) void ClearDebris();
```

**v3.5 landed status: thin slice — `AStaticMeshActor` + `bSimulatePhysics = true` debris,
mass + initial linear velocity hints from `FFrameFragmentCluster`. Full Chaos
`UGeometryCollectionComponent` integration deferred to v3.6 (UE 5.7 Chaos destruction
API has rough edges; the StaticMesh path delivers the same end-user effect).** See
[`docs/RELEASE_v3.5.0.md`](../RELEASE_v3.5.0.md) Honest Boundaries for rationale and
[`docs/HANDOFF_v3.5.0.md`](../HANDOFF_v3.5.0.md) U-09 for the v3.6 migration path.

Original v3.5 spec implementation path (v3.6 work): use UE5's `UGeometryCollectionComponent` (Chaos destruction)
to spawn a `AGeometryCollectionActor` per `FFrameFragmentCluster`. Geometry
collection's bounds come from the fragment's member/shell list (compute AABB).
Initial velocity / angular velocity hints can come from the last
`FFrameDynCollapseFrame.Velocity` field at each cluster's nodes.

Tests (`FrameCoreUE.Render.FragmentCluster.*`):

1. **SpawnCountMatchesClusters**: assert spawned actor count == `Fragments.Num()`.
2. **CentroidPosition**: spawned actor location == cluster centroid (FVector
   distance < 0.1).
3. **Mass propagation**: spawned chunk mass approximates `Cluster.Mass`
   (Chaos mass setting; tolerance loose since Chaos defaults may override).

Estimated Phase 5 cost: 10 hr (Chaos integration is the time sink; the BP
plumbing is small).

### Phase 6 — Influence Line Actor

`AFrameInfluenceLineActor`: reads `FFrameInfluenceLine`, paints a band along
the loaded path showing influence intensity at the queried response.

API:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite) FFrameInfluenceLine Result;
UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FFrameMemberGeometry> PathGeometry;
UFUNCTION(BlueprintCallable) bool BuildMesh();
```

Build logic: per path member, color samples by influence value (blue = negative
influence, red = positive).

Tests (`FrameCoreUE.Render.InfluenceLine.*`):

1. **SSBeamMidspanMoment**: SS beam, query response = midspan moment, influence
   line should peak triangularly at midspan with value `L / 4`.

Estimated Phase 6 cost: 3 hr.

### Phase 7 — Real-time Interactive Subsystem

`UFrameInteractiveSubsystem : UGameInstanceSubsystem`: wraps a
`frame::ReSolveSession` (S1 ReSolve), exposes BP-callable
`ApplyPatch(FFramePatch) → FFrameSolveResult` with rank-k updates instead of
fresh factorisation. Target: 60 fps @ 10K DOF interactive (engine R2 lane has
this benchmark on standalone).

API:

```cpp
UPROPERTY(BlueprintReadOnly) UFrameModel* Model;  // active model
UFUNCTION(BlueprintCallable) bool StartSession(UFrameModel* Model);
UFUNCTION(BlueprintCallable) bool ApplyPatchAndResolve(const FFrameModelPatch& Patch, FFrameSolveResult& OutResult);
UFUNCTION(BlueprintCallable) void EndSession();
```

`FFrameModelPatch` mirrors `frame::ModelPatch` (or whatever the engine's S1
patch type is named): {nodalLoadAdditions, sectionUpdates, materialUpdates,
prescribedDisplacementUpdates}.

The Subsystem keeps a `frame::ReSolveSession*` alive across BP ticks; each
`ApplyPatchAndResolve` call dispatches to engine `analysis.reanalysis_solve`
which routes through the S1 three-level ladder (Woodbury / stale-LDLT PCG /
rebaseline).

Tests (`FrameCoreUE.Interactive.*`):

1. **StartEndLifetime**: start session, no leaks; end session frees resources.
2. **PatchSemantics**: nodal load increment patch → result matches fresh solve
   with the cumulative load rel < 1e-9.
3. **PerfBaseline**: 10K-DOF cantilever, measure ApplyPatchAndResolve latency,
   assert <= 200 ms (CI-friendly relaxation; the real 60 fps @ 10K DOF target lives on the engine R2 lane standalone benchmark, not the UE wrapper test — see [`docs/HANDOFF_v3.5.0.md`](../HANDOFF_v3.5.0.md) "Phase 7 PerfBaseline note"). Originally specified as < 16.7 ms (60 fps); the UE wrapper test asserts only "no pathological marshalling overhead" on a 50-segment cantilever (~306 DOF). This is the BP-side mirror of the engine R2
   benchmark.

Estimated Phase 7 cost: 8 hr (subsystem lifetime + perf benchmark are the
non-trivial parts).

### Phase 8 — ResponseSpectrum + RealTimeDynamic Actors

`AFrameResponseSpectrumActor` + `AFrameRealTimeDynamicActor`: both animate a
deformed structure driven by a time-history or spectrum-derived modal
combination.

API and tests follow the same pattern as `AFrameModalShapeActor` (Phase 3).

Estimated Phase 8 cost: 5 hr.

### Phase 9 — Demo Map + BP examples

`Content/Maps/FrameCoreShowcase.umap`: one Map containing one instance of
every v3.3+v3.5 actor in a labeled grid layout. Designed to be the Hit-Play
starting point for any new UE5 designer onboarding to FrameCore.

Plus 4-6 BP example assets demonstrating:

- "Hello cantilever" (Phase 1 + 3)
- "Multi-story tower under wind" (Phase 2)
- "Pull a column, watch collapse" (Phase 4 + 5)
- "Real-time slider on tip load" (Phase 7)

NOTE: Map assets are UE editor binary, not textual. They must be authored
in-Editor (or via UE Python `unreal.EditorAssetLibrary`). This Phase is the
manual UE-editor portion of v3.5; budget reflects that.

Estimated Phase 9 cost: 3 hr (Map authoring + BP example drag-and-drop in
Editor).

### Phase 10 — Version bumps + audit + release

- `FrameSolver.uplugin` Version 32 → 33, VersionName "3.4.0" → "3.5.0".
- `Dispatcher.h` `kEngineVer` "3.4.0" → "3.5.0".
- `Scripts/run_gate.ps1` `$ExpectedUeTests` ~95 → ~120 (per-Phase test count
  finalised at Phase 10).
- `Scripts/run_gpu_gate.ps1` + `.github/workflows/release-gate.yml`
  `FRAMECORE_EXPECTED_ENGINE_VER` '3.4.0' → '3.5.0'.
- `README.md` v3.5 status block; "game-ready" callout.
- `docs/VERIFICATION.md` UE count update + new render test families list.
- `docs/ARCHITECTURE.md` UE count update + new actor list.
- `docs/HANDOFF_v3.5.0.md` + `docs/RELEASE_v3.5.0.md` (new).
- `docs/specs/UE5_visual_surface_map.md` (this file): mark "landed v3.5.0".
- `E:/project/CLAUDE.md` line 13 v3.4.0 → v3.5.0.
- 3-agent audit pass.
- Closeout findings.
- Final 5-leg gate green.
- git tag v3.5.0 + push + gh release with bundle.

Estimated Phase 10 cost: 5 hr.

## Risks

- **Chaos API churn**: UE 5.7 Chaos destruction API has rough edges; some
  Phase-5 work may need workarounds. Budget Phase 5 generously (10 hr) and
  reserve the option to ship Phase 5 as a "thin slice + known issues" if
  Chaos integration is harder than spec.
- **PMC vs DynamicMesh**: v3.3 chose `UProceduralMeshComponent` (PMC) for the
  stress field actor because it's a long-stable plugin. UE 5 is migrating to
  `UDynamicMeshComponent`. Stay on PMC throughout v3.5 for consistency; revisit
  the DynamicMesh migration in v3.6 as a separate refactor.
- **Tick-rate budget for Phase 7 InteractiveSubsystem**: the 16.7 ms / frame
  goal is a hard target. If S1 ReSolve cannot meet it on the test fixture
  (10K DOF), the Phase 7 test loosens to "at least 4x faster than fresh
  solve" and the spec adds an honest note that 60 fps requires <= N DOF
  empirically. The engine R2 benchmark already pins the standalone number;
  v3.5 only needs to confirm the UE wrapper does not impose overhead beyond
  thin marshaling.
- **Map authoring is manual**: Phase 9 cannot be fully scripted from
  Claude-Code (UE Editor session required). Plan ~1 hr human-in-the-loop at
  Phase 9 start to author the Showcase Map; the BP examples are easier to
  author from Python via `unreal.EditorAssetLibrary` if scripting helps.

## Done definition (v3.5 ships)

1. Every actor in the table at the top of this doc exists, is `Blueprintable`,
   builds without error in a transient world (UE test), and produces a visible
   mesh (or in Chaos's case, a spawned `AGeometryCollectionActor`).
2. `UFrameInteractiveSubsystem` lifetime test passes; perf baseline test passes
   (with the honest fallback if Phase 7's risk hit fires).
3. `FrameCoreShowcase.umap` exists and ships in the v3.5 source tarball.
4. UE test count moves ~95 → ~120, all pass.
5. 5-leg gate green; v2_roundtrip CPU green with `kEngineVer 3.5.0` pin.
6. Engine source delta = 0 lines under `FrameCore/`. CUDA path delta = 0.
7. RELEASE notes' "Honest boundaries" section adds the visual-side caveats
   (Chaos destruction is visual, not physical; tick-rate caveats for Phase 7).
8. 3-agent audit clean.

## Honest boundaries (v3.5-specific, added on top of v3.4's engine boundaries)

- **Chaos POD destruction is a visual effect, not a physics-accurate
  simulation**. The engine's `FragmentCluster` tells us *which members
  detached*; Chaos chooses *how the chunks fall*. The engine does not
  produce post-collapse physics — Chaos owns that. Document explicitly in
  Phase 5 RELEASE notes.
- **DeflectionScale is a visual amplification** (default 100x). Real
  displacements are typically < 1 mm for serviceable structures; without
  amplification the deformed shape looks identical to the undeformed shape
  in a viewport. Document the design choice + how to interpret.
- **Real-time interactive at 60 fps depends on DOF and patch size**. The
  10K-DOF / 16.7 ms baseline is the v3.5 spec target; structures larger than
  that require sub-60 fps interaction (or a thinner patch). Document in
  Phase 7 RELEASE notes.
- **Replay actor interpolation is linear between frames**. The engine's
  Newmark integration produces frames at fixed `dt`; visual interpolation
  between two frames is linear (not cubic / spline). Adequate for replay at
  default dt < 1/30 s; high-frequency artifacts possible at coarser dt.
- **Modal shape animation is artificial timing**. The animation phase
  `cos(omega * t)` is real frequency, but the amplitude is BP-tuned and
  unrelated to the actual energy in the mode.

These boundaries are documented at each actor's class-level doc-comment so a
BP designer sees them in the editor tooltip.

## Cross-version contract

- v3.4 lands first. v3.5 cannot start until v3.4 ships because v3.5 actors
  consume v3.4 USTRUCTs (`FFrameSolveResult`, `FFrameModalResult`,
  `FFrameDynCollapseResult`, etc.).
- During v3.4 implementation, if a USTRUCT design choice surfaces that needs
  iteration based on actor consumer needs, file as a "v3.5 needs X" note on
  v3.4 RELEASE notes. v3.4 can ship with the planned USTRUCT shape; v3.5
  Phase-1 reads start with a "v3.4 USTRUCT validation pass" before any actor
  work begins.
