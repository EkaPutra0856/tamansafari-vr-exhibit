// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "PathGuideTypes.generated.h"

UENUM(BlueprintType)
enum class EPathGuideState : uint8
{
	Idle,
	Guiding,
	Arrived,
	Failed
};

/** One selectable destination. The WHERE is the AWzPathGuideDestination with the same tag. */
USTRUCT(BlueprintType)
struct EXPLORATIONSYSTEM_API FPathGuidePlace
{
	GENERATED_BODY()

	/** Unique id; must match the PlaceTag of an AWzPathGuideDestination placed in the map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place", meta = (Categories = "PathGuide.Place"))
	FGameplayTag PlaceTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place")
	FText DisplayName;

	/** Guidance ends when the visitor is this close (horizontally) to the destination anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place", meta = (ClampMin = "50"))
	float ArrivalRadius = 400.f;
};

/** Ground-draped route ribbon: a left, centre and right lane per cross-section. World space, cm. */
struct EXPLORATIONSYSTEM_API FPathGuideRibbon
{
	TArray<FVector> Lefts;
	TArray<FVector> Centers;
	TArray<FVector> Rights;
	TArray<FVector> Normals;

	/** Distance along the centreline of each cross-section. */
	TArray<float> Distances;
	float TotalLength = 0.f;

	FORCEINLINE bool IsValid() const { return Centers.Num() >= 2; }
	FORCEINLINE int32 Num() const { return Centers.Num(); }
};
