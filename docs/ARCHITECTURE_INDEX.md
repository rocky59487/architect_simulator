# Architecture Index — Architect Simulator

> **Purpose:** Cheap-to-consult registry of what exists in this repo. Read
> the relevant section before writing new code/docs/tests to avoid duplicating
> existing surface. **Not** a tutorial — see linked docs for depth.
>
> **Owner:** session-driver skill (manager thread). Subagents read only.
> Updated at Phase 6 of every release-hardening cycle.
>
> **Latest tag:** v0.6.1 (**S-09 close** — audit hardening + sidecar v2 + PIE gate 收緊 + reproducibility 閉環: AS-40 persistence edge-case guards (4 headless tests) + AS-41 sidecar format v2 `SidecarFormatVersion=2` with loads/UDLs/releases/tension-only/shells/library defs/per-DOF fixity + v1 archive compat (8 headless tests) + AS-42 `run_pie_gate.ps1` per-test `$ExpectedPieTests` guard + screenshot freshness + 180-tick PortalFrame hard assert + AS-39 third-party reproducibility manifest (`docs/THIRD_PARTY.md` pinned SHA + `Scripts/setup_third_party.ps1` + `run_gate.ps1` precondition check). ExpectedUeTests 165 (cuDSS) / 163 (non-cuDSS). ArchSim headless family 29; PIE category 2 (unchanged)) — see [`docs/RELEASE_v0.6.1.md`](RELEASE_v0.6.1.md) + [`docs/HANDOFF_v0.6.1.md`](HANDOFF_v0.6.1.md) + [`docs/logs/S-09/manager.md`](logs/S-09/manager.md) **§ SESSION CLOSE**(retrospective + durable lessons).
> **Prior tags:** v0.6.0 (S-08 CLOSED — persistence chain complete: PIE.SaveLoadSmoke + leg 6 category-wide; ExpectedUeTests 153) — see [`docs/RELEASE_v0.6.0.md`](RELEASE_v0.6.0.md) + [`docs/HANDOFF_v0.6.0.md`](HANDOFF_v0.6.0.md) + [`docs/logs/S-08/manager.md`](logs/S-08/manager.md) / v0.5.4 (S-08 — SPUD sidecar wiring + RF_Transient audit closed: SPUD gate is `ISpudObject` opt-in, NOT RF_Transient; `UArchSimPersistenceSubsystem` + `Registry::Reset()` + 4 headless tests) / v0.5.3 (S-08 — PIE test infra closeout: three v0.5.1 NITs + AS-37 closed commandlet-only with `ArchSimPieHarness::OverrideGameModeForSafePIE()` — **all commandlet PIE tests MUST call it**) / v0.5.2 (S-08 — PlaceKSetMember node-pair degeneration fix: bare `AActor` no RootComponent → SpawnActor location dropped → two K1 columns shared one node pair → LDLT singular; fix = `USceneComponent` root graft + SC8/SC9; **user-driven PIE P10/P11 human re-verify pending**) / v0.5.1 (S-07 close — AS-35 PIE auto-smoke 6-leg gate: `ArchSim.PIE.PortalFrameSmoke` + `Scripts/run_pie_gate.ps1`; USER-DRIVEN P1..P15 still canonical for「ready for student trial」per `pie-auto-smoke-preference` memory) / v0.5.0 (Sprint S-06 close — AS-30 + U-ALS + U-IWYU + U-LOW) / v0.4.0.1 (S-05 patch — AS-28 Scenario widget cross-world hotfix + IWYU first-header rebuild fix) / v0.4.0 (Sprint S-05 close — Scenario MVP minor bump; marked prerelease because PIE end-to-end flow broken by cross-world bug pre-v0.4.0.1) / v0.3.1 (Sprint S-04 patch close — S-03 carryover AS-20 + AS-24 + 7 cosmetic NITs + outside-repo hook fix) / v0.3.0 (Sprint S-03 close — hardening + PIE-world test harness foundation) / v0.2.0 (Sprint S-02 close — ALS pawn end-to-end) / v0.1.5 / v0.1.4 (patch bundles)

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
| `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` | `UArchSimModelRegistry` | UGameInstanceSubsystem holding FFrameModelDef; `RegisterMember`/`DeactivateMember`/`SetCurrentDemand`/`RequestSolve`; 150 ms debounce; `MaxRankBeforeRebaseline=96` bounds PendingRankAccumulation in RequestSolve (NOT register count — see §7 backlog AS-07 closure); 3 telemetry getters added v0.1.4 AS-10. **AS-30 (v0.5.0):** `RegisterFixedSupport(FVector PosMm) -> int32` — register a fully-fixed support node (Fixed=[T,T,T,T,T,T]); internally calls `FindOrAddNode` for 1 mm dedup + sets `FFrameNode.Fixed=all-true`; idempotent (repeated calls at same position re-confirm Fixed without duplicating node). **Does NOT auto-trigger Solve** — caller is expected to batch supports + members and rely on RegisterMember's 150 ms debounce timer for the eventual Solve kick (matches the original RequestSolve/RegisterMember separation pattern; see Registry header L68-69). **AS-40/41 (v0.6.1):** additive v2 API — `RestoreLibraries(Materials, Sections)` / `ApplyFixityAt(NodeIdx, FixityArray)` / `InjectNodalLoad` / `InjectMemberUDL` / `FindOrAddNodePublic(Pos, Tol)` / `SetMemberFlags(Idx, bActive, bTensionOnly, Releases)`; `Reset()` now also clears component `CachedUtilization` flags (AS-40 fix); `RegisterFixedSupport` invalidates session to force rebaseline (AS-40 fix); `RegisterMember` rejects non-finite endpoint positions (AS-40 guard). | v0.1 (A1-02..A1-05) |
| `Source/ArchSim/Public/ArchSimGameInstance.h` | `UArchSimGameInstance` | UGameInstance + `FTickableGameObject`. Tick body detects `Registry->GetRegisteredCount()` delta since last frame and emits `RequestSolve(FFrameModelPatch{})` to bridge BeginPlay-time Member registrations into the solver (RegisterMember does NOT auto-trigger solve). 4 BP-pure getters: `GetTickCount` / `GetAccumulatedTime` / `GetLastSeenRegisteredCount` / `GetSolveTriggerCount`. `IsTickable()` three-condition AND. **Wired as `GameInstanceClass=/Script/ArchSim.ArchSimGameInstance` in `Config/DefaultEngine.ini`**. | v0.1.4 (AS-02a) + v0.1.5 (AS-02b) |
| `Source/ArchSim/Public/Characters/ArchSimCharacter.h` | `AArchSimCharacter` | `AAlsCharacter` subclass — production-grade third-person locomotion via the ALS-Refactored v4.17 state machine. Adds 5 Enhanced Input `TObjectPtr<UInputAction>` slots + 1 `TObjectPtr<UInputMappingContext>` (default null in CDO, BP assigns from `Content/Input/*` UAssets per `docs/INPUT_MAPPING.md`). 7 handler methods (HandleMove view-space / HandleLook / HandleJumpPressed+Released / HandleSprintPressed+Released / HandleCrouchToggle) wire to ALS state setters. 1 `TObjectPtr<UAlsCameraComponent>` Camera default subobject attached to mesh (Yaw=90 via `SetRelativeRotation_Direct`). `bUseControllerRotation*` all false. **S-06 U-ALS:** added `PostInitProperties()` + `BeginPlay()` overrides + `LoadAlsAssetsLate()` helper that wires Settings / MovementSettings / SkeletalMesh / AnimBlueprint via runtime-late `LoadObject<T>()` (CDO ConstructorHelpers fail because ALS plugin content not mounted at CDO time per `Saved/Logs/ArchSim-backup-2026.06.28-03.37.00.log:L916-929`). BeginPlay fallback calls helper BEFORE `Super::BeginPlay()` because ALS L138-146 fires `ALS_ENSURE(IsValid(Settings))` inside Super. Companion ALS plugin patch in `Tools/patches/als_l400_animinstance_guard.patch` adds null-guard to `AAlsCharacter::RefreshMeshProperties` L400 (untracked-third-party convention — fresh checkouts apply via `git apply`). | v0.2.0 (AS-03a/b/c) + v0.5.0 (S-06 U-ALS) |
| `Source/ArchSim/Public/ArchSimGameMode.h` | `AArchSimGameMode` | `AGameModeBase` subclass. `DefaultPawnClass = AArchSimCharacter::StaticClass()` so PIE spawns the ALS pawn. **Wired as `GlobalDefaultGameMode=/Script/ArchSim.ArchSimGameMode` in `Config/DefaultEngine.ini`**. | v0.2.0 (AS-03c) |
| `Source/ArchSim/Public/Editor/ArchSimScenarioWidget.h` | `UArchSimScenarioWidget` | `UEditorUtilityWidget` subclass, `UCLASS(Abstract)` — Editor-side Scenario MVP widget gated by `#if WITH_EDITOR` (whole class compiles out for packaged shipping builds). BP-callable surface: `PlaceK1Column` / `PlaceK2Beam` / `PlaceK4Brace` (K-set placement via private `PlaceKSetMember` shared helper) + `RequestSolveAndVisualize` (Registry debounced solve + lazy-spawn `AFrameUtilizationHeatmapActor` in PIE world) + `AdvanceTutorialStep` (linear 6-state `EArchSimTutorialState` machine, FreeExplore terminal) + `GetCurrentPromptText` (LOCTEXT namespace `ArchSimTutorial`) + `ResetWidgetState` (unsubscribe delegate + destroy HeatmapActor + clear PlacedActors list + reset state). Two `BlueprintImplementableEvent` stubs for BP overlay UI: `OnTutorialStateChanged(NewState, PromptText)` + `OnVoicePromptShouldPlay(PromptText)` — text-only, no third-party TTS SDK linked in C++. `WITH_EDITORONLY_DATA` UPROPERTYs: `HeatmapActor` (u2) / `TutorialState` (u3) / `PlacedActors` (u3). Explicit `Registry->OnSolveComplete.Remove(Handle)` in `BeginDestroy` via cached `TWeakObjectPtr<UArchSimModelRegistry> SubscribedRegistry`. Production class instantiation is via a Blueprint child class (`UCLASS(Abstract)`). **AS-30 (v0.5.0, WITH_EDITOR guarded):** `PlaceFixedSupport(FVector LocationWorld) -> int32` (PIE-world preferred, world→mm ×10, delegates to `Registry::RegisterFixedSupport`) + `SpawnDefaultPortalFrame() -> bool` (2 fixed supports + 2 K1 columns + 1 K2 beam at 2x2x2 m portal; graceful-fail returns false when no PIE Registry). | v0.4.0 (SPIKE-Scenario-u1/u2/u3) |

| `Source/ArchSim/Public/Subsystems/ArchSimPersistenceSubsystem.h` | `UArchSimPersistenceSubsystem` | UGameInstanceSubsystem save-slot orchestrator — SPUD global-object sidecar (`AddPersistentGlobalObjectWithName`). **v2 format (SidecarFormatVersion=2, v0.6.1 AS-41):** sidecar covers member WorldTransform / EndI-J offsets / Group / MatIdx / SecIdx / active flag / bTensionOnly / Release[12]; nodal loads / member UDLs / shells (position-keyed / RecordIdx mapped); per-DOF fixity `NodeFixities[]` (SupportPositions leaves empty for v1-compat); materials + sections library definitions. `SaveToSlot` has `bAllowEmptyOverwrite=false` guard (AS-40 fix). `LoadFromSlot` → `OnPostLoadGame` → `ReplayLoadedSidecar` (Registry `Reset()` → `RestoreLibraries` → supports → respawn + root graft → re-register → debounced solve); pre-check via `GetSaveGameInfo` before load (AS-40 fix); orphan `DestroyActor` on replay (AS-40 fix). MemberIdx reassigned on load; `CachedUtilization` restore value = display fallback until post-load solve. **v1 archive compat:** missing v2 fields walk SPUD skip-and-default (`SpudState.cpp:1077`) → v1 semantic replay. **Honest boundary:** `LoadFromSlot` full chain (OpenLevel latent sequence) is not reachable under PIE commandlet automation (`SpudSubsystem.cpp:977`); `ReplayLoadedSidecar` direct path is the automation-verified equivalent; human play-session verify is canonical for end-to-end OpenLevel chain. | AS-08 fully closed v0.5.4+v0.6.0; v2 format AS-41 v0.6.1 |

### Planned classes (with backlog ID; see §7 for status)

*(No remaining planned classes — all landed. See §7 backlog for open items.)*

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
| `ArchSimScenarioFixtureTest.cpp` | `FArchSimScenarioFixtureTest` | `ArchSim.Gameplay.ScenarioFixture` | 9 sub-checks (SC1–SC9): SC1 `PlaceFixedSupport` UFunction reflection (BlueprintCallable + int32 return + 1 FVector param); SC2 `SpawnDefaultPortalFrame` UFunction reflection (BlueprintCallable + bool return + 0 params); SC3 Registry headless `RegisterFixedSupport(0,0,0)` → NodeIdx>=0 + `Fixed.Num()==6` + all-true; SC4 node-snap dedupe — second call at same pos returns same idx (FindOrAddNode 1 mm tolerance verified); SC5 idempotent Fixed — second call leaves Fixed length-6 all-true; SC6 transient widget graceful-fail — `SpawnDefaultPortalFrame()` no-crash when no PIE Registry; SC7 AddInfo PIE oracle link for P10/P11 smoke (`docs/logs/S-05/u3_pie_smoke.md` AS-30 update). PIE fixture (5 actors + heatmap) deferred to USER-DRIVEN smoke P10/P11 (AS-30, v0.5.0, S-06); **SC8** headless two-members-at-distinct-locations → distinct node pairs + node count==4; **SC9** actor-location round-trip after `USceneComponent` root graft (AS-36, v0.5.2, S-08) |

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

> **Reproducibility:** the four third-party plugins (ALS / SPUD / SUQS / Prefabricator) are not bundled in the repo. Pinned SHAs + patch instructions are in [`docs/THIRD_PARTY.md`](THIRD_PARTY.md). Run `Scripts\setup_third_party.ps1` on a fresh clone to install and patch them; `run_gate.ps1` checks for their presence and fails fast if missing (AS-39, v0.6.1).

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

`IMPLEMENT_SIMPLE_AUTOMATION_TEST` count (as of v0.6.1):

| Namespace | Count | Source |
|---|---|---|
| `FrameCore.*` (UE automation, headless) | 60 | `Plugins/FrameSolver/Source/FrameCore/Private/Tests/` |
| `FrameCore.UE.*` (UE automation) | 77 | `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/` |
| `ArchSim.*` (game body, headless legs 1-5) | 29 | `Source/ArchSim/Private/Tests/` |
| `ArchSim.PIE.*` (game body, **render-thread leg 6 only**) | 2 | `ArchSimPortalFramePIESmokeTest.cpp` + `ArchSimSaveLoadPIESmokeTest.cpp` |
| **leg-2 gate total** | **165** (cuDSS) / **163** (non-cuDSS; 2 CUDA-gated tests compile out via `#if FRAMECORE_CUDA`) | run via `Scripts/run_gate.ps1 -RequireOpenSees`; PIE leg 6 runs separately via `Scripts/run_pie_gate.ps1` |

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
- v0.5.0: `ArchSim.Gameplay.ScenarioFixture` (AS-30, S-06; `PlaceFixedSupport` + `SpawnDefaultPortalFrame` UFunction reflection SC1/SC2; Registry headless `RegisterFixedSupport` SC3; node-snap dedupe SC4; idempotent Fixed SC5; transient widget graceful-fail SC6; PIE oracle AddInfo SC7; **SC8 distinct-node-pairs + SC9 actor-location round-trip added v0.5.2 (AS-36, S-08)**; 9 sub-checks; portal frame commandlet-PIE solve+heatmap [VERIFIED] since v0.5.2 leg 6; user-driven P10/P11 human re-verify pending)
- v0.5.1: `ArchSim.PIE.PortalFrameSmoke` (AS-35, S-07; first member of NEW `ArchSim.PIE.*` Category — render-thread leg-6-only; `IMPLEMENT_COMPLEX_AUTOMATION_TEST` + 8-stage LatentCommand chain: `FStartPIECommand(false)` real PIE → `FWaitForMapToLoadCommand` → `FEngineWaitLatentCommand(1.0f)` → custom `FDrivePortalFrameSmokeCommand` (widget instantiation + `SpawnDefaultPortalFrame` + Registry node/member count verify + `RequestSolveAndVisualize`) → `FEngineWaitLatentCommand(0.5f)` → custom `FVerifyHeatmapSpawnedCommand` (best-effort `AddWarning` on miss per AS-36 Bug C tolerance) → custom `FSafeEditorScreenshotCommand` (Slate-free alternative to canonical `FTakeActiveEditorScreenshotCommand` which asserts in commandlet mode) → `FEndPlayMapCommand`. Pre-step sidestep extracted to `ArchSimPieHarness::OverrideGameModeForSafePIE()` in v0.5.3 (AS-37-u2) — sidesteps AS-37 ALS commandlet crash; production unchanged; all future commandlet PIE tests MUST call it (see §7 AS-37 row). Run via `Scripts/run_pie_gate.ps1` (NEW PowerShell wrapper, locale-defensive parser using ASCII-only `TEST COMPLETE. EXIT CODE: 0` as primary PASS signal, no `Tee-Object` capture to avoid NativeCommandError pollution). Screenshot artifact `Saved/Screenshots/WindowsEditor/v0_5_x_pie_smoke*.png`.)
- v0.5.4: `ArchSim.Persistence.SpudEmptyModelSave` / `SpudRfTransientAudit` / `SpudSidecarClearSemantics` / `SpudSidecarRoundtrip` (AS-08-u1, S-08; 4 headless tests, SC1-SC14 — Registry `Reset()` clear semantics; RF_Transient audit with `SpudPropertyUtil::IsPersistentObject` as live oracle (conclusion: SPUD gate is `ISpudObject` opt-in, not RF_Transient); sidecar field roundtrip = value-copy + reflection `CPF_SaveGame` flag oracle (honestly marked NOT a binary roundtrip — that is PIE-only, AS-08-u2); empty-model save boundary. Leg-2 count 149 → **153** (non-cuDSS 151).)
- v0.6.0: `ArchSim.PIE.SaveLoadSmoke` (AS-08-u2, S-08; second member of `ArchSim.PIE.*` — leg-6-only, NOT in leg-2 count; 13-step LatentCommand chain per PortalFrameSmoke template + mandatory `OverrideGameModeForSafePIE()` pre-step; 30 hard asserts: `.sav` exists+size (PIE-1), replay rebuild node/member counts (PIE-2 partial), member transform 1mm (PIE-5), support node 1mm (PIE-6); CachedUtilization soft check (PIE-7 AddWarning by design); teardown deletes `__PieSmoke__` slot. Leg 6 now runs the whole `ArchSim.PIE` category — `run_pie_gate.ps1` gained pre-run live-log cleanup + result-log selection.)
- v0.6.1: S-09 additions (+12 headless leg-2 tests total) — **AS-40** `ArchSim.Persistence.ResetClearsComponentFlags` / `RegisterMemberNonFinite` / `ReplayOrphanGuard` / `SaveLoadGuards` (4 tests; persistence edge-case contracts: Reset clears flags, non-finite endpoint guard, orphan destroy-actor invariant, SaveToSlot empty/partial guards + LoadFromSlot pre-check); **AS-41** `ArchSim.Persistence.V2LibraryStructRoundtrip` / `V2RestoreLibraries` / `V2InjectLoads` / `V2FixityApi` / `V2FormatVersion` / `V2DeactivatedSaveGuard` / `V2V1CompatDefaults` / `N00TensionReleaseWire` (8 tests SC19–26; sidecar v2 format roundtrip: library defs / loads-UDLs injection / per-DOF fixity API / format-version stamp / deactivated-member guard / v1-compat skip-and-default; SC17 `ReplayOrphanGuard` renamed `ReplayOrphanDataInvariant`); **AS-42** PIE leg-6 収緊 (no new leg-2 tests; `run_pie_gate.ps1` per-test `$ExpectedPieTests` guard + screenshot freshness check + PortalFrame 180-tick hard assert + `SaveLoadSmoke` tracked-set oracle `SC_D` + empty-overwrite `SC_E1` direct PIE assert [VERIFIED]; `SC_E2` partial-snapshot / `SC_E3` orphan PIE direct [NEW CODE, fault-injection unreachable, DEFERRED]). See `docs/logs/S-09/` for full agent logs.

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
| AS-08 | SPUD orchestration `RF_Transient` audit + save/load wiring | ✅ **fully closed S-08 (u1 v0.5.4 + u2 v0.6.0)** — RF_Transient audit: SPUD gate = `ISpudObject` opt-in (`SpudPropertyUtil.cpp:1342`), NOT RF_Transient (`SpudState.cpp:1133`). Sidecar wiring (`UArchSimPersistenceSubsystem` + `Registry::Reset()` + 4 headless tests) + PIE end-to-end smoke (`ArchSim.PIE.SaveLoadSmoke`; PIE-1/3/5/6 VERIFIED, PIE-2/7 PARTIAL by design, PIE-4/8/9 DEFERRED — LoadFromSlot OpenLevel full chain is the honest automation boundary, `SpudSubsystem.cpp:977`). | `docs/RELEASE_v0.6.0.md` + `docs/logs/S-08/agent_AS-08-u1.md` + `agent_AS-08-u2.md` |
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
| AS-36 | `PlaceKSetMember` two-K1-column-same-node-pair bug (Bug C from AS-35-u1 commandlet PIE) | ✅ closed S-08 (v0.5.2) — root cause NOT dedup/offset math: bare `AActor` has no RootComponent → SpawnActor location silently dropped → `GetActorTransform()` Identity in `RegisterMember` (`ArchSimModelRegistry.cpp:197-199`) → endpoints degenerate to raw offsets → duplicate node pair → floating substructure → LDLT singular. Fix = `USceneComponent` root graft in `PlaceKSetMember` (S-01 v0.1.1 test-side lesson repeated in production). SC8/SC9 regression sub-checks; commandlet PIE SC2b distinct pairs + SC4 heatmap [VERIFIED]. Production bug (not commandlet-only) — user-driven P10/P11 human re-verify pending. | `docs/RELEASE_v0.5.2.md` + `docs/logs/S-08/agent_AS-36-u1.md` |
| AS-38 | `PlaceKSetMember` shipping-safe Root guard + SC8/SC9 strengthening | ✅ closed v0.6.1 (S-09, AS-38) — (a) `check(Root)` replaced with if-guard + UE_LOG + destroy-and-return in `PlaceKSetMember`; (b) SC8 comment updated to state node-count==4 as the strict guarantee; (c) SC9 now calls `PlaceKSetMember` production path directly instead of pinning UE `SetActorLocation` round-trip. | `docs/logs/S-09/` |
| AS-37 | ALS commandlet PIE crash audit | ✅ closed S-08 (v0.5.3) — **Severity: commandlet-only** (cooked/pak packaged not affected; Dev `-game` without pak — unverified caveat). **Crash chain (precise):** `ArchSimGameMode` spawns `AArchSimCharacter` (ALS subclass) on PIE start; in `UnrealEditor-Cmd` commandlet mode `AssetRegistry` scan for `/ALS/` content is not complete at pawn-spawn timing → all 4 `LoadObject<T>()` calls in `AArchSimCharacter::LoadAlsAssetsLate()` fail → `MovementSettings` null → ALS fires without null-guard: **`AlsCharacterMovementComponent.cpp:L894` (`SetMovementSettings`, FIRST crash point), `L903` (`RefreshGaitSettings`)** → **`AlsCharacter.cpp:L526` (`NotifyLocomotionModeChanged`)** → `EXCEPTION_ACCESS_VIOLATION (reading 0xd8)`. Crash log: `Saved/Crashes/UECC-Windows-7ECCED384D44B2CCD56A45B7F390734D_0002`. **Disposal: (a)+(b-1).** (a) Documented here and in `ArchSimPieHarness.h` / `ArchSimPieHarness.cpp`. (b-1) Reusable helper `ArchSimPieHarness::OverrideGameModeForSafePIE(FAutomationTestBase*)` added to `Source/ArchSim/Private/Tests/ArchSimPieHarness.h/.cpp`; `ArchSimPortalFramePIESmokeTest.cpp` inline sidestep replaced with a single call to the helper (behaviour-identical). **Future PIE test rule:** all commandlet PIE tests (including AS-08-u2 SPUD smoke) MUST call `OverrideGameModeForSafePIE()` in their `RunTest()` pre-step, unless the test goal is specifically ALS character behaviour. | `Source/ArchSim/Private/Tests/ArchSimPieHarness.h` + `ArchSimPieHarness.cpp` + `ArchSimPortalFramePIESmokeTest.cpp` |
| AS-39 | Third-party plugin reproducibility closure | ✅ closed v0.6.1 (S-09) — `docs/THIRD_PARTY.md` pinned-SHA manifest for ALS/SPUD/SUQS/Prefabricator; `Tools/patches/` 4 auto-applicable patches; `Scripts/setup_third_party.ps1` guided setup; `run_gate.ps1` precondition check (plugin dirs + patch fingerprints, fails fast with instructions). | `docs/logs/S-09/` |
| AS-40 | Persistence / Registry v0.6.0 bug fixes | ✅ closed v0.6.1 (S-09) — SaveToSlot `bAllowEmptyOverwrite=false` guard; LoadFromSlot `GetSaveGameInfo` pre-check; replay orphan `DestroyActor`; `Registry::Reset()` clears component `CachedUtilization` flags; `RegisterFixedSupport` session invalidation; `RegisterMember` non-finite endpoint guard; AS-38 `PlaceKSetMember` shipping guard; +4 headless tests (SC15-18). | `docs/logs/S-09/` |
| AS-41 | Sidecar format v2 | ✅ closed v0.6.1 (S-09) — `SidecarFormatVersion=2`; materials/sections library defs; member active/bTensionOnly/Release[12]; nodal loads/UDLs/shells (position-keyed/RecordIdx); per-DOF fixity `NodeFixities[]` (SupportPositions留空為 v1-compat); v1 archive: missing fields walk SPUD skip-and-default → v1 semantic replay. Honest exclusion: RefVec (geometry-recomputed, manual RefVec not preserved); +8 headless tests (SC19-26; SC17 renamed `ReplayOrphanGuard`). | `docs/logs/S-09/` |
| AS-42 | Leg-6 PIE gate 收緊 | ✅ closed v0.6.1 (S-09) — `run_pie_gate.ps1` per-test `$ExpectedPieTests` array; screenshot freshness UTC check; UE5.7 Path={} parser fix; PortalFrame 180-tick hard assert; `SaveLoadSmoke` tracked-set oracle `SC_D` + empty-overwrite `SC_E1` [VERIFIED PIE]; `SC_E2`/`SC_E3` PIE fault-injection DEFERRED (unreachable path). | `docs/logs/S-09/` |
| AS-43 | Claim 誠實化 + docs drift | ✅ closed v0.6.1 (S-09) — ERRATA block appended to `docs/RELEASE_v0.6.0.md`; README drift fixed (six-leg, 165/163, third-party setup指引); ARCHITECTURE_INDEX updated (Latest tag v0.6.1, §2 class map v2, §4 THIRD_PARTY pointer, §6 counts as of v0.6.1, §7 AS-38..43 rows, §10 links). | `docs/logs/S-09/` |

---

## 8. Gate command cheat-sheet

```powershell
# Build UE editor (run after any Source/ArchSim/ or Plugins/FrameSolver/Source/FrameCoreUE/ change)
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# 6-leg gate (default 165 leg-2 + 2 leg-6 PIE; pass -ExpectedUeTests 163 on non-cuDSS host)
# Legs 1-5 headless; leg 6 is render-thread PIE auto-smoke (since v0.5.1;
# runs the whole ArchSim.PIE category — PortalFrameSmoke + SaveLoadSmoke — per-test $ExpectedPieTests since v0.6.1)
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
    -ExecCmds="Automation RunTests ArchSim.PIE; Quit" `
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
2. **Any FrameCore change must pass the 6-leg gate** — standalone F1..F71,
   UE 165 (cuDSS) / 163 (non-cuDSS) as of v0.6.1, OpenSees strict,
   linear_deep_audit 104, CLI round-trip, PIE auto-smoke (leg 6). Run before commit:
   `Scripts\run_gate.ps1 -RequireOpenSees`.
3. **Honest verify, no over-claiming** — every capability has an independent
   oracle (analytic / dense / OpenSees / sympy-numpy). `[NEW CODE]` vs
   `[VERIFIED]` honest grading. Textbook methods don't claim novelty.
4. **Index not raw pointer** — `Member`/`ShellQuad` hold `matIdx`/`secIdx`
   (indices into `FrameModel::materials/sections`), never raw pointers.
   `validate()` does range-checks.
5. **Commit hygiene** — explicit `git add` per file, **NEVER `-A` / `.`**.
   **NEVER** touch `.gitignore`, `ArchSim.uproject`, `Plugins/LevelSim/`,
   build artifacts. Run full 6-leg gate before commit.

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
- Latest release notes: [`RELEASE_v0.6.1.md`](RELEASE_v0.6.1.md)
- Latest handoff: [`HANDOFF_v0.6.1.md`](HANDOFF_v0.6.1.md)
- Sprint S-09 logs: [`logs/S-09/`](logs/S-09/)
- Engine verification map: [`VERIFICATION.md`](VERIFICATION.md)
- CLI protocol: [`CLI_PROTOCOL.md`](CLI_PROTOCOL.md)
- Karamba3D parity roadmap: [`KARAMBA3D_ROADMAP.md`](KARAMBA3D_ROADMAP.md)
- v3.x series retrospective: [`V3_SERIES_RETROSPECTIVE.md`](V3_SERIES_RETROSPECTIVE.md)
- Per-sprint logs (new, session-driver maintained): `logs/S-XX/`

---

*This file is rewritten surgically by session-driver Phase 6 after every*
*release-hardening cycle. If you find it stale, that means session-driver*
*broke convention — raise as AS-XX item.*
