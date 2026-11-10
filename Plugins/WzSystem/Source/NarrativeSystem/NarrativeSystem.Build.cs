// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NarrativeSystem : ModuleRules
{
	public NarrativeSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"UMG",
			"DeveloperSettings",
			"UISystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"SlateCore"
		});
	}
}
