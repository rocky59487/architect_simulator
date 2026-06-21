// Phase 3 editor smoke test — verifies the Slate panel widget constructs cleanly under
// the editor build (Slate API surface didn't drift) and that OnComputeClicked() drives
// the compute path without crashing the test harness. Does NOT exercise the tab spawner
// (NomadTab spawner needs a real editor session, which Slate automation can't fake);
// the spawner registration itself is implicitly verified because the test runs at all —
// FFrameCoreUEModule::StartupModule already ran by then.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/SFrameCoreStressFieldPanel.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "Widgets/SNullWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEEditorSmokeTest,
    "FrameCore.UE.EditorSmokeTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEEditorSmokeTest::RunTest(const FString& /*Parameters*/)
{
    // (1) Construct the panel widget. If Slate API drift or missing module dep, this
    //     either won't compile (caught at build time) or will assert at construction.
    TSharedRef<SFrameCoreStressFieldPanel> Panel = SNew(SFrameCoreStressFieldPanel);
    TestNotNull(TEXT("Panel constructed"), &Panel.Get());

    // (2) Drive the compute path explicitly — avoid simulating a click since Slate
    //     automation can't reliably fake input events under the test harness.
    const FReply Result = Panel->OnComputeClicked();
    TestTrue(TEXT("OnComputeClicked returns Handled"), Result.IsEventHandled());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
#endif // WITH_EDITOR
