// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ZooPlaceCardsAsset.generated.h"

class UTexture2D;

/** What the "Mau ke mana?" list shows for one place, keyed by the Path Guide place tag. */
USTRUCT(BlueprintType)
struct FZooPlaceCard
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (Categories = "PathGuide.Place"))
	FGameplayTag PlaceTag;

	/** Number on the card badge (matches the in-world trail map). Also the list order. 0 = unnumbered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (ClampMin = "0"))
	int32 Number = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card")
	TSoftObjectPtr<UTexture2D> Thumbnail;
};

/** Data-only card content for every place in the destination list. */
UCLASS(BlueprintType)
class UZooPlaceCardsAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ====== Data ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cards", meta = (TitleProperty = "PlaceTag"))
	TArray<FZooPlaceCard> Cards;

public:
	// ====== Getters ====== //

	/** Lookup without copying; null when the tag has no card. */
	const FZooPlaceCard* FindCard(const FGameplayTag& PlaceTag) const;

#if WITH_EDITOR
public:
	// ====== Lifecycle ====== //

	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};