using UnrealBuildTool;
using System.IO;

// LevelSimPlay — playable UE presentation layer for the LevelSim measurement core.
// The pure C++17 core (../../Core, zero UE / zero Eigen) is unity-included into this
// module (see Private/LevelCoreUnit.cpp) so no export macros / second module needed.
// All truth values & scoring flow through the core; this module is rendering + input glue.
public class LevelSimPlay : ModuleRules
{
    public LevelSimPlay(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage    = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;   // UE 5.7 baseline (core is C++17-clean, fine under C++20)

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "..", "Core"));
    }
}
