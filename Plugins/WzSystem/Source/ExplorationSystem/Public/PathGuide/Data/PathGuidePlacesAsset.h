// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PathGuide/PathGuideTypes.h"
#include "Engine/DataAsset.h"
#include "PathGuidePlacesAsset.generated.h"

/** Data-only list of every place the visitor can be guided to. */
UCLASS(BlueprintType)
class EXPLORATIONSYSTEM_API UWzPathGuidePlacesAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ====== Data ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Places", meta = (TitleProperty = "DisplayName"))
	TArray<FPathGuidePlace> Places;

public:
	// ====== Getters ====== //

	/** Valid places in array order. */
	TArray<FPathGuidePlace> GetPlaces() const;

	/** Lookup without copying; null when the tag is not listed. */
	const FPathGuidePlace* FindPlacePtr(const FGameplayTag& PlaceTag) const;

#if WITH_EDITOR
public:
	// ====== Lifecycle ====== //

	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
