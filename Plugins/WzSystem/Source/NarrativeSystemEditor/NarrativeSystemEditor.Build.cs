// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NarrativeSystemEditor : ModuleRules
{
	public NarrativeSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"NarrativeSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"GraphEditor",
			"AssetDefinition",
			"PropertyEditor",
			"ToolMenus",
			"EditorFramework"
		});
	}
}