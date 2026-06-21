// Phase 6e U-04 closeout — sanity test for the FFrameCoreUEModule::StartupModule nomad
// tab spawner registration. Closes the v3.2.0 deferred item U-04: "if
// WorkspaceMenu::GetMenuStructure() ever moves API, the panel registration may fail
// silently". This test asserts the spawner is present after the editor build's module
// startup, so a future UE engine version that breaks the WorkspaceMenuStructure
// resolution path will surface as a failed automation run rather than a quiet missing
// editor menu entry.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Framework/Docking/TabManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEEditorTabSpawnerTest,
    "FrameCore.UE.EditorTabSpawnerTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEEditorTabSpawnerTest::RunTest(const FString& /*Parameters*/)
{
    // The tab name string here MUST match FrameCoreUEModule.cpp's
    // `GFrameCoreStressFieldTabName` literal. If the constant ever moves, both must move
    // together; this test catches the divergence.
    static const FName kTabName(TEXT("FrameCoreStressFieldPanel"));

    const bool bSpawnerRegistered =
        FGlobalTabmanager::Get()->HasTabSpawner(kTabName);

    TestTrue(TEXT("FrameCoreUE nomad tab spawner is registered after StartupModule"),
             bSpawnerRegistered);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
#endif // WITH_EDITOR
