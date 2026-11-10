// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ExplorationSystem : ModuleRules
{
	public ExplorationSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"UMG",
			"SlateCore",
			"DeveloperSettings",
			"UISystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ProceduralMeshComponent",
			"NavigationSystem"
		});
	}
}
