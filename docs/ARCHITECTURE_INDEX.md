# Architecture Index — Architect Simulator

> **Purpose:** Cheap-to-consult registry of what exists in this repo. Read
> the relevant section before writing new code/docs/tests to avoid duplicating
> existing surface. **Not** a tutorial — see linked docs for depth.
>
> **Owner:** session-driver skill (manager thread). Subagents read only.
> Updated at Phase 6 of every release-hardening cycle.
>
> **Latest tag:** v0.5.1 (Sprint S-07 close — AS-35 PIE auto-smoke 6-leg gate: NEW `ArchSim.PIE.PortalFrameSmoke` C++ Automation Test via UE Automation framework + LatentCommands, NEW `Scripts/run_pie_gate.ps1` 6th-leg wrapper, M `Scripts/run_gate.ps1` leg 2 filter Option A category enumeration + leg 6 invocation block) — see [`docs/RELEASE_v0.5.1.md`](RELEASE_v0.5.1.md) + [`docs/HANDOFF_v0.5.1.md`](HANDOFF_v0.5.1.md) + [`docs/logs/S-07/manager.md`](logs/S-07/manager.md) **§ SESSION CLOSE** for retrospective + durable lessons + S-08 candidate scope. **Auto leg 6 catches PortalFrame regression; USER-DRIVEN PIE smoke per `docs/logs/S-05/u3_pie_smoke.md` P1..P15 STILL canonical for「ready for student trial」judgement** (per `pie-auto-smoke-preference` memory: automate routine, keep gameplay-feel manual).
> **Prior tags this minor:** v0.5.0 (Sprint S-06 close — AS-30 + U-ALS + U-IWYU + U-LOW) / v0.4.0.1 (S-05 patch — AS-28 Scenario widget cross-world hotfix + IWYU first-header rebuild fix) / v0.4.0 (Sprint S-05 close — Scenario MVP minor bump; marked prerelease because PIE end-to-end flow broken by cross-world bug pre-v0.4.0.1) / v0.3.1 (Sprint S-04 patch close — S-03 carryover AS-20 + AS-24 + 7 cosmetic NITs + outside-repo hook fix) / v0.3.0 (Sprint S-03 close — hardening + PIE-world test harness foundation) / v0.2.0 (Sprint S-02 close — ALS pawn end-to-end) / v0.1.5 / v0.1.4 (patch bundles)

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
| `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` | `UArchSimModelRegistry` | UGameInstanceSubsystem holding FFrameModelDef; `RegisterMember`/`DeactivateMember`/`SetCurrentDemand`/`RequestSolve`; 150 ms debounce; `MaxRankBeforeRebaseline=96` bounds PendingRankAccumulation in RequestSolve (NOT register count — see §7 backlog AS-07 closure); 3 telemetry getters added v0.1.4 AS-10. **AS-30 (v0.5.0):** `RegisterFixedSupport(FVector PosMm) -> int32` — register a fully-fixed support node (Fixed=[T,T,T,T,T,T]); internally calls `FindOrAddNode` for 1 mm dedup + sets `FFrameNode.Fixed=all-true`; idempotent (repeated calls at same position re-confirm Fixed without duplicating node). **Does NOT auto-trigger Solve** — caller is expected to batch supports + members and rely on RegisterMember's 150 ms debounce timer for the eventual Solve kick (matches the original RequestSolve/RegisterMember separation pattern; see Registry header L68-69). | v0.1 (A1-02..A1-05) |
| `Source/ArchSim/Public/ArchSimGameInstance.h` | `UArchSimGameInstance` | UGameInstance + `FTickableGameObject`. Tick body detects `Registry->GetRegisteredCount()` delta since last frame and emits `RequestSolve(FFrameModelPatch{})` to bridge BeginPlay-time Member registrations into the solver (RegisterMember does NOT auto-trigger solve). 4 BP-pure getters: `GetTickCount` / `GetAccumulatedTime` / `GetLastSeenRegisteredCount` / `GetSolveTriggerCount`. `IsTickable()` three-condition AND. **Wired as `GameInstanceClass=/Script/ArchSim.ArchSimGameInstance` in `Config/DefaultEngine.ini`**. | v0.1.4 (AS-02a) + v0.1.5 (AS-02b) |
| `Source/ArchSim/Public/Characters/ArchSimCharacter.h` | `AArchSimCharacter` | `AAlsCharacter` subclass — production-grade third-person locomotion via the ALS-Refactored v4.17 state machine. Adds 5 Enhanced Input `TObjectPtr<UInputAction>` slots + 1 `TObjectPtr<UInputMappingContext>` (default null in CDO, BP assigns from `Content/Input/*` UAssets per `docs/INPUT_MAPPING.md`). 7 handler methods (HandleMove view-space / HandleLook / HandleJumpPressed+Released / HandleSprintPressed+Released / HandleCrouchToggle) wire to ALS state setters. 1 `TObjectPtr<UAlsCameraComponent>` Camera default subobject attached to mesh (Yaw=90 via `SetRelativeRotation_Direct`). `bUseControllerRotation*` all false. **S-06 U-ALS:** added `PostInitProperties()` + `BeginPlay()` overrides + `LoadAlsAssetsLate()` helper that wires Settings / MovementSettings / SkeletalMesh / AnimBlueprint via runtime-late `LoadObject<T>()` (CDO ConstructorHelpers fail because ALS plugin content not mounted at CDO time per `Saved/Logs/ArchSim-backup-2026.06.28-03.37.00.log:L916-929`). BeginPlay fallback calls helper BEFORE `Super::BeginPlay()` because ALS L138-146 fires `ALS_ENSURE(IsValid(Settings))` inside Super. Companion ALS plugin patch in `Tools/patches/als_l400_animinstance_guard.patch` adds null-guard to `AAlsCharacter::RefreshMeshProperties` L400 (untracked-third-party convention — fresh checkouts apply via `git apply`). | v0.2.0 (AS-03a/b/c) + v0.5.0 (S-06 U-ALS) |
| `Source/ArchSim/Public/ArchSimGameMode.h` | `AArchSimGameMode` | `AGameModeBase` subclass. `DefaultPawnClass = AArchSimCharacter::StaticClass()` so PIE spawns the ALS pawn. **Wired as `GlobalDefaultGameMode=/Script/ArchSim.ArchSimGameMode` in `Config/DefaultEngine.ini`**. | v0.2.0 (AS-03c) |
| `Source/ArchSim/Public/Editor/ArchSimScenarioWidget.h` | `UArchSimScenarioWidget` | `UEditorUtilityWidget` subclass, `UCLASS(Abstract)` — Editor-side Scenario MVP widget gated by `#if WITH_EDITOR` (whole class compiles out for packaged shipping builds). BP-callable surface: `PlaceK1Column` / `PlaceK2Beam` / `PlaceK4Brace` (K-set placement via private `PlaceKSetMember` shared helper) + `RequestSolveAndVisualize` (Registry debounced solve + lazy-spawn `AFrameUtilizationHeatmapActor` in PIE world) + `AdvanceTutorialStep` (linear 6-state `EArchSimTutorialState` machine, FreeExplore terminal) + `GetCurrentPromptText` (LOCTEXT namespace `ArchSimTutorial`) + `ResetWidgetState` (unsubscribe delegate + destroy HeatmapActor + clear PlacedActors list + reset state). Two `BlueprintImplementableEvent` stubs for BP overlay UI: `OnTutorialStateChanged(NewState, PromptText)` + `OnVoicePromptShouldPlay(PromptText)` — text-only, no third-party TTS SDK linked in C++. `WITH_EDITORONLY_DATA` UPROPERTYs: `HeatmapActor` (u2) / `TutorialState` (u3) / `PlacedActors` (u3). Explicit `Registry->OnSolveComplete.Remove(Handle)` in `BeginDestroy` via cached `TWeakObjectPtr<UArchSimModelRegistry> SubscribedRegistry`. Production class instantiation is via a Blueprint child class (`UCLASS(Abstract)`). **AS-30 (v0.5.0, WITH_EDITOR guarded):** `PlaceFixedSupport(FVector LocationWorld) -> int32` (PIE-world preferred, world→mm ×10, delegates to `Registry::RegisterFixedSupport`) + `SpawnDefaultPortalFrame() -> bool` (2 fixed supports + 2 K1 columns + 1 K2 beam at 2x2x2 m portal; graceful-fail returns false when no PIE Registry). | v0.4.0 (SPIKE-Scenario-u1/u2/u3) |

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
| `ArchSimScenarioWidgetTest.cpp` | `FArchSimScenarioWidgetSmokeTest` | `ArchSim.Gameplay.ScenarioWidget` | 7 sub-checks: class hierarchy `IsChildOf(UEditorUtilityWidget)`; CDO non-null via `NewObject<>()`; `PlaceK1Column` BP-callable UFunction reflection + signature returns `AActor*` + takes 1 `FVector`; `WITH_EDITOR` macro defined in test compile path; K1 placeholder Actor class = `AActor`; `UArchSimMemberData` component class metadata; honest-defer placeholder for full PIE placement (sub-check 7 documented as AS-13 precedent). End-to-end `PlaceK1Column → MemberData attach → Registry RegisterMember` deferred to u3 PIE fixture (SPIKE-Scenario-u1, v0.4.0) |
| `ArchSimScenarioSolveWireTest.cpp` | `FArchSimScenarioSolveWireTest` | `ArchSim.Gameplay.ScenarioSolveWire` | 7 sub-checks: `RequestSolveAndVisualize` UFunction reflection (return `FBoolProperty` + 0 input params); `HeatmapActor` UPROPERTY type `FObjectPropertyBase` with `PropertyClass.IsChildOf(AFrameUtilizationHeatmapActor)`; `BuildMemberGeometryFromRegistry(nullptr)` contract verified indirectly via sub-check 5/6 (helper is private static); `RequestSolveAndVisualize()` on transient widget returns `false` + `HeatmapActor` remains `nullptr` (graceful-fail without Registry); BeginDestroy lifecycle safe; honest-defer for full PIE solve→delegate→heatmap chain (SPIKE-Scenario-u2, v0.4.0) |
| `ArchSimScenarioTutorialTest.cpp` | `FArchSimScenarioTutorialTest` | `ArchSim.Gameplay.ScenarioTutorial` | 8 sub-checks: `PlaceK2Beam` + `PlaceK4Brace` UFunction reflection (return `AActor*` + 1 `FVector`); `EArchSimTutorialState` UENUM 6 named values + count verification with defence-in-depth comment; `TutorialState` UPROPERTY BlueprintReadOnly + initial `Welcome`; `AdvanceTutorialStep` linear transitions (Welcome → PromptPlaceK1 → ... → FreeExplore terminal); `GetCurrentPromptText` non-empty FText per state; `ResetWidgetState` headless safety (no crash); `OnTutorialStateChanged` + `OnVoicePromptShouldPlay` BlueprintImplementableEvent reflection with `FUNC_BlueprintEvent` flag (UE5.7 canonical; sub-check 8 has main-thread inline-fix carry-forward citing `UE_5.7\…\Script.h:163`). Full PIE solve→delegate→heatmap chain deferred to USER-DRIVEN PIE 5min smoke per `docs/logs/S-05/u3_pie_smoke.md` (SPIKE-Scenario-u3, v0.4.0) |
| `ArchSimScenarioFixtureTest.cpp` | `FArchSimScenarioFixtureTest` | `ArchSim.Gameplay.ScenarioFixture` | 7 sub-checks (SC1–SC7): SC1 `PlaceFixedSupport` UFunction reflection (BlueprintCallable + int32 return + 1 FVector param); SC2 `SpawnDefaultPortalFrame` UFunction reflection (BlueprintCallable + bool return + 0 params); SC3 Registry headless `RegisterFixedSupport(0,0,0)` → NodeIdx>=0 + `Fixed.Num()==6` + all-true; SC4 node-snap dedupe — second call at same pos returns same idx (FindOrAddNode 1 mm tolerance verified); SC5 idempotent Fixed — second call leaves Fixed length-6 all-true; SC6 transient widget graceful-fail — `SpawnDefaultPortalFrame()` no-crash when no PIE Registry; SC7 AddInfo PIE oracle link for P10/P11 smoke (`docs/logs/S-05/u3_pie_smoke.md` AS-30 update). PIE fixture (5 actors + heatmap) deferred to USER-DRIVEN smoke P10/P11 (AS-30, v0.5.0, S-06) |

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

`IMPLEMENT_SIMPLE_AUTOMATION_TEST` count (as of v0.5.1):

| Namespace | Count | Source |
|---|---|---|
| `FrameCore.*` (standalone) | 60 | `Plugins/FrameSolver/Source/FrameCore/Private/Tests/` |
| `FrameCore.UE.*` (UE automation) | 76 | `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/` |
| `ArchSim.*` (game body, headless legs 1-5) | 13 | `Source/ArchSim/Private/Tests/` |
| `ArchSim.PIE.*` (game body, **render-thread leg 6 only**) | 1 | `Source/ArchSim/Private/Tests/ArchSimPortalFramePIESmokeTest.cpp` |
| **6-leg gate total** | **149 leg 2 + 1 leg 6 = 150** (cuDSS) / **147 + 1 = 148** (non-cuDSS) | run via `Scripts/run_gate.ps1 -RequireOpenSees` (legs 1-5 from existing infra; leg 6 from `Scripts/run_pie_gate.ps1` since v0.5.1) |

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
- v0.4.0: `ArchSim.Gameplay.ScenarioWidget` (SPIKE-Scenario-u1; UArchSimScenarioWidget CDO + reflection smoke for `PlaceK1Column` BP-callable + WITH_EDITOR guard + K1 placeholder Actor class verification; 7 sub-checks; honest-defer of full PIE placement to u3 fixture per AS-13 precedent)
- v0.4.0: `ArchSim.Gameplay.ScenarioSolveWire` (SPIKE-Scenario-u2; `RequestSolveAndVisualize` BP-callable + `HeatmapActor` UPROPERTY reflection + graceful-fail without Registry + BeginDestroy lifecycle; 7 sub-checks; honest-defer of live PIE solve→delegate→heatmap chain)
- v0.4.0: `ArchSim.Gameplay.ScenarioTutorial` (SPIKE-Scenario-u3; K2/K4 BP-callable + `EArchSimTutorialState` UENUM 6-state + `AdvanceTutorialStep` linear transition + FreeExplore terminal + `GetCurrentPromptText` + `ResetWidgetState` headless safety + `OnTutorialStateChanged`/`OnVoicePromptShouldPlay` BlueprintImplementableEvent reflection via canonical UE5.7 `FUNC_BlueprintEvent` flag; 8 sub-checks; full USER-DRIVEN PIE 5min smoke per `docs/logs/S-05/u3_pie_smoke.md`)
- v0.5.0: `ArchSim.Gameplay.ScenarioFixture` (AS-30, S-06; `PlaceFixedSupport` + `SpawnDefaultPortalFrame` UFunction reflection SC1/SC2; Registry headless `RegisterFixedSupport` SC3; node-snap dedupe SC4; idempotent Fixed SC5; transient widget graceful-fail SC6; PIE oracle AddInfo SC7; 7 sub-checks; portal frame PIE fixture [NEW CODE, PIE required] per P10/P11 AS-30 update in `docs/logs/S-05/u3_pie_smoke.md`)
- v0.5.1: `ArchSim.PIE.PortalFrameSmoke` (AS-35, S-07; first member of NEW `ArchSim.PIE.*` Category — render-thread leg-6-only; `IMPLEMENT_COMPLEX_AUTOMATION_TEST` + 8-stage LatentCommand chain: `FStartPIECommand(false)` real PIE → `FWaitForMapToLoadCommand` → `FEngineWaitLatentCommand(1.0f)` → custom `FDrivePortalFrameSmokeCommand` (widget instantiation + `SpawnDefaultPortalFrame` + Registry node/member count verify + `RequestSolveAndVisualize`) → `FEngineWaitLatentCommand(0.5f)` → custom `FVerifyHeatmapSpawnedCommand` (best-effort `AddWarning` on miss per AS-36 Bug C tolerance) → custom `FSafeEditorScreenshotCommand` (Slate-free alternative to canonical `FTakeActiveEditorScreenshotCommand` which asserts in commandlet mode) → `FEndPlayMapCommand`. Test-local `WorldSettings->DefaultGameMode = AGameModeBase::StaticClass()` override sidesteps AS-37 ALS commandlet crash; production unchanged. Run via `Scripts/run_pie_gate.ps1` (NEW PowerShell wrapper, locale-defensive parser using ASCII-only `TEST COMPLETE. EXIT CODE: 0` as primary PASS signal, no `Tee-Object` capture to avoid NativeCommandError pollution). Screenshot artifact `Saved/Screenshots/WindowsEditor/v0_5_x_pie_smoke*.png`.)

**Namespace convention for new tests:**
- ArchSim tests: `ArchSim.<Category>.<TestName>` where Category ∈
  {`Persistence`, `Integration`, `Gameplay`, `UI`, `Multiplayer`, **`PIE`**}
- `PIE` category (added v0.5.1) is render-thread-only — runs in leg 6 of the
  gate (`Scripts/run_pie_gate.ps1`), NOT in leg 2 (which uses `-nullrhi`).
  Leg 2 filter explicitly enumerates `Persistence + Integration + Gameplay`
  to exclude `PIE`. When adding a new `ArchSim.<NewCategory>.*` test,
  decide leg-2 (headless) or leg-6 (PIE) and update the filter accordingly.
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
| AS-25 | Hook regex broaden for `S-XXa` suffix sprints | ✅ closed S-05 Round 1 (ceremonial OUTSIDE-repo; `^S-\d+$` → `^S-[\w]+$` in `~/.claude/hooks/work-phase-guard.ps1` L104 + WHY comment L91-102; 4-scenario stdin test PASS including new `S-04a` suffix scenario; no ArchSim commit) | (closed) |
| AS-26 | `UArchSimModelRegistry` ClassWithin verify + ArchSimPieHarness NewObject outer mirror | ✅ closed S-05 Round 1 (commit `26153c3`; 1-line + 11-line WHY comment at `ArchSimPieHarness.cpp:81` mirroring AS-24 pattern; `UCLASS(Abstract, Within = GameInstance, MinimalAPI)` at `GameInstanceSubsystem.h:15` confirmed explicit; honest disclosure that fix is intent-documentation since `NewObject<T>()` default outer is already `GetTransientPackage()` per `UObjectGlobals.h:1919`) | (closed) |
| AS-27 | Stale doc references in ARCH_INDEX §8 + DriverLoopTest sub-check 1 comments | ✅ closed S-05 Round 1 (commit `21a06d9`; ARCH_INDEX §8 `140/138` → `145/143`; DriverLoopTest L54-56 + L59 empirical phrasing mirroring NIT-a precedent at `ArchSimPieHarness.h:52`; no logic change) | (closed) |
| AS-28 | Hook case-sensitivity + .bak header comment sync | ✅ closed S-06 U-LOW — `-notmatch` → `-cnotmatch` at `~/.claude/hooks/work-phase-guard.ps1` L114; WHY comment added L104-112; `.bak` regenerated (production == .bak, 0-line diff); 5-scenario stdin test PASS including new Scenario 3b lowercase fail-open behaviour. OUTSIDE repo — no ArchSim commit. | `~/.claude/hooks/work-phase-guard.ps1` L114 |
| AS-29 | `run_gate.ps1` standalone leg PowerShell environment race diagnosis | 🟡 backlog (LOW) — AS-27-u1 subagent observed `[1/5] standalone: exit 1` under PowerShell session while direct `build.bat` ALL PASS; AS-26-u1 subagent on same host got `[1/5] standalone: ALL PASS` — likely shell-state / cwd / PATH race during parallel dispatches. Workaround: run `Plugins/FrameSolver/Standalone/build.bat` directly as fallback. | docs/logs/S-05/manager.md Round 1 AS-27-u1 review |
| AS-30 | Scenario valid-frame fixture + boundary support API | ✅ closed S-06 (v0.5.0) — `Registry::RegisterFixedSupport(FVector PosMm)` + `Widget::PlaceFixedSupport(FVector WorldCm)` + `Widget::SpawnDefaultPortalFrame()` shipped. 7-sub-check headless test `ArchSim.Gameplay.ScenarioFixture`. Portal frame PIE solve + heatmap = [NEW CODE, PIE required]; covered by u3_pie_smoke P10/P11 AS-30 update (S-06). | `Source/ArchSim/Private/Tests/ArchSimScenarioFixtureTest.cpp` |
| AS-35 | PIE auto-smoke via UE Automation Test framework + LatentCommands (C++) | ✅ closed S-07 (v0.5.1) — `Source/ArchSim/Private/Tests/ArchSimPortalFramePIESmokeTest.cpp` (NEW 443 LOC `IMPLEMENT_COMPLEX_AUTOMATION_TEST` + 8-stage LatentCommand chain) + `Scripts/run_pie_gate.ps1` (NEW 170 LOC PowerShell wrapper) + `Scripts/run_gate.ps1` (M +55/-19; leg 2 Option A category enumeration + new leg 6 block + [N/5]→[N/6] labels). 6-leg gate PASS exit 0 confirmed 2026-06-28. Engine source delta vs v0.5.0: 0 lines (FROZEN). Python `-ExecutePythonScript` path remains architecturally dead per v0.5.0 post-mortem memory. | `Source/ArchSim/Private/Tests/ArchSimPortalFramePIESmokeTest.cpp` + `Scripts/run_pie_gate.ps1` |
| AS-36 | `PlaceKSetMember` two-K1-column-same-node-pair bug (Bug C from AS-35-u1 commandlet PIE) | 🟡 backlog (MEDIUM) — commandlet PIE log SC2b shows `Member[0] I=2 J=3 / Member[1] I=2 J=3` (両柱共用 node pair) → LDLT rank-deficient → solve silently fails → HeatmapActor never spawns. Verify in user-driven PIE first (may be commandlet-only); then debug `PlaceKSetMember` — likely `FindOrAddNode` 1mm dedup misbehaviour OR `EndIOffsetUE`/`EndJOffsetUE` calculation error for stacked columns. spawn_task `task_8cf96d94` superseded. | `docs/HANDOFF_v0.5.1.md` § 4 + `docs/logs/S-07/agent_AS-35-u1.md` § "Adversarial review" finding 4 |
| AS-37 | ALS commandlet PIE crash audit | 🟡 backlog (MEDIUM) — `AArchSimCharacter` spawn in commandlet PIE → ALS `LoadObject<T>()` for plugin content (`SKM_Als`, `CS_Als_Default`, etc.) fails at PIE-pawn-spawn timing → `MovementSettings` null → `NotifyLocomotionModeChanged()` EXCEPTION_ACCESS_VIOLATION. User-driven PIE doesn't hit (Editor pre-mounts plugin content earlier). AS-35-u1 test sidesteps via test-local `WorldSettings->DefaultGameMode = AGameModeBase::StaticClass()` override; production behaviour unchanged. Decision: (a) document as known commandlet-only limitation, or (b) investigate ALS LoadObject timing fix. | `docs/HANDOFF_v0.5.1.md` § 4 + `Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp:L138-146` |

---

## 8. Gate command cheat-sheet

```powershell
# Build UE editor (run after any Source/ArchSim/ or Plugins/FrameSolver/Source/FrameCoreUE/ change)
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# 6-leg gate (default 149 leg-2 + 1 leg-6 PIE = 150; pass -ExpectedUeTests 147 on non-cuDSS host)
# Legs 1-5 headless; leg 6 is render-thread PIE auto-smoke (since v0.5.1)
.\Scripts\run_gate.ps1 -RequireOpenSees

# Single UE headless test (replace path) — legs 2-5 pattern
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests <test.path>; Quit" `
    -unattended -nullrhi -log

# Single PIE auto-smoke run (leg 6 only; NO -nullrhi — render thread needed)
.\Scripts\run_pie_gate.ps1 -Root . -Engine $env:UE_ENGINE_ROOT -UProject .\ArchSim.uproject
# OR raw commandlet:
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.PIE.PortalFrameSmoke; Quit" `
    -unattended -log

# Optional extra leg: CUDA / cuDSS gate
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
