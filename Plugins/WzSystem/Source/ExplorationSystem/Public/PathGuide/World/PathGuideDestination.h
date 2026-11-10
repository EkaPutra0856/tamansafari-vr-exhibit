// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "PathGuideDestination.generated.h"

/**
 * Anchor for a place. Drop it in the level, move it to the real spot and set the PlaceTag to the tag used in
 * UWzPathGuidePlacesAsset. Its transform is the destination point (the spot the beacon marks); its forward vector is the way
 * visitors face on arrival.
 */
UCLASS(BlueprintType, HideCategories = (Rendering, Replication, Collision, Input, HLOD, Physics, Networking, Cooking, Actor))
class EXPLORATIONSYSTEM_API AWzPathGuideDestination : public AActor
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AWzPathGuideDestination();

public:
	// ====== Place ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PathGuide", meta = (Categories = "PathGuide.Place"))
	FGameplayTag PlaceTag;
};
