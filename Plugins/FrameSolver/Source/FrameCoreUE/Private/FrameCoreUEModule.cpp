#include "FrameCoreUE/FrameCoreUEModule.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "FrameCoreUE/SFrameCoreStressFieldPanel.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#endif

static const FName GFrameCoreStressFieldTabName(TEXT("FrameCoreStressFieldPanel"));

void FFrameCoreUEModule::StartupModule()
{
#if WITH_EDITOR
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        GFrameCoreStressFieldTabName,
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& /*Args*/)
        {
            return SNew(SDockTab)
                .TabRole(ETabRole::NomadTab)
                [
                    SNew(SFrameCoreStressFieldPanel)
                ];
        }))
        .SetDisplayName(FText::FromString(TEXT("FrameCore Stress Field")))
        .SetTooltipText(FText::FromString(TEXT("FrameCore S11 stress-field visualisation panel (dev preview).")))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
#endif
}

void FFrameCoreUEModule::ShutdownModule()
{
#if WITH_EDITOR
    if (FGlobalTabmanager::Get()->HasTabSpawner(GFrameCoreStressFieldTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GFrameCoreStressFieldTabName);
    }
#endif
}

IMPLEMENT_MODULE(FFrameCoreUEModule, FrameCoreUE)
