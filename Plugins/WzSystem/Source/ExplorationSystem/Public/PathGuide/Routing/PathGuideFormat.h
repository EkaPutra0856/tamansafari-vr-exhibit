// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWzPathGuideUIConfig;

/** Display texts for distances and walking time. Lengths in cm; patterns come from the UI config. */
namespace PathGuide::Format
{
	/** "7 m" (1 m steps under 10 m), "180 m" (5 m steps under 1 km), "1.2 km". */
	EXPLORATIONSYSTEM_API FText Distance(float Length, const UWzPathGuideUIConfig& Config);

	/** "3 min" (rounded up) or "< 1 min". */
	EXPLORATIONSYSTEM_API FText Eta(float Length, const UWzPathGuideUIConfig& Config);

	/** "180 m · 3 min" */
	EXPLORATIONSYSTEM_API FText DistanceAndEta(float Length, const UWzPathGuideUIConfig& Config);
}