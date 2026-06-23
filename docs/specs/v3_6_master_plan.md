# v3.6 Master Plan — FrameCore Final Release

> **Status: design locked 2026-06-23.** Implementation pending.
>
> **Scope:** v3.6 closes every deferred item on the v3.5 series HANDOFF list **except**
> S11 MITC9i high-order shells (the only item that requires engine algorithm work);
> adds the C6/C7/C8 along-span visualisation data line; and ships the most thorough
> adversarial audit + exit-test suite FrameCore has ever had.
>
> **Hard rule: this is the LAST FrameCore update.** After v3.6 tags, engine /
> consumer surface is frozen.

## Non-goals (carried forward, never to be done)

These are documented dead-ends from CLAUDE.md "刻意排除" lines:

- RC ultimate-state check (D/C stays elastic allowable)
- Fibre section / pushover (user explicit exclusion)
- Biaxial My-Mz N-M interaction (S10 ships uniaxial only)
- N-M tangent stiffness (event-to-event stays the contract)
- True elastoplastic unload / reverse
- S11 MITC9i high-order shells (user explicit defer; would touch 9 engine sites)
- Curved shell elements (flat-facet MITC4 stays the contract)

## Engine source delta budget

**Hard target: 0 lines under `Plugins/FrameSolver/Source/FrameCore/`** for the entire
v3.6 cycle. Every C6/C7/C8 visualisation builds on existing `FRAMECORE_API` entries
(`computeStressField`, `ReSolveSession::setMemberActive`, the 8 linear analyses,
S1-S10 nonlinear). If during implementation any item is found to need a new engine
facade, the spec is wrong and the item drops to deferred-after-v3.6.

CUDA path delta: 0 lines (same reasoning).

## Phase tree (14 phases + 14-agent audit)

### Phase 0 — Master plan + task tree
This document. Defines done conditions and the exit-test suite contract.

### Phase 1 — U-11: cubic Hermite member-axis interpolation
`AFrameDeformedShapeActor` currently lerps linearly between two end displacements.
Cubic Hermite uses the per-end rotation vectors (Rx/Ry/Rz) — already marshalled in
`FFrameNodalDisplacement` — to produce a smooth deflected curve.

Implementation:
- Add helper to `FramePMCHelpers.h`: `HermitePoint(t, startPos, startTangent, endPos, endTangent)`.
- `BuildOneMemberSection` lerps ring centres via Hermite when both end indices are
  set; tangent is derived from member local frame rotated by Rx/Ry/Rz times member
  length scale.
- New BP toggle `bUseHermiteInterpolation` (default true; falls back to lerp when
  rotations are zero).

Tests: 3 new (`Hermite.SineDeflection`, `Hermite.ZeroRotationLerp`,
`Hermite.RotationDriven`).

### Phase 2 — U-12: FFrameModelPatch incremental nodal-load patch
v3.5 `FFrameModelPatch` only carries activate/deactivate ids. v3.6 adds:

- `TArray<FFrameNodalLoad> AddNodalLoads` (incremental;
  added to current load state at `ApplyPatchAndResolve`)
- `TArray<FFrameNodalLoad> SetNodalLoads` (replace at given node)
- `bool bResetLoads` (clear loads first, then apply Set/Add)

`UFrameInteractiveSubsystem::ApplyPatchAndResolve` is extended to: clear loads if
`bResetLoads`, write `SetNodalLoads` into `Cached->Model`, accumulate `AddNodalLoads`,
then call `Session->solve()` — solve internally uses the model's nodal-loads, so the
ReSolveSession picks up the new RHS without needing a new engine facade.

Tests: 2 new (`PatchAddLoad.Cantilever`, `PatchResetReplaces`).

### Phase 3 — C6: AFrameInternalForceFieldActor (沿桿 BMD/SFD)
Reads `FFrameStressField` (already computed via `UFrameCoreStressFieldLibrary`) and
emits a ribbon mesh per member where the ribbon height is proportional to one of
`{N, Vy, Vz, T, My, Mz}` at each sample along the member. Sign-aware (positive vs
negative renders on opposite sides of the member axis).

API:
```
FFrameStressField Field
TArray<FFrameMemberGeometry> MemberGeometry
EFrameForceComponent Component  // enum N/Vy/Vz/T/My/Mz
float HeightScale = 100.f
bool bDualSidedSigned = true
```

Build logic: per member, sample `Trace.Samples[k].{N,Vy,...}`, build 2D ribbon along
member +localY (or +localZ for My) with sign-flipped extrusion. Colour ramp via
`SignedRamp` from InfluenceLine actor.

Tests: 4 new (cantilever Vz triangular, SSBeam Mz parabolic + half-span sign change,
empty trace, component-switch).

### Phase 4 — C7: AFrameUtilizationFieldActor (沿桿 D/C 場)
Distinct from v3.5 `AFrameUtilizationHeatmapActor` (which paints per-member peak
risk). v3.6 paints **along-span D/C** sampled at each `FFrameStressFieldSample`,
computed from `max(SigmaCompMax / Cap.Comp, SigmaTensMax / Cap.Tens, TauShear /
Cap.Shear)` — the same screen as `ElasticAllowable` but per-sample.

API:
```
FFrameStressField Field
TArray<FFrameMemberGeometry> MemberGeometry
TArray<FFrameMaterial> Materials       // per-member material refs (sec idx into model)
float SaturationDC = 1.0f
bool bShowExceedanceOnly = false       // only paint segments where DC > 1
```

Tests: 3 new (cantilever root-vs-tip ramp, exceedance filter, unstressed model).

### Phase 5 — C8: AFrameRedundancyFieldActor (贅餘度 sample)
Builds on `UFrameInteractiveSubsystem` + S1 ReSolve. For each member in a watch list,
deactivates the member, resolves, measures the resulting `MaxDC` jump on the remaining
structure, and reactivates. The "redundancy" of a member is high when removing it
causes a large DC jump (the structure depends on it). Paints members by jump magnitude.

API:
```
UFrameInteractiveSubsystem* Subsystem  // BP ref
TArray<int32> WatchedMemberIds         // typically the user-defined "critical members"
TArray<FFrameMemberGeometry> MemberGeometry
float SaturationJump = 0.5f            // DC jump value mapped to "red"
UFUNCTION(Async) ComputeRedundancy()   // long-running; emits OnRedundancyComputed
```

Tests: 2 new (cantilever single-member: removing makes mechanism → ∞ jump; portal
2-member: removing redundant brace → moderate jump).

### Phase 6 — U-09: Chaos POD GeometryCollection real integration
v3.5 ships `AStaticMeshActor` thin-slice debris. v3.6 migrates to
`UGeometryCollectionComponent` + `AGeometryCollectionActor`:
- Add `GeometryCollection` + `GeometryCollectionEngine` to FrameCoreUE Build.cs
- Add `GeometryCollectionPlugin` to FrameSolver.uplugin Plugins block
- Rewrite `AFrameFragmentClusterActor::SpawnOneChunk` to use
  `UGeometryCollectionComponent::SetGeometryCollection` with a procedurally-baked
  collection (one chunk per `FFrameFragmentCluster.Members`).

Risk acknowledged: UE 5.7 GeometryCollection API has rough edges; v3.6 ships with
both `bUseChaosPOD = true` (Chaos path) and `bUseChaosPOD = false` (v3.5 StaticMesh
path) for fallback safety.

Tests: 3 (Chaos-on spawn count, Chaos-off legacy parity, mass propagation through GC
API).

### Phase 7 — U-10: influence-line polarity audit + flip option
Audit: confirm `FFrameInfluenceLine.ReactionAtPosition[k]` sign convention against
the engine's `reactionInfluenceLine` for both +Z and -Z unit-load conventions. Add
`bFlipPolarity` BP toggle on `AFrameInfluenceLineActor` for designer convention
mismatch.

Tests: 1 (cantilever influence flip toggle).

### Phase 8 — U-15: PerfBaseline CI-calibrated threshold
Replace the 200 ms generous threshold with: (a) absolute ≤ 20 ms on the host's
50-segment cantilever fixture, (b) relative ≤ 4× fresh `frame::solve` baseline. The
existing test gains a second `TestTrue` for the relative bound; the absolute bound
moves to 20 ms; the message names the actual `frame::solve` baseline so a CI failure
is immediately diagnostic.

### Phase 9 — U-08: showcase map Python script enhanced + BP examples spec
`Tools/build_v3_5_showcase_map.py` becomes `Tools/build_v3_6_showcase_map.py` with
the 3 new C6/C7/C8 actors added, plus `Content/BP/` example assets generated
declaratively via the Python script. Adds `docs/UE5_BP_EXAMPLES.md` documenting
"hello cantilever" / "tower under wind" / "pull a column, watch collapse" /
"real-time slider on tip load" / "redundancy heatmap" BP graphs (text spec; designer
recreates in-Editor following the doc).

### Phase 10 — Real-time DynCollapse callback live channel
v3.5 `AFrameDynCollapseReplayActor` plays back `DynCollapseResult.Frames` already
materialised. v3.6 adds a **live** path: `UFrameInteractiveSubsystem::StartLiveDynCollapse`
launches a background thread that calls `frame::runDynamicCollapse` with the
`onFrameEmitted` callback bound to a thread-safe queue; the actor's Tick drains the
queue and renders frames as they arrive. Game-feel target: see the structure deform
in real time, not after-the-fact replay.

API additions:
```
UFUNCTION(BlueprintCallable) bool StartLiveDynCollapse(const FFrameDynCollapseOptions& Opts)
UFUNCTION(BlueprintCallable) void CancelLiveDynCollapse()
UPROPERTY(BlueprintAssignable) FFrameLiveFrameDelegate OnFrameReceived
UPROPERTY(BlueprintReadOnly) bool bLiveCollapseRunning
```

Tests: 2 (live frame count > 0; cancel mid-run brings count to plateau).

### Phase 11 — Exit-test suite (出場測試)

**This is the new artefact unique to v3.6.** Beyond per-feature unit tests, the exit
suite proves the engine + UE surface holds under adversarial conditions across
4 dimensions:

1. **Property-based fixture generator** (`Standalone/exit_property.cpp`,
   built via `build_exit_property.bat`):
   - 1000 randomly-generated valid models (sampled from a `FrameModel` distribution:
     5..200 nodes, 1..400 members, random material/section table, random load).
   - Per model: solve, then check oracle invariants — solver `singular` flag matches
     `validate()`'s mechanism prediction, `PivotMargin > 0` ⇔ non-singular,
     `sum(Reactions) + sum(Loads) ≈ 0` (equilibrium to 1e-9), per-end member force
     equilibrium per member, `MaxDC > 0` for any non-trivial load.
   - Fixed `XorShift64*` seed (no `random_device`) for reproducibility.

2. **Fuzz testing on JSON dispatcher** (`Tools/exit_fuzz.py`):
   - 100K random JSON payloads (well-formed up to schema; deliberately broken at
     fuzz step) sent to `frame_capi_v2.dll` via cffi; expect every reply to be a
     valid `kind in {ok, error}` envelope with documented error code, never a
     segfault, never a wire ABI break.

3. **Large-scale benchmark ladder** (`Standalone/exit_bench.cpp`):
   - 10K / 100K / 500K DOF fixtures; measure default LDLT, opt-in supernodal, opt-in
     GPU backsub (when cuDSS available). Per fixture: `assembleAndFactor` time,
     `solveLoad` time, peak memory. Print a fingerprinted table; the gate compares
     against a checked-in baseline `Standalone/exit_bench_baseline.json` and fails
     on > 15% regression.

4. **Cross-implementation oracle re-runs** (existing OpenSees compare in extended
   mode):
   - 50-case OpenSees mega benchmark beyond the gate's default 1-case shallow-arch.
   - All v3 fixtures F68/F69/F70/F71 + 22 v3.5 UE renderer tests + 18 v3.4 BP
     analysis tests re-run with `FRAMECORE_EXIT_TEST=1` strict-mode env var that
     tightens every floating tolerance by 10×.

Exit-suite runner: `Scripts/run_exit_tests.ps1` — runs all 4 dimensions, prints
pass/fail per dimension + per item, exit 0 only if every item passes. Documents
"exit-test PASS" in RELEASE notes as a stricter superset of the 5-leg gate.

### Phase 12 — 14-agent adversarial audit

Twice the v3.5.0 release-hardening fan-out. 14 parallel read-only audit agents
across 14 dimensions:

| # | Lane | Focus |
|---|---|---|
| 1 | Engine numerics core | DOF order, mechanism via pivot, opt-in bit-identity (15 fixtures) |
| 2 | Stress kernel / D/C ladder | StressKernel.h vs ElasticAllowable invariants, F70 D/C interlock |
| 3 | S1 ReSolve ladder correctness | Tier-1 exact, Tier-2 tolerance, Tier-3 fallback under adversarial patch sequences |
| 4 | S2/S3/S4/S5/S7/S9/S10 nonlinear path | Cross-check vs OpenSees + analytic |
| 5 | MITC4 shell + opt-in (DKQ/QM6/CR/warp) | bit-identity proofs, F-fixtures coverage |
| 6 | v2 dispatcher + JSON wire ABI | every capability, every error code, schema stability |
| 7 | C-API DLL surface | every `frame_capi_*` symbol export, return-code matrix |
| 8 | UE5 BP surface — input USTRUCT | every `FFrameModelDef` field, every options struct |
| 9 | UE5 BP surface — output USTRUCT | every `FFrameSolveResult` field, marshal correctness |
| 10 | UE5 actor PMC geometry | all 11 actors (8 v3.5 + 3 new C6/C7/C8), normal flip, cap winding |
| 11 | UE5 subsystem lifetime | StartSession / ApplyPatch / Live DynCollapse threading |
| 12 | Build system + version pins | uplugin, kEngineVer, env vars, $ExpectedUeTests, gate stability |
| 13 | Docs cartography + repro | README / VERIFICATION / ARCHITECTURE / CLAUDE.md + every code-block runs from a stranger's fresh clone |
| 14 | Privacy + sanitize + release-readiness | hardcoded paths, agent scratch, build artefacts, untracked, gh release notes |

Each agent uses the v3.5 prompt template + a structured finding table. Cross-agent
finding overlaps are confidence boosts (e.g. F60 cited by 1, 2 and 5 is a single
issue to fix once with high confidence).

Closeout per v3.5 pattern: small fixes apply in-place; substantive items defer
**outside FrameCore** (frozen) — to UE consumer-side tickets that don't belong to
this release.

### Phase 13 — Final release + v3 series retrospective doc

- Single release commit (explicit `git add` per CLAUDE.md 鐵則 #5).
- `git tag -a v3.6.0 -m "..."`, push branch + tag.
- `gh release create v3.6.0 --title "FrameCore final" --notes-file docs/RELEASE_v3.6.0.md
  --latest`.
- New: `docs/V3_SERIES_RETROSPECTIVE.md` — the story of v3.0..v3.6, what was learned,
  why MITC9i never landed, what the engine is honestly good for and not.
- `docs/HANDOFF_v3.6.0.md` is the *last* HANDOFF; documents the freeze contract and
  what a future maintainer would have to violate to make changes.
- `CLAUDE.md` anchor block updated to v3.6.0 + a "FROZEN" marker on engine source.
- Bundle `framecore-v3.6.0-win64.zip` (same composition as prior releases).

## Done conditions

1. Every U-XX item from v3.5.0 + v3.5.1 HANDOFF (except S11 MITC9i) is implemented
   or deliberately documented as out-of-scope-permanent.
2. C6 + C7 + C8 visualisation data line lands as 3 new BP-callable actors with
   passing tests.
3. UE test count 120 → ~145 (+25 for v3.6 additions).
4. 14-agent adversarial audit clean (zero open BLOCKER, all HIGH closed or
   explicitly deferred-permanent).
5. Exit-test suite green across all 4 dimensions.
6. 5-leg gate green (Legs 1-6) + exit-test green (a stricter superset).
7. Engine source delta vs v3.5.1 = 0 lines under `FrameCore/`. CUDA path delta = 0.
8. RELEASE notes' "Honest boundaries" section adds the v3.6 visualisation caveats
   (along-span D/C is allowable-screen, not ultimate-state, etc.).
9. `CLAUDE.md` carries the FROZEN marker; future PRs against `Plugins/FrameSolver/
   Source/FrameCore/` require an explicit CLAUDE.md amendment first.
10. `gh release` is Latest; URL works; release notes render.

## Risks + mitigations

- **Chaos GeometryCollection API churn (Phase 6)**: ship behind
  `bUseChaosPOD` runtime toggle defaulting to FALSE (v3.5 StaticMesh path).
  Designers opt in when Chaos works on their UE setup.
- **Exit-test runtime cost (Phase 11)**: large-fixture benchmarks may take 30+ min
  on the integrator's host. Make exit-test opt-in via `-RunExitTests` flag to
  `run_gate.ps1`; release-time it runs once.
- **14-agent context budget**: stagger launches (7 + 7 with 60s gap) so notifications
  arrive in two waves rather than collapsing.
- **Real-time live dyn-collapse threading (Phase 10)**: engine's `onFrameEmitted`
  callback may fire from any thread; UE Tick reads must be lock-protected. Use
  `TQueue<FFrameDynCollapseFrame, EQueueMode::Mpsc>` lock-free single-consumer.
- **"Last release" lock-in**: a critical post-v3.6 bug in production would need
  v3.6.1. Mitigation: v3.6 explicitly admits the hard-frozen contract is the
  engine, not the surface — a v3.6.1 may patch consumer-side surface only.

## Timeline contract

This is a single release session. No mid-progress HANDOFF; no partial ship.

When the integrator says "v3.6 is done", what they mean is:
- All 13 phases landed.
- Exit-test green.
- 14-agent audit clean.
- Tag pushed.
- gh release created.
- v3.6 is Latest.
- CLAUDE.md FROZEN marker in place.
- This document marked "LANDED v3.6.0" at the top.
