using UnrealBuildTool;

// FrameCoreUE — UE-side reflection / Blueprint exposure layer for the FrameCore native
// engine. Pure consumer-side: zero engine source under Plugins/FrameSolver/Source/FrameCore/
// is touched by anything in this module. Hosts USTRUCT mirrors of frame::StressField POD
// types and a UBlueprintFunctionLibrary that wraps FRAMECORE_API computeStressField.
//
// Editor-only Slate panel additions land in #if WITH_EDITOR regions in this same module
// (Phase 3 of PLAN v3.2 — adds Slate / SlateCore / UnrealEd / ToolMenus deps under
// bBuildEditor) rather than a separate FrameCoreUEEditor module, keeping .uplugin schema
// at 2 entries instead of 3.
public class FrameCoreUE : ModuleRules
{
    public FrameCoreUE(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage    = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        // bUseUnity=false because Private/Tests/*.cpp use anonymous namespaces for fixture
        // helpers; on a clean (cold) build with all .cpp out of UBT's adaptive working set,
        // UE would merge them into one unity TU and the anon-namespace helpers would
        // shadow each other. Adaptive build only protects in-flight edits — committed
        // files re-enter unity on the next clean build. CLAUDE.md 踩雷 #4.
        bUseUnity   = false;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "FrameCore",
        });

        // Phase 3 (Slate panel + nomad tab spawner registered under Tools workspace menu).
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[] {
                "Slate",
                "SlateCore",
                "UnrealEd",
                "EditorStyle",
                "EditorSubsystem",
                "ToolMenus",
                "InputCore",
                "WorkspaceMenuStructure",
            });
        }
    }
}
