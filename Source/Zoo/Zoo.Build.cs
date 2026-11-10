// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Zoo : ModuleRules
{
	public Zoo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "UISystem", "GameplayTags", "NarrativeSystem", "ExplorationSystem", "AIModule", "Niagara" });

		PrivateDependencyModuleNames.AddRange(new string[] { "HeadMountedDisplay", "XRBase", "NavigationSystem", "Json", "JsonUtilities", "ImageWriteQueue", "ImageWrapper", "LevelSequence", "MovieScene" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
