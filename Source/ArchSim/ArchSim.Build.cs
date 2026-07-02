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
			"ALS",       // AS-03a: Advanced Locomotion System (AAlsCharacter base class)
			"ALSCamera", // AS-03c: UAlsCameraComponent (third-person ALS camera; header forward-decl'd in .h, full include in .cpp)
		});

		// SPUD is Private: ArchSimPersistenceSubsystem.h exposes no SPUD types
		// (SpudSubsystem.h is included only in the .cpp), so downstream TUs never
		// need SPUD headers. (AS-08-u1 review finding #2.)
		PrivateDependencyModuleNames.AddRange(new string[] { "SPUD" });

		// AS-SPIKE-Scenario-u1: Editor Utility Widget deps for UArchSimScenarioWidget.
		// Gated by Target.Type == TargetType.Editor so packaged (shipping/client/server)
		// targets never link against these Editor-only modules.
		// WHY each dep:
		//   Blutility         — UEditorUtilityWidget + UEditorUtilitySubsystem base classes
		//   UMG               — UUserWidget (UEditorUtilityWidget's UMG parent)
		//   UMGEditor         — Editor-time UMG tooling (Tab spawner, palette registration)
		//
		// WHY EditorScriptingUtilities is EXCLUDED: that module requires the
		// EditorScriptingUtilities Plugin listed in ArchSim.uproject dependencies. Touching
		// .uproject violates iron rule #5. Since PlaceK1Column uses GEditor->GetEditorWorldContext()
		// + World->SpawnActor<> directly (no UEditorActorUtilities helper), the dep is unneeded.
		// WHY UnrealEd: GEditor global + UEditorEngine::GetEditorWorldContext() live in the
		// UnrealEd module. Without this dep, LNK2019 fires on __imp_GEditor and
		// __imp_GetEditorWorldContext. UnrealEd is always present in Editor targets (it IS the
		// Editor), so this dep is safe and doesn't bloat non-Editor builds (gate is TargetType.Editor).
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"Blutility",
				"UMG",
				"UMGEditor",
				"UnrealEd",
			});
		}
	}
}
