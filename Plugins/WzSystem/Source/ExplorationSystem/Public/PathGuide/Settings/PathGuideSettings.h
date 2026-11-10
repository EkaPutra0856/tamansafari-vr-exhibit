// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PathGuideSettings.generated.h"

class UWzPathGuideConfig;
class UWzPathGuidePlacesAsset;
class UWzPathGuideUIConfig;

/** Project Settings > Plugins > Path Guide. Points the runtime at the data assets. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Path Guide"))
class EXPLORATIONSYSTEM_API UWzPathGuideSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ====== Data ====== //

	UPROPERTY(Config, EditAnywhere, Category = "Data")
	TSoftObjectPtr<UWzPathGuidePlacesAsset> PlacesAsset;

	UPROPERTY(Config, EditAnywhere, Category = "Data")
	TSoftObjectPtr<UWzPathGuideConfig> ConfigAsset;

	UPROPERTY(Config, EditAnywhere, Category = "Data")
	TSoftObjectPtr<UWzPathGuideUIConfig> UIConfigAsset;

public:
	// ====== Lifecycle ====== //

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};