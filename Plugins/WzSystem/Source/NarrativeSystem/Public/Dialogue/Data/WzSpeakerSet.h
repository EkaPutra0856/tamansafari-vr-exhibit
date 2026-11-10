// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WzSpeakerSet.generated.h"

class UTexture2D;

/** How a speaker is shown in the dialogue widget. */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEM_API FWzSpeakerInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker")
	FText DisplayName;

	/** Optional. No portrait = the widget hides its portrait. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker")
	FLinearColor NameColor = FLinearColor::White;
};

/** Display data of every speaker tag of the game. Referenced by the Wz Dialogue project settings. */
UCLASS(BlueprintType)
class NARRATIVESYSTEM_API UWzSpeakerSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speakers")
	TMap<FGameplayTag, FWzSpeakerInfo> Speakers;
};