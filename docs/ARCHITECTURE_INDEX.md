# Architecture Index — Architect Simulator

> **Purpose:** Cheap-to-consult registry of what exists in this repo. Read
> the relevant section before writing new code/docs/tests to avoid duplicating
> existing surface. **Not** a tutorial — see linked docs for depth.
>
> **Owner:** session-driver skill (manager thread). Subagents read only.
> Updated at Phase 6 of every release-hardening cycle.
>
> **Latest tag:** v0.3.1 (Sprint S-04 patch close — S-03 carryover AS-20 + AS-24 + 7 cosmetic NITs + outside-repo hook fix) — see [`docs/RELEASE_v0.3.1.md`](RELEASE_v0.3.1.md) for changelog + [`docs/HANDOFF_v0.3.1.md`](HANDOFF_v0.3.1.md) for S-05 first-actions + [`docs/logs/S-04/manager.md` § SESSION CLOSE](logs/S-04/manager.md) for retrospective + durable lessons + S-05 recommended scope (Path A spike vs Path B consolidate)
> **Prior tags this minor:** v0.3.0 (Sprint S-03 close — hardening + PIE-world test harness foundation) / v0.2.0 (Sprint S-02 close — ALS pawn end-to-end) / v0.1.5 / v0.1.4 (patch bundles)

---

## 1. Layer diagram

Three layers, strict one-way dependency. Crossing FROZEN boundaries requires
CLAUDE.md amendment (see iron rule #1 in §9).

```
┌────────────────────────────────────────────────────────────────────────┐
│ ArchSim game body            (Source/ArchSim/)                         │
│   • UArchSimMemberData (component)                                     │
│   • UArchSimModelRegistry (GameInstanceSubsystem)                      │
│   • Tests under ArchSim.* namespace                                    │
│   • NOT FROZEN — game-body evolution is the active development surface │
└──────────────────────────────┬─────────────────────────────────────────┘
                               │ depends on (via FrameCoreUE)
                               ▼
┌────────────────────────────────────────────────────────────────────────┐
│ FrameCoreUE                  (Plugins/FrameSolver/Source/FrameCoreUE/) │
│   • USTRUCT / UCLASS / Library — UE-side BP-callable surface           │
│   • Visual actors, subsystems, editor panels                           │
│   • Tests under FrameCore.UE.* namespace                               │
│   • NOT FROZEN — UE consumer surface evolves under v4.0.x / v4.1.x     │
└──────────────────────────────┬─────────────────────────────────────────┘
                               │ depends on (via FRAMECORE_API)
                               ▼
┌────────────────────────────────────────────────────────────────────────┐
│ FrameCore  [FROZEN since v4.0.0]                                       │
│   (Plugins/FrameSolver/Source/FrameCore/)                              │
│   • Pure C++17/Eigen structural engine                                 │
│   • Public API POD/std only, ZERO UE leak                              │
│   • Tests under FrameCore.* namespace (standalone) + FrameCore.UE.*    │
│   • ANY change requires CLAUDE.md amendment FIRST                      │
└────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────┐
│ LevelSim  [FROZEN since v2.2+1]    (Plugins/LevelSim/)                 │
│   • Independent surveying-level engine                                 │
│   • Standalone gate: level_gate.exe (115/115 PASS)                     │
│   • Separate plugin, no shared code with FrameCore                     │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. ArchSim game-body class map

### Production classes (currently existing)

| Path | Class | Purpose | First introduced |
|---|---|---|---|
| `Source/ArchSim/Public/Components/ArchSimMemberData.h` | `UArchSimMemberData` | UActorComponent linking a placed Actor to a FrameCore Member; 3 UPROPERTY(SaveGame): `MemberIdx`, `StructureGroupId`, `CachedUtilization` | v0.1 (A1-01) |
| `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` | `UArchSimModelRegistry` | UGameInstanceSubsystem holding FFrameModelDef; `RegisterMember`/`DeactivateMember`/`SetCurrentDemand`/`RequestSolve`; 150 ms debounce; `MaxRankBeforeRebaseline=96` bounds PendingRankAccumulation in RequestSolve (NOT register count — see §7 backlog AS-07 closure); 3 telemetry getters added v0.1.4 AS-10 | v0.1 (A1-02..A1-05) |
| `Source/ArchSim/Public/ArchSimGameInstance.h` | `UArchSimGameInstance` | UGameInstance + `FTickableGameObject`. Tick body detects `Registry->GetRegisteredCount()` delta since last frame and emits `RequestSolve(FFrameModelPatch{})` to bridge BeginPlay-time Member registrations into the solver (RegisterMember does NOT auto-trigger solve). 4 BP-pure getters: `GetTickCount` / `GetAccumulatedTime` / `GetLastSeenRegisteredCount` / `GetSolveTriggerCount`. `IsTickable()` three-condition AND. **Wired as `GameInstanceClass=/Script/ArchSim.ArchSimGameInstance` in `Config/DefaultEngine.ini`**. | v0.1.4 (AS-02a) + v0.1.5 (AS-02b) |
| `Source/ArchSim/Public/Characters/ArchSimCharacter.h` | `AArchSimCharacter` | `AAlsCharacter` subclass — production-grade third-person locomotion via the ALS-Refactored v4.17 state machine. Adds 5 Enhanced Input `TObjectPtr<UInputAction>` slots + 1 `TObjectPtr<UInputMappingContext>` (default null in CDO, BP assigns from `Content/Input/*` UAssets per `docs/INPUT_MAPPING.md`). 7 handler methods (HandleMove view-space / HandleLook / HandleJumpPressed+Released / HandleSprintPressed+Released / HandleCrouchToggle) wire to ALS state setters. 1 `TObjectPtr<UAlsCameraComponent>` Camera default subobject attached to mesh (Yaw=90 via `SetRelativeRotation_Direct`). `bUseControllerRotation*` all false. | v0.2.0 (AS-03a/b/c) |
| `Source/ArchSim/Public/ArchSimGameMode.h` | `AArchSimGameMode` | `AGameModeBase` subclass. `DefaultPawnClass = AArchSimCharacter::StaticClass()` so PIE spawns the ALS pawn. **Wired as `GlobalDefaultGameMode=/Script/ArchSim.ArchSimGameMode` in `Config/DefaultEngine.ini`**. | v0.2.0 (AS-03c) |

### Planned classes (with backlog ID; see §7 for status)

| Path | Class | Purpose | Backlog ID |
|---|---|---|---|
| (TBD when SPUD orchestration wires up) | (UE save-slot orchestration) | Connect `USpudSubsystem` to the registry's SaveGame surface | AS-08 |

### Tests (`Source/ArchSim/Private/Tests/`)

| File | Test class | Path | Asserts |
|---|---|---|---|
| `ArchSimSaveLoadTest.cpp` | `FArchSimSaveLoadRoundTripTest` | `ArchSim.Persistence.SaveLoadRoundTrip` | 21+ sub-assertions: SaveGame UPROPERTY roundtrip on 3 fields; `Member.Id == MemberIdx` contract pre/post (AS-A1-07, v0.1.1) |
| `ArchSimSaveLoadTest.cpp` | `FArchSimMaxRankCeilingTest` | `ArchSim.Persistence.MaxRankCeiling` | 7+ sub-assertions: 97 sequential Register; pins true production semantic (no register-count ceiling; MaxRankBeforeRebaseline=96 bounds PendingRankAccumulation in RequestSolve) (AS-07, v0.1.3) |
| `ArchSimRebaselineTest.cpp` | `FArchSimRebaselineCeilingTest` | `ArchSim.Persistence.RebaselineCeiling` | 7 sub-checks: strict `>` ceiling semantic (97th rank trips, not 96th); accumulator math; multi-rank single patch; empty-patch no-op; const-getter purity. Honest headless limitation (trip path unreachable via GI-null early-return) (AS-10, v0.1.4) |
| `ArchSimGameInstanceTest.cpp` | `FArchSimTickDriverSmokeTest` | `ArchSim.Integration.TickDriver` | 7 sub-checks: Tick telemetry increment (5-tick / 100-tick); IsTickable filter (CDO + bIsActive=false); LastSeen/SolveTrigger initial state; const-getter purity. Honest headless limitation (driver-loop branch unreachable; GetSubsystem returns null without GameInstance pipeline — deferred PIE fixture AS-13) (AS-02c, v0.1.5) |
| `ArchSimCharacterTest.cpp` | `FArchSimCharacterClassSmokeTest` | `ArchSim.Gameplay.CharacterInput` | 7 sub-checks (24 assertions): class hierarchy AAlsCharacter/ACharacter/APawn; AArchSimGameMode inherits AGameModeBase; DefaultPawnClass==AArchSimCharacter; AS-03a bUseControllerRotation* all false; AS-03c UAlsCameraComponent default subobject named "Camera"; AS-03b 6 Enhanced Input UPROPERTY slots null in CDO; LogArchSim link symbol; reflection GetName. Honest headless limitation (Enhanced Input + ALS state machine + camera attachment + actor movement deferred to AS-13 PIE fixture) (AS-03d, v0.2.0) |

---

## 3. FrameCoreUE plugin surface quick-ref

`Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/` — 26 public headers.

### Visual Actors (BP-callable, drop into level)

`AFrameDeformedShapeActor`, `AFrameUtilizationHeatmapActor`,
`AFrameModalShapeActor`, `AFrameDynCollapseReplayActor`,
`AFrameFragmentClusterActor`, `AFrameInfluenceLineActor`,
`AFrameResponseSpectrumActor`, `AFrameRealTimeDynamicActor`,
`AFrameInternalForceFieldActor`, `AFrameUtilizationFieldActor`,
`AFrameRedundancyFieldActor`, `ACoreStressFieldActor`

### Subsystems

- `UFrameInteractiveSubsystem` — GameInstanceSubsystem wrapping `frame::ReSolveSession`
  for 60 fps re-analysis target

### Blueprint Libraries

- `UFrameCoreUEAnalysisLibrary` (15 BP entries: SolveLinear / AnalysisModal /
  AnalysisBuckling / LoadCombineEnvelope / InfluenceLine / ResponseSpectrum /
  RealTimeDynamic / ReanalysisSolve / SolvePDelta / SolveTensionOnly /
  SolveSizeOpt / SolveBESO / SolveCorotational / SolveArcLength / SolveDynCollapse)
- `UFrameCoreUELibrary` (utility helpers)
- `UFrameMaterialLibrary` (8 presets + custom)
- `UFrameSectionLibrary` (Rectangular / Circular factories)
- `UFrameModelBuilder` (`ValidateModel` / `LoadModelFromJson`)
- `UFrameCoreStressFieldLibrary` (stress field BP accessors, v3.2.0)

### USTRUCT (input + output types)

- `FrameCoreUEModelTypes.h` — 17 input USTRUCT (Material/Section/Node/Member/
  Shell/3 loads + SolveOptions + 7 analysis options + `FFrameModelDef` aggregate)
- `FrameCoreUEResultTypes.h` — 9 output USTRUCT (`FFrameSolveResult` +
  sub-types; **note: `FFrameDemandSummary` has `MaxDC` not `MaxMemberDC`**,
  with `SafetyFactor` real)
- `FrameCoreUEAnalysisTypes.h` — analysis options USTRUCT
- `FrameCoreUEVisualTypes.h` — visual sample types (stress field, etc.)
- `FrameCoreUETypes.h` — common base types

### Editor surface

- `SFrameCoreStressFieldPanel.h` — Slate nomad-tab panel registered in
  `WorkspaceMenu::GetMenuStructure().GetToolsCategory()` (v3.2.0)

---

## 4. External plugin entry points

| Plugin | Path | Entry `.uplugin` | Role |
|---|---|---|---|
| ALS-Refactored v4.17 | `Plugins/ALS/` | `ALS.uplugin` | Advanced Locomotion System — character movement (AAlsCharacter) |
| Prefabricator UE5 | `Plugins/Prefabricator/` | `Prefabricator.uplugin` | Create reusable prefabs in UE5 |
| SPUD | `Plugins/SPUD/` | `SPUD.uplugin` | Streaming-aware persistence + save-game system (`USpudSubsystem`) |
| SUQS | `Plugins/SUQS/` | `SUQS.uplugin` | Data-driven quest system |
| **LevelSim** (internal, FROZEN) | `Plugins/LevelSim/` | `LevelSim.uplugin` | Surveying-level simulator (`ALevelSimPawn`, `ALevelStaffActor`, `ALevelSimGameMode`, `ALevelSimHUD`) |
| **FrameSolver** (internal, FrameCore FROZEN) | `Plugins/FrameSolver/` | `FrameSolver.uplugin` | Structural engine (see §3 for UE-side surface) |

---

## 5. Data-flow snapshot

```
Player places Actor in world
        │
        ▼
UArchSimMemberData (BeginPlay)
        │  RegisterMember(Comp)
        ▼
UArchSimModelRegistry.Members[]  (FFrameModelDef builds up)
        │
        │  (every Tick / on-dirty)
        ▼
UArchSimModelRegistry.RequestSolve()
        │  (debounce 150 ms; rebaseline when PendingRankAccumulation > 96)
        ▼
UFrameInteractiveSubsystem.ApplyPatchAndResolve()
        │
        ▼
FrameCore::solveLoad()   [FROZEN engine]
        │
        ▼
FFrameSolveResult  (Displacements / MemberInternalForces / MemberUtilization / DemandSummary)
        │
        ▼
UArchSimModelRegistry.SetCurrentDemand(Result)
        │  per-member: writes Comp->CachedUtilization
        ▼
UArchSimMemberData.CachedUtilization  (BP-readable; UI/heatmap consumes)
```

**Wire status as of v0.2.0:**
- The "every Tick / on-dirty" arrow is **wired** via `UArchSimGameInstance::Tick`
  (AS-02b, v0.1.5). Tick detects `Registry->GetRegisteredCount()` delta and
  emits `RequestSolve(FFrameModelPatch{})`. Position-change sync is deliberately
  out of scope (demo MVP has static buildings).
- The "SetCurrentDemand → CachedUtilization" arrow is exercised end-to-end at
  the Tick → Registry → FrameInteractiveSubsystem boundary in production, but
  only the static (CDO/reflection) side is covered by the headless smoke test
  (`ArchSim.Integration.TickDriver` / `ArchSim.Gameplay.CharacterInput`).
  Full runtime driver-loop + trip-path observability is deferred to **AS-13**
  PIE-world fixture (see § 7).

### AS-18: Two-GameInstanceSubsystem teardown ordering

**Context (S-02 review finding C-04):** `UArchSimModelRegistry` and
`UFrameInteractiveSubsystem` are both `UGameInstanceSubsystem`s. UE tears them
down in **reverse-init order** — the exact order is not guaranteed by the game
code and depends on subsystem registration sequence at runtime.

**Both orderings are race-safe — verified against the production source:**

- **Registry deinitializes first** (`UArchSimModelRegistry::Deinitialize`,
  `ArchSimModelRegistry.cpp` `Deinitialize` body): calls `Sub->EndSession()` if
  `bSessionStarted`. `EndSession` on `UFrameInteractiveSubsystem` is idempotent
  (`if (Session) { delete Session; Session = nullptr; }` — double-call is a
  no-op). ✅ safe.

- **Sub (`UFrameInteractiveSubsystem`) deinitializes first**: Registry's
  `Deinitialize` calls `GetFrameSubsystem()` which calls
  `GetGameInstance()->GetSubsystem<UFrameInteractiveSubsystem>()`. UE returns
  `nullptr` for a subsystem that has already been deinitialized. The registry
  body guards with `if (UFrameInteractiveSubsystem* Sub = GetFrameSubsystem())`
  and skips the `EndSession` call. ✅ safe.

**Conclusion:** no ordering bug under current architecture. **If you add a new
cross-call between these two subsystems, or convert either to a
`UWorldSubsystem` (different teardown lifecycle), re-verify this analysis.**

---

## 6. UE test inventory

`IMPLEMENT_SIMPLE_AUTOMATION_TEST` count (as of v0.3.0):

| Namespace | Count | Source |
|---|---|---|
| `FrameCore.*` (standalone) | 60 | `Plugins/FrameSolver/Source/FrameCore/Private/Tests/` |
| `FrameCore.UE.*` (UE automation) | 76 | `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/` |
| `ArchSim.*` (game body) | 9 | `Source/ArchSim/Private/Tests/` |
| **5-leg gate total** | **145** (cuDSS) / **143** (non-cuDSS) | run via `Scripts/run_gate.ps1 -RequireOpenSees` |

**Recent additions:**
- v0.1.1: `ArchSim.Persistence.SaveLoadRoundTrip`
- v0.1.3: `ArchSim.Persistence.MaxRankCeiling`
- v0.1.4: `ArchSim.Persistence.RebaselineCeiling` (AS-10; pins strict `>` semantic of MaxRankBeforeRebaseline=96 in RequestSolve; 7 sub-checks including accumulator math, boundary 96 stays/97 grows, const-getter purity, multi-rank patch, empty-patch no-op; note: trip path unreachable in headless NewObject fixture due to GI-null early-return — this is honest per AS-07 lesson #1)
- v0.1.5: `ArchSim.Integration.TickDriver` (AS-02c; UArchSimGameInstance Tick telemetry + IsTickable filter smoke; 7 sub-checks; headless cannot exercise full registry-delta driver-loop branch because GetSubsystem returns null without a real GameInstance pipeline — deferred to PIE-world fixture as AS-13)
- v0.2.0: `ArchSim.Gameplay.CharacterInput` (AS-03d; AArchSimCharacter + AArchSimGameMode CDO/reflection smoke; 7 sub-checks covering class hierarchy, GameMode wire, AS-03a controller-rotation flags, AS-03c camera default subobject, AS-03b Enhanced Input UPROPERTY slots; full input + locomotion runtime deferred to AS-13)
- v0.3.0: `FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession` (AS-17; FrameInteractiveSubsystem empty-model graceful-fail oracle; 10 TestXxx assertions in 4 logical sub-checks — fully empty / partial empty / recovery after failure / double EndSession idempotency; engine validate() returns `"no nodes"` → diagnostic `"invalid model: no nodes"` → existing `if (!Session->valid())` guard fires)
- v0.3.0: `ArchSim.Integration.PieHarnessSmoke` (AS-13-u1; PIE-world bootstrap helper self-verification; 8 sub-checks proving the proven `GEngine->GetWorldContexts()` pattern works in `-nullrhi` commandlet, three-level coverage contract honest about Level 3 fallback; `SpawnActor<AArchSimCharacter>` confirmed to succeed in commandlet)
- v0.3.0: `ArchSim.Integration.PieRebaseline` (AS-13-u2; harness-based rebaseline accumulator validation; 7 sub-checks pinning 96 boundary + 97 no-reset math; honest defer of trip-path to Level 1/2 OR true PIE)
- v0.3.0: `ArchSim.Integration.PieDriverLoop` (AS-13-u2; harness-based driver-loop observation; 7 sub-checks pinning World non-null + GI null in Level 3 + Tick safety + SolveTriggerCount stays 0; honest defer of full driver-loop firing)
- v0.3.0: `ArchSim.Gameplay.PieInputRuntime` (AS-13-u2; harness-based runtime character state — genuine new coverage; 7 sub-checks pinning instance Camera != null (vs CDO null), controller-rotation flags, IA UPROPERTY slot contract, DestroyActor + IsValid lifecycle; full input/ALS state runtime deferred to true PIE because `SetupPlayerInputComponent`/`NotifyControllerChanged` are protected virtual)

**Namespace convention for new tests:**
- ArchSim tests: `ArchSim.<Category>.<TestName>` where Category ∈
  {`Persistence`, `Integration`, `Gameplay`, `UI`, `Multiplayer`}
- FrameCore tests stay in `FrameCore.*` / `FrameCore.UE.*` (engine FROZEN)

---

## 7. Backlog status (AS-XX live)

| ID | Title | Status | Where to find first action |
|---|---|---|---|
| AS-01 | `run_gate.ps1` ArchSim namespace coverage | ✅ closed v0.1.2 | (closed) |
| AS-02 | A1-06 full integration (Tick + sync + BP) | ✅ closed v0.1.5 (Tick driver = registered-count delta; position sync deferred to AS-13 PIE fixture) | (closed) |
| AS-03 | A2-01 ALS pawn integration | ✅ closed v0.2.0 (a/b/c/d: subclass AAlsCharacter + Enhanced Input + ALSCamera + GameMode + headless smoke; full input runtime deferred to AS-13 PIE fixture) | (closed) |
| AS-04 | Gate 0 UE Editor Plugins panel visual | 🟡 open (human) | HANDOFF_v0.1.3.md §4 #3 |
| AS-05 | K1-T2 / K4 art assets | 🟡 open (parallel) | HANDOFF_v0.1.3.md §4 #4 |
| AS-06 | SPUD UE5.5 StructUtils deprecation | 🔵 deferred (pre-5.8 upgrade) | HANDOFF_v0.1.3.md §4 #5 |
| AS-07 | A1-07 MaxRank ceiling stress test | ✅ closed v0.1.3 (with spec correction) | (closed) |
| AS-08 | SPUD orchestration `RF_Transient` audit | 🟡 open (when wiring SPUD) | HANDOFF_v0.1.3.md §4 #6 |
| AS-09 | Re-verify gate on non-cuDSS host | 🔵 deferred (opportunistic) | HANDOFF_v0.1.3.md §4 #7 |
| AS-10 | Genuine PendingRankAccumulation ceiling test | ✅ closed v0.1.4 (headless fixture with honest limitation notice; getter telemetry added to header; 7 sub-checks; trip path requires live GI — deferred to future PIE-world test) | (closed) |
| AS-11 | Header comment precision for rebaseline reset points | ✅ closed v0.3.0 (LOW-batch-u1; 6 stale `cpp:NNN` line-refs all rewritten to stable form `see RequestSolve body` / `see ExecuteSolve top + 3 early-exit paths` to avoid future drift) | (closed) |
| AS-12 | `GetMaxRankBeforeRebaseline()` production consumer | ✅ closed v0.3.0 (LOW-batch-u1; TODO comment added above accessor noting HUD rank-budget indicator is the intended caller, out-of-S-03 scope; legitimate backlog'd TODO) | (closed) |
| AS-13 | PIE-world fixture for driver-loop + trip-path observability | ✅ closed v0.3.0 (u1 ships harness using proven `GEngine->GetWorldContexts()` pattern; u2 ships 3 harness-based tests — `PieRebaseline` + `PieDriverLoop` + `PieInputRuntime` — honest Level 3 defer for trip-path + driver-loop, genuine new coverage for spawn-time character state) | (closed) |
| AS-14 | Analog stick / gamepad input ClampMagnitude012D normalization | ✅ closed v0.3.0 (LOW-batch-u1; `UAlsVector::ClampMagnitude012D(Value.Get<FVector2D>())` substituted in `HandleMove`; ALS API signature verified via 3-point grep) | (closed) |
| AS-15 | Enhanced Input lifecycle refit (NotifyControllerChanged + RemoveMappingContext + Canceled + bNotifyUserSettings) | ✅ closed v0.3.0 (AS-15-u1; mirror of ALS `AlsCharacterExample.cpp:19-49`; closes A-02/D-01/D-02/D-03/D-06 hardening findings; ~50 LOC net) | (closed) |
| AS-16 | CalcCamera override for ALSCamera pipeline | ✅ closed v0.3.0 (AS-16-u1; routes through `UAlsCameraComponent::GetViewInfo` per ALS L51-60; `IsValid(Camera)` defensive prefix; ~8 code LOC) | (closed) |
| AS-17 | empty-CurrentModel StartSession behavior audit | ✅ closed v0.3.0 (AS-17-u1; Case A no-guard-needed; engine `validate()` returns `false "no nodes"` → existing `if (!Session->valid())` guard fires; new test `FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession` pins the contract; renamed S-04 PHASE5-NITS-u1 NIT-f for namespace parity) | (closed) |
| AS-18 | Two-GameInstanceSubsystem teardown order documentation | ✅ closed v0.3.0 (LOW-batch-u1; ~30-line paragraph added to §5 documenting why both teardown directions are race-safe via `EndSession` idempotency + `GetFrameSubsystem` null guard) | (closed) |
| AS-19 | `UArchSimMemberData::BeginPlay` early-out warn/retry | ✅ closed v0.3.0 (LOW-batch-u1; Option A warn-only via `UE_LOG(LogTemp, Warning, ...)`; Option B retry-via-timer rejected at 35-45 LOC > 30 LOC threshold) | (closed) |
| AS-20 | Upgrade `LogTemp` → shared `LogArchSim` log category | ✅ closed S-04 Round 1 (commit `4b6f094`; 3-site sweep `ArchSimMemberData.cpp:26` + `ArchSimSaveLoadTest.cpp:86,294` using pre-existing umbrella) | (closed) |
| AS-24 | FrameCoreUE NewObject outer for InteractiveSubsystem isolated runs | ✅ closed S-04 Round 1 (commit `2883d40`; 3-site mechanical `GetTransientPackage()` outer; honest disclosure — UE5.7 `UObjectGlobals.h:1918` confirms default outer is already `GetTransientPackage()`, fix value is intent-clarity + comments) | (closed) |
| AS-25 | Hook regex broaden for `S-XXa` suffix sprints | 🟡 backlog (LOW; current `^S-\d+$` in `~/.claude/hooks/work-phase-guard.ps1` won't match suffix sprints; defer to future maintenance — no current convention uses suffix) | docs/logs/S-04/manager.md Round 1 review |
| AS-26 | `UArchSimModelRegistry` ClassWithin verify + ArchSimPieHarness NewObject outer mirror | 🟡 backlog (MEDIUM; HANDOFF_v0.3.0.md §4 AS-24 said "should also adopt the same pattern" but excluded from S-04 AS-24-u1 scope; needs UArchSimModelRegistry ClassWithin confirm then mirror at `ArchSimPieHarness.cpp:81`) | docs/logs/S-04/manager.md Round 1 review |
| AS-27 | Stale doc references in ARCH_INDEX §8 + DriverLoopTest sub-check 1 comments | 🟡 backlog (LOW; cosmetic — §8 gate cheat-sheet still says `140 expected / 138 on non-cuDSS` (current 145/143; pre-existing); DriverLoopTest.cpp L54+L58 sub-check 1 comments have empirical-overclaim `always has`/`always provides`) | docs/logs/S-04/manager.md Round 2 review |

---

## 8. Gate command cheat-sheet

```powershell
# Build UE editor (run after any Source/ArchSim/ or Plugins/FrameSolver/Source/FrameCoreUE/ change)
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# 5-leg gate (default 140 expected; pass 138 on non-cuDSS host)
.\Scripts\run_gate.ps1 -RequireOpenSees

# Single UE test (replace path)
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests <test.path>; Quit" `
    -unattended -nullrhi -log

# Optional 6th leg: CUDA / cuDSS gate
.\Scripts\run_gpu_gate.ps1 -Strict

# Exit-test suite (D1 property / D3 strict-mode oracle)
.\Scripts\run_exit_tests.ps1
```

---

## 9. Iron rules (verbatim from the project root `CLAUDE.md`)

1. **FrameCore 維持純 C++17/Eigen** — public API POD/std only, zero UE leak,
   zero Eigen leak (Eigen only in `Private/FrameEigen.h` + `PreparedSystemImpl.h`,
   `FRAMECORE_UE` dual-build; `PreparedSystem` is PIMPL opaque). Cross-DLL
   symbols tagged `FRAMECORE_API`.
   - **[FROZEN since v4.0.0]** `Plugins/FrameSolver/Source/FrameCore/` engine
     source permanently frozen. Any PR touching this path **must first do a
     CLAUDE.md amendment** removing the FROZEN marker (with explicit reason).
     `Plugins/FrameSolver/Source/FrameCoreUE/` (UE consumer side) **not** in
     FROZEN scope; still evolves under v4.0.x patch / v4.1.x minor.
2. **Any FrameCore change must pass the 5-leg gate** — standalone F1..F71,
   UE 145 (cuDSS) / 143 (non-cuDSS) as of v0.3.0, OpenSees strict,
   linear_deep_audit 104, CLI round-trip. Run before commit:
   `Scripts\run_gate.ps1 -RequireOpenSees`.
3. **Honest verify, no over-claiming** — every capability has an independent
   oracle (analytic / dense / OpenSees / sympy-numpy). `[NEW CODE]` vs
   `[VERIFIED]` honest grading. Textbook methods don't claim novelty.
4. **Index not raw pointer** — `Member`/`ShellQuad` hold `matIdx`/`secIdx`
   (indices into `FrameModel::materials/sections`), never raw pointers.
   `validate()` does range-checks.
5. **Commit hygiene** — explicit `git add` per file, **NEVER `-A` / `.`**.
   **NEVER** touch `.gitignore`, `ArchSim.uproject`, `Plugins/LevelSim/`,
   build artifacts. Run full 5-leg gate before commit.

**FROZEN paths (exact):**
- `Plugins/FrameSolver/Source/FrameCore/` (since v4.0.0)
- `Plugins/LevelSim/Source/LevelCore/` (since v2.2+1)

**Never-touch paths:**
- `.gitignore`
- `ArchSim.uproject`
- `Plugins/LevelSim/*` (entire dir, even Public/)
- Any `*.dll` / `*.exp` / `*.lib` / `*.obj` / `*.pdb` / `*.exe`

---

## 10. Quick links

- Engine architecture deep-dive: [`ARCHITECTURE.md`](ARCHITECTURE.md)
- Game-body master plan: [`ARCHITECT_SIM_MASTER_PLAN.md`](ARCHITECT_SIM_MASTER_PLAN.md)
- Implementation breakdown (1899h): [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md)
- Per-sprint execution: [`SPRINT_NOTES.md`](SPRINT_NOTES.md)
- Latest release notes: [`RELEASE_v0.2.0.md`](RELEASE_v0.2.0.md)
- Latest handoff: [`HANDOFF_v0.2.0.md`](HANDOFF_v0.2.0.md)
- Sprint S-02 logs (manager + 8 agent logs + scope + plan): [`logs/S-02/`](logs/S-02/)
- Engine verification map: [`VERIFICATION.md`](VERIFICATION.md)
- CLI protocol: [`CLI_PROTOCOL.md`](CLI_PROTOCOL.md)
- Karamba3D parity roadmap: [`KARAMBA3D_ROADMAP.md`](KARAMBA3D_ROADMAP.md)
- v3.x series retrospective: [`V3_SERIES_RETROSPECTIVE.md`](V3_SERIES_RETROSPECTIVE.md)
- Per-sprint logs (new, session-driver maintained): `logs/S-XX/`

---

*This file is rewritten surgically by session-driver Phase 6 after every*
*release-hardening cycle. If you find it stale, that means session-driver*
*broke convention — raise as AS-XX item.*
