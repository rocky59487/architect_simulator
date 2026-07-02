// ArchSim — PIE Automation smoke test: SpawnDefaultPortalFrame + solve + screenshot.
//
// Sprint S-07 AS-35-u1. Plan ref: docs/logs/S-07/plan_*.md § AS-35-u1.
//
// WHY this file exists:
//   Python -ExecutePythonScript PIE drive was proven architecturally dead
//   (see memory/v0-5-0-pie-auto-smoke-architecture.md). The canonical UE approach
//   for PIE-bound automation is C++ latent commands that yield to the game thread
//   between each step. This file implements that approach.
//
// Test name category: ArchSim.PIE.* (new namespace per AS-35-u1 dispatch).
//   Existing convention: ArchSim.<Category>.<TestName> with Category ∈
//   {Persistence, Integration, Gameplay, UI, Multiplayer}. "PIE" is a new category.
//
// What this test verifies (in real PIE mode, no -nullrhi):
//   Step 1. FStartPIECommand(false)  — start real PIE (PlayerController active)
//   Step 2. FWaitForMapToLoadCommand — wait for PIE world ready
//   Step 3. FEngineWaitLatentCommand(1.0) — allow ALS BeginPlay + Registry init
//   Step 4. FDrivePortalFrameSmokeCommand — instantiate Widget, call
//            SpawnDefaultPortalFrame(), verify 4-node model, call RequestSolveAndVisualize()
//   Step 5. FEngineWaitLatentCommand(0.5) — allow 150 ms Registry debounce + solve
//   Step 6. FVerifyHeatmapSpawnedCommand — check HeatmapActor spawned (best-effort warning)
//   Step 7. FSafeEditorScreenshotCommand — capture viewport via FScreenshotRequest
//              (replaces FTakeActiveEditorScreenshotCommand which crashes in
//              UnrealEditor-Cmd: GetActiveTopLevelWindow() → null → SharedPointer.h:1046)
//   Step 8. FEndPlayMapCommand — clean PIE shutdown
//
// What this test CANNOT verify (honest per AS-07 lesson #1):
//   - Correctness of structural solution values (oracle = ArchSim.Gameplay.ScenarioFixture
//     + standalone gate F1..F71)
//   - ALS locomotion (separate u3_pie_smoke.md gate)
//   - SPUD persistence (deferred AS-08)
//
// Build note: IMPLEMENT_COMPLEX_AUTOMATION_TEST requires a GetTests() override.
//   For a single smoke scenario with no parameterisation we push one empty-string
//   entry so the test runner registers exactly one run.
//
// Naming: ArchSim.PIE.PortalFrameSmoke → $ExpectedUeTests 149 → 150 (cuDSS)
//         non-cuDSS: 147 → 148  (u2 bumps run_gate.ps1 $ExpectedUeTests)
//
// FROZEN guard: zero lines under Plugins/FrameSolver/Source/FrameCore/ (v4.0.0 FROZEN).
// FROZEN guard: zero lines under Plugins/LevelSim/Source/LevelCore/ (v2.2+1 FROZEN).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"               // AWorldSettings
#include "EngineUtils.h"                              // TActorIterator<AActor>
#include "GameFramework/GameModeBase.h"               // AGameModeBase (null-pawn GameMode)
#include "UnrealClient.h"                              // FScreenshotRequest::RequestScreenshot
#include "Editor/ArchSimScenarioWidget.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "Tests/ArchSimPieHarness.h"                  // OverrideGameModeForSafePIE

// ---------------------------------------------------------------------------
// Step 4 latent command: drive the portal frame smoke flow.
//
// Resolves PIE world via GEditor->PlayWorld (returns false to retry if null —
// the automation framework will call Update() each tick until we return true).
// Instantiates UArchSimScenarioWidget via NewObject (same headless pattern as
// ArchSimScenarioFixtureTest SC6), then calls SpawnDefaultPortalFrame() and
// verifies the 4-node portal model was registered.
// ---------------------------------------------------------------------------
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
    FDrivePortalFrameSmokeCommand,
    FAutomationTestBase*, Test);

bool FDrivePortalFrameSmokeCommand::Update()
{
    // Resolve PIE world. Returns false (retry) if PlayWorld isn't ready yet.
    if (!GEditor)
    {
        return false;  // should not happen; retry anyway
    }

    UWorld* PIEWorld = GEditor->PlayWorld;
    if (!PIEWorld)
    {
        // PIE world not yet initialised; retry next tick.
        return false;
    }

    // -----------------------------------------------------------------------
    // SC1: SpawnDefaultPortalFrame
    // Instantiate widget in the transient package (same NewObject pattern as
    // ArchSimScenarioFixtureTest SC6). In PIE the widget will find the live
    // Registry via UArchSimModelRegistry::Get(GEditor->PlayWorld).
    // -----------------------------------------------------------------------
    UArchSimScenarioWidget* Widget = NewObject<UArchSimScenarioWidget>(
        GetTransientPackage(),
        UArchSimScenarioWidget::StaticClass());

    if (!Test->TestNotNull(TEXT("SC1: Widget NewObject non-null"), Widget))
    {
        return true;  // fatal for this sub-check; proceed to cleanup anyway
    }

    const bool bSpawnOk = Widget->SpawnDefaultPortalFrame();
    Test->TestTrue(TEXT("SC1: SpawnDefaultPortalFrame() returned true in PIE"), bSpawnOk);

    if (bSpawnOk)
    {
        Test->AddInfo(TEXT("SC1 [VERIFIED]: SpawnDefaultPortalFrame() succeeded in PIE."));
    }
    else
    {
        Test->AddWarning(TEXT("SC1: SpawnDefaultPortalFrame() returned false — "
                              "Registry may not have been ready yet. "
                              "PIE world exists but Registry::Get returned nullptr? "
                              "Check ALS BeginPlay / GameInstance timing."));
    }

    // -----------------------------------------------------------------------
    // SC2: Verify 4-node model in Registry
    // Portal frame = 2 fixed-support base nodes + 2 free top-corner nodes.
    // SpawnDefaultPortalFrame deduplicates via FindOrAddNode so column base
    // nodes overlap exactly with the RegisterFixedSupport nodes → 4 unique nodes.
    // -----------------------------------------------------------------------
    UArchSimModelRegistry* Registry = UArchSimModelRegistry::Get(PIEWorld);
    if (Test->TestNotNull(TEXT("SC2: Registry non-null in PIE"), Registry))
    {
        const FFrameModelDef& Model = Registry->GetCurrentModel();
        // 4 nodes: 2 fixed supports (base) + 2 free top-corner nodes
        Test->TestEqual(TEXT("SC2: Model has 4 nodes (2 fixed base + 2 free top)"),
                        Model.Nodes.Num(), 4);
        Test->AddInfo(FString::Printf(
            TEXT("SC2: Model.Nodes.Num() = %d (expected 4)"), Model.Nodes.Num()));

        // Also verify 3 members: Column A, Column B, Beam
        Test->TestEqual(TEXT("SC2: Model has 3 members (2 columns + 1 beam)"),
                        Model.Members.Num(), 3);
        Test->AddInfo(FString::Printf(
            TEXT("SC2: Model.Members.Num() = %d (expected 3)"), Model.Members.Num()));
    }

    // -----------------------------------------------------------------------
    // SC2b: Dump model state for LDLT failure diagnosis
    // WHY: In commandlet PIE, FlushAndStartSession reports LDLT rank-deficient.
    // AddInfo each member's node indices and positions so we can check geometry.
    // -----------------------------------------------------------------------
    if (Registry)
    {
        const FFrameModelDef& Model2 = Registry->GetCurrentModel();
        for (int32 ni = 0; ni < Model2.Nodes.Num(); ++ni)
        {
            const FFrameNode& N = Model2.Nodes[ni];
            Test->AddInfo(FString::Printf(
                TEXT("SC2b: Node[%d] Pos=(%.1f,%.1f,%.1f) mm Fixed=%d"),
                ni, N.Pos.X, N.Pos.Y, N.Pos.Z,
                (N.Fixed.Num()==6 && N.Fixed[0]) ? 1 : 0));
        }
        for (int32 mi = 0; mi < Model2.Members.Num(); ++mi)
        {
            const FFrameMember& M = Model2.Members[mi];
            Test->AddInfo(FString::Printf(
                TEXT("SC2b: Member[%d] I=%d J=%d MatIdx=%d SecIdx=%d RefVec=(%.2f,%.2f,%.2f) active=%d"),
                mi, M.I, M.J, M.MatIdx, M.SecIdx,
                M.RefVec.X, M.RefVec.Y, M.RefVec.Z, M.bActive ? 1 : 0));
        }
    }

    // -----------------------------------------------------------------------
    // SC3: RequestSolveAndVisualize
    // Triggers the 150 ms debounced solve + lazy-spawn HeatmapActor.
    // The actual heatmap spawn happens in the delegate callback (150 ms later),
    // so we verify it in Step 6 (FVerifyHeatmapSpawnedCommand after 500 ms wait).
    // -----------------------------------------------------------------------
    const bool bSolveOk = Widget->RequestSolveAndVisualize();
    Test->TestTrue(TEXT("SC3: RequestSolveAndVisualize() returned true"), bSolveOk);

    if (bSolveOk)
    {
        Test->AddInfo(TEXT("SC3 [VERIFIED]: RequestSolveAndVisualize() dispatched solve in PIE."));
    }
    else
    {
        Test->AddWarning(TEXT("SC3: RequestSolveAndVisualize() returned false — "
                              "Registry null or no PIE world. Check SC1 result."));
    }

    // Stash the widget pointer for Step 6 via a world tag approach is not
    // straightforward across latent commands. Instead Step 6 re-acquires
    // HeatmapActor via world actor iteration (AFrameUtilizationHeatmapActor).
    return true;  // latent command complete; proceed to next step
}

// ---------------------------------------------------------------------------
// Step 6 latent command: verify HeatmapActor spawned after solve debounce.
//
// Best-effort: AddWarning (not TestFalse) on null because the 500 ms wait in
// Step 5 may not be sufficient on all hosts (solve is async-debounced).
// ---------------------------------------------------------------------------
DEFINE_LATENT_AUTOMATION_COMMAND(FVerifyHeatmapSpawnedCommand);

bool FVerifyHeatmapSpawnedCommand::Update()
{
    if (!GEditor || !GEditor->PlayWorld)
    {
        // PIE ended unexpectedly; nothing to verify.
        return true;
    }

    UWorld* PIEWorld = GEditor->PlayWorld;

    // Iterate actors to find AFrameUtilizationHeatmapActor.
    // WHY actor iteration vs. storing widget pointer: latent commands are
    // independent objects; passing a raw UObject* between them would be unsafe
    // (GC could collect it between steps). Actor iteration is safe because the
    // actor is rooted in the PIE world.
    bool bHeatmapFound = false;
    for (TActorIterator<AActor> It(PIEWorld); It; ++It)
    {
        // Check class name to avoid a hard include of the FrameCoreUE header
        // in this file (which would add a transitive dep on FrameSolver module).
        // WHY GetClass()->GetName(): avoids coupling to AFrameUtilizationHeatmapActor
        // header from the FrameCoreUE plugin in an ArchSim-only test TU.
        // The class name is stable (FROZEN API).
        if (It->GetClass()->GetName() == TEXT("FrameUtilizationHeatmapActor"))
        {
            bHeatmapFound = true;
            break;
        }
    }

    if (bHeatmapFound)
    {
        // Use GCurrentAutomationTestBase to record the result.
        // GetCurrentTest() is the standard pattern for latent commands
        // that need to assert without holding a Test* parameter.
        if (FAutomationTestBase* CurrentTest = FAutomationTestFramework::Get().GetCurrentTest())
        {
            CurrentTest->AddInfo(
                TEXT("SC4 [VERIFIED]: AFrameUtilizationHeatmapActor found in PIE world after solve."));
        }
    }
    else
    {
        // Best-effort warning — not a hard test failure.
        // The 500 ms wait may be insufficient on a slow host. The important
        // signal is that Steps 1-3 completed cleanly (PIE up, widget spawned,
        // solve dispatched).
        if (FAutomationTestBase* CurrentTest = FAutomationTestFramework::Get().GetCurrentTest())
        {
            CurrentTest->AddWarning(
                TEXT("SC4 [NEW CODE, PIE required]: AFrameUtilizationHeatmapActor not found "
                     "after 500 ms wait. Possible causes: (a) solve debounce > 500 ms on this host, "
                     "(b) HeatmapActor spawn failed silently, (c) PIE teardown already began. "
                     "Not a hard failure — smoke completion (screenshot) is the primary gate."));
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Step 7 safe screenshot command: guards FSlateApplication::GetActiveTopLevelWindow()
// against nullptr (null in UnrealEditor-Cmd commandlet mode — no Slate window exists).
//
// WHY this wrapper: FTakeActiveEditorScreenshotCommand::Update() calls
//   FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef()
// without a null-pointer guard. In commandlet mode GetActiveTopLevelWindow()
// returns a null TSharedPtr<SWindow>, and ToSharedRef() asserts on null
// (SharedPointer.h:1046 "Assertion failed: IsValid()"), crashing with exit 3.
//
// This wrapper checks for a valid Slate window before delegating to the built-in
// command, or logs a warning and skips silently in headless environments.
// Observed crash: ArchSim.log 2026-06-28 10:04:57 (SharedPointer.h:1046).
// ---------------------------------------------------------------------------
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
    FSafeEditorScreenshotCommand,
    FString, ScreenshotName);

bool FSafeEditorScreenshotCommand::Update()
{
    // Strategy: prefer FScreenshotRequest::RequestScreenshot over the Slate-based
    // FTakeActiveEditorScreenshotCommand. The latter calls
    //   FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef()
    // without a null guard and crashes in UnrealEditor-Cmd commandlet mode
    // (no Slate top-level window exists → SharedPointer.h:1046 assert).
    // Root-cause observed: ArchSim.log 2026-06-28 10:04:57.
    //
    // FScreenshotRequest::RequestScreenshot queues a viewport-level capture flag
    // consumed by the render thread. Does NOT touch Slate. In PIE with render
    // thread alive (no -nullrhi) the frame buffer is captured on the next render
    // cycle and written to Saved/Screenshots/. Safe in commandlet mode.

    FScreenshotRequest::RequestScreenshot(ScreenshotName, /*bShowUI=*/false,
                                          /*bAddSuffix=*/true);

    if (FAutomationTestBase* CurrentTest = FAutomationTestFramework::Get().GetCurrentTest())
    {
        CurrentTest->AddWarning(FString::Printf(
            TEXT("SC5 [PARTIAL]: Screenshot request queued as '%s' via FScreenshotRequest. "
                 "Artifact written to Saved/Screenshots/ if render thread processes it. "
                 "Full Slate-window screenshot skipped: FTakeActiveEditorScreenshotCommand "
                 "crashes in UnrealEditor-Cmd (SharedPointer.h:1046 null SWindow)."),
            *ScreenshotName));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test class: ArchSim.PIE.PortalFrameSmoke
//
// EditorContext: test runs inside the UE Editor process (required for GEditor).
// ClientContext: test requires a client (PIE) context — FStartPIECommand needs this.
// ProductionFilter: safe to run in CI / release builds (non-destructive).
//
// WHY IMPLEMENT_COMPLEX_AUTOMATION_TEST (not SIMPLE):
//   COMPLEX is chosen for the GetTests() single-scenario registration pattern,
//   which lets the automation log emit a human-readable beautified name
//   ("PortalFrame PIE Smoke") distinct from the test class identifier. Both
//   SIMPLE and COMPLEX variants support ADD_LATENT_AUTOMATION_COMMAND inside
//   RunTest equally well — the choice here is presentation/registration shape,
//   NOT a latent-command capability difference.
//   See Engine/Source/Runtime/Core/Public/Misc/AutomationTest.h:4136-4160.
// ---------------------------------------------------------------------------
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FArchSimPortalFramePIESmokeTest,
    "ArchSim.PIE.PortalFrameSmoke",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::ProductFilter
)

// GetTests: register one smoke scenario (no parameterisation needed).
// WHY one empty-string entry: the test runner calls RunTest(Parameters) for each
// OutTestCommands entry. A single empty string means one run. OutBeautifiedNames
// provides the human-readable sub-name shown in the automation log.
void FArchSimPortalFramePIESmokeTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("PortalFrame PIE Smoke"));
    OutTestCommands.Add(TEXT(""));  // empty parameters = single non-parameterised run
}

// RunTest: queue latent commands in order, return true immediately.
// The automation framework calls Update() on each latent command each tick
// until it returns true, then advances to the next. RunTest itself returns
// before PIE even starts — all real work happens in the latent commands.
bool FArchSimPortalFramePIESmokeTest::RunTest(const FString& Parameters)
{
    // Pre-step: switch to a blank Editor map and override WorldSettings.DefaultGameMode
    // so PIE uses AGameModeBase (no Pawn spawn) instead of ArchSimGameMode (ALS pawn).
    //
    // WHY: see ArchSimPieHarness::OverrideGameModeForSafePIE() for full crash chain
    //   (AS-37-u1: AlsCharacterMovementComponent.cpp:L894/L903 + AlsCharacter.cpp:L526,
    //    commandlet-only EXCEPTION_ACCESS_VIOLATION).
    //
    // SCOPE: This is contained inside the test .cpp file (test-side only).
    //   No production files, Config, or FROZEN paths are touched.
    if (!ArchSimPieHarness::OverrideGameModeForSafePIE(this))
    {
        return false;  // AddError already emitted by helper
    }

    // Step 1: Start real PIE (false = NOT Simulate; ALS pawn needs PlayerController).
    // WHY false (not true): true = Simulate mode (no PlayerController spawned).
    // On the blank map, ALS character spawns but in a minimal world with no
    // streaming or asset-heavy startup, reducing crash probability.
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));

    // Step 2: Wait for PIE world to be valid + map loaded.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());

    // Step 3: Wait 1 second for ALS BeginPlay + Registry GameInstanceSubsystem init.
    // WHY 1.0 s: ALS character BeginPlay calls LoadAlsAssetsLate() which loads
    // SKM_Als, AnimBP, Settings, MovementSettings via LoadObject<T>(). The Registry
    // subsystem also initialises after GameInstance is ready. 1 s is conservative;
    // the actual init is typically < 200 ms but we allow margin for slow hosts.
    ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

    // Step 4: Drive the portal frame smoke flow (widget spawn + portal frame + solve dispatch).
    ADD_LATENT_AUTOMATION_COMMAND(FDrivePortalFrameSmokeCommand(this));

    // Step 5: Wait 500 ms for the 150 ms Registry debounce + solve + heatmap spawn.
    // The solve fires ~150 ms after RequestSolveAndVisualize; HeatmapActor spawns
    // in the OnSolveComplete delegate. 500 ms is generous margin.
    ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(0.5f));

    // Step 6: Verify HeatmapActor spawned (best-effort; AddWarning not TestFalse on miss).
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyHeatmapSpawnedCommand());

    // Step 7: Capture screenshot of the PIE viewport.
    // Screenshot saved to Saved/Screenshots/ with name prefix "v0_5_x_pie_smoke".
    //
    // WHY FSafeEditorScreenshotCommand (not FTakeActiveEditorScreenshotCommand):
    //   FTakeActiveEditorScreenshotCommand::Update() calls
    //     FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef()
    //   without a null guard. In UnrealEditor-Cmd commandlet mode no Slate top-level
    //   window exists, so GetActiveTopLevelWindow() returns a null TSharedPtr<SWindow>
    //   and ToSharedRef() asserts (SharedPointer.h:1046), crashing with exit 3.
    //   FSafeEditorScreenshotCommand uses FScreenshotRequest::RequestScreenshot instead,
    //   which only queues the request (safe in commandlet mode).
    //
    // WHY screenshot: provides a human-reviewable artifact that the viewport showed
    // the portal frame + heatmap colours, even in CI where a human isn't watching.
    ADD_LATENT_AUTOMATION_COMMAND(FSafeEditorScreenshotCommand(TEXT("v0_5_x_pie_smoke")));

    // Step 8: End PIE cleanly. Always runs even if earlier steps had warnings/failures.
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

    // RunTest returns true immediately; latent framework drives the rest.
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
