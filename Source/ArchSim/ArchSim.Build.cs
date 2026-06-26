// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArchSim : ModuleRules
{
	public ArchSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// EnhancedInput: pre-declared for Sprint A2 (ALS character + Enhanced Input MappingContext); not consumed by v0.1 sources yet.
		// FrameCoreUE: structural engine consumer-side API surface (UFrameInteractiveSubsystem + USTRUCT model/result/patch types).
		// ALS: Advanced Locomotion System — AArchSimCharacter.h #includes AlsCharacter.h (public base class).
		//      Public dep required because any TU that includes ArchSimCharacter.h must also resolve ALS headers.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "FrameCoreUE",
			"ALS"  // AS-03a: Advanced Locomotion System (AAlsCharacter base class)
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
