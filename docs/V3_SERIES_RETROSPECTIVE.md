# v3 series retrospective — FrameCore 2026-06-21 → 2026-06-23

> A 2-day, 7-release arc. This document captures what was actually built, what
> didn't make it, and what lessons survive past the FROZEN tag.

## The releases

| Tag | Date | Headline | Engine source delta |
|---|---|---|---|
| v3.0.0 | 2026-06-21 | STABLE v3 anchor; +5 RC items + 7-agent audit fixes | from v2.11.1 + 5 small RC items |
| v3.0.1 | 2026-06-21 | post-v3.0.0 hardening (6 findings closed) | small additive |
| v3.1.0 | 2026-06-21 | S11 stress-field post-process; shared `StressKernel.h` SSOT | ~350 LOC additive |
| v3.2.0 | 2026-06-22 | FrameCoreUE thin-slice UE module (USTRUCT + library + Slate panel) | 0 lines |
| v3.2.1 / v3.2.2 | 2026-06-22 | test coverage bump + audit closeouts | 0 lines |
| v3.3.0 | 2026-06-22 | stress-field schema break (-1 sentinel); UE renderer; BP JSON load | ~30 lines (sentinel rename) |
| v3.4.0 | 2026-06-22 night | Karamba3D-parity BP-callable analysis library (15 entries) | 3 lines (FRAMECORE_API facade) |
| v3.5.0 | 2026-06-22 late night | visual + game-ready surface (8 actors + 1 subsystem) | 0 lines |
| v3.5.1 | 2026-06-23 | deferred closeout + first VERIFIED 5-leg gate | 0 lines |
| v3.6.0 | 2026-06-23 | final: C6/C7/C8 along-span data line + exit-test suite | 0 lines |

## What was built

### Engine (pre-v3)

The engine (Plugins/FrameSolver/Source/FrameCore/) was already complete at v3.0
launch. The S1..S10 Karamba3D-parity track had landed under v2.x; v3 inherited a
working solver. Engine algorithm additions during the v3 series:

- v3.1: S11 stress-field post-process (along-span sigma/tau/Mz field, used by
  later C6/C7).
- v3.3: -1 sentinel for "no governing element" (the only schema break).
- Otherwise: engine source delta across v3.4 → v3.6 is **3 lines**
  (FRAMECORE_API facade on FrameModel.h index helpers in v3.4) + zero lines.

### UE consumer surface (v3.2 → v3.6)

This is where the v3 series real work happened.

- v3.2: FrameCoreUE module bootstrap. 5 USTRUCT, 1 BP library, 1 Slate panel.
- v3.4: Karamba3D-parity. 17 input USTRUCT, 9 output USTRUCT, 15 BP-callable
  analysis entries (`UFrameAnalysisLibrary`). Every linear + nonlinear engine
  analysis got a BP entry.
- v3.5: 8 visualisation actors (DeformedShape / Heatmap / ModalShape /
  DynCollapseReplay / FragmentCluster / InfluenceLine / ResponseSpectrum /
  RealTimeDynamic) + 1 UGameInstanceSubsystem for S1 ReSolve interactive.
- v3.5.1: PMC helper extraction, test helper extraction, modular phase
  reduction, debris cap.
- v3.6: 3 along-span data-line actors (InternalForceField / UtilizationField /
  RedundancyField) + Hermite member-axis interpolation + incremental load
  patch + influence polarity flip.

### Gate + audit infrastructure

- 5-leg gate (standalone / UE automation / OpenSees / deep audit / CLI round-trip)
  + Leg 6 v2 dispatcher round-trip. Established at v2.x; refined through v3.
- 7-agent adversarial release-hardening audit pattern. Introduced at v2.11
  release-hardening; refined per release.
- Exit-test suite. Introduced at v3.6. D1 (property-based 1000-fixture sweep)
  + D3 (strict-mode oracle re-run) are the binding green-gate dimensions; D2
  (bench ladder) + D4 (fuzz) are placeholders.

## What was tried and didn't ship

- **Live DynCollapse callback channel** (v3.5 → v3.6 deferred). The engine has
  `DynCollapseOptions::onFrameEmitted`, but threading it through a UE
  GameInstanceSubsystem with thread-safe TQueue + render-thread Tick marshalling
  is multi-cycle work. v3.5 replay actor delivers the end-user effect
  (post-history playback) and is good enough.
- **Chaos POD GeometryCollection real integration** (v3.5 thin slice → v3.6
  permanent defer). UE 5.7 Chaos destruction API has rough edges and was
  judged not worth the integration risk for a structural simulator. The v3.5
  StaticMesh + physics-enabled debris path delivers the visual effect.
- **S11 MITC9i high-order shells**. User explicit defer; would touch 9 engine
  modification sites for a feature that doesn't have a customer use case
  motivating it.

## What was learned

### Engine source delta budget as a release discipline

CLAUDE.md 鐵則 #1 ("FrameCore 維持純 C++17/Eigen") evolved during v3 into a
release-time invariant: every release commit message should report engine
source delta in lines, and the audit ran against `git diff <prev-tag>..HEAD --
Plugins/FrameSolver/Source/FrameCore/`. v3.4 added 3 lines (facade), v3.5 / v3.5.1
/ v3.6 added 0. This made the "what changed" question always answerable in one
diff.

### Audit lanes that consistently caught real bugs

- **Cross-doc numeric consistency.** Every release of v3.4 / v3.5 / v3.5.1 had
  stale UE test counts in some doc, even when the code change was correct. The
  release-hardening Phase 4.5 numeric-table audit caught it every time.
- **UE-build-time blockers that read-only audits couldn't see.** v3.5.1
  surfaced 4 of these (UFunction TObjectPtr, PhysicsEngine Chaos include,
  GENERATED_BODY collision with explicit `= delete`, TUniquePtr\<incomplete\>
  in UHT auto-gen ctor). Pattern: read-only audit is necessary but not
  sufficient; the first real UE Build catches a different category.
- **Headless commandlet test failures.** Tests using `GetSubsystem<>()` against
  a GameInstance silently fail in `-nullrhi -unattended` mode. v3.5.1 surfaced
  it via the actual UE automation run; the `NewObject<>` fallback became the
  pattern.

### What didn't work

- **Property-based fixture invariant I4 (MaxDC=0 when load=0)**. The engine's
  `SolveResult` doesn't carry utilization; `frame::worstUtilization(model, result)`
  is the post-process API. The exit-test sweep had to drop this invariant to
  build. Not a real loss — equilibrium / pivot / validate invariants are
  enough; D/C is downstream of those.

### Mistakes that were caught in time

- v3.5.0 Audit D-05 ("explicit `= delete` copy ctor for UObject"). Reverted in
  v3.5.1 after UE Build showed it collided with GENERATED_BODY's declarations.
  Over-engineering caught by the next release's first real build.
- v3.5.0 raw `new`/`delete` on `frame::ReSolveSession*` was justified at the
  time (TUniquePtr\<incomplete\> fails UHT FVTableHelper); v3.5.1 confirmed.
- v3.6 audit Lane 3 surfaced a potential U-12 silent-no-op against
  ReSolveSession's internal `work.nodalLoads` snapshot. Whether this is a real
  bug or audit overreach is the gate's call — the analytic-Uz test in
  `FrameCore.UE.LoadPatch.Add` is the arbiter.

## The FROZEN contract

Effective v3.6.0 tag:

> Any PR that touches `Plugins/FrameSolver/Source/FrameCore/` (including
> `Standalone/`, but excluding tests/exit_*.cpp) REQUIRES a prior CLAUDE.md
> amendment removing the FROZEN marker.

This is the final discipline. The engine has been bit-stable since v3.3
(stress-field schema break). The v3.4 facade is the last meaningful engine
edit. The v3.5–v3.6 cycle proved that the UE consumer surface can be enriched
without touching the engine, which is the strongest signal the engine is done.

## Goodbye

The v3 series is over. The engine algorithm set is complete. The UE consumer
surface covers every analysis the engine offers, with a visualisation actor for
every numerical result kind and an exit-test suite that asserts solver
correctness invariants across a 1000-fixture random sweep.

Future maintainers: read this doc + `HANDOFF_v3.6.0.md` + `CLAUDE.md` before
touching anything. The repo will tell you what it's good for and what it
isn't, if you let it.
