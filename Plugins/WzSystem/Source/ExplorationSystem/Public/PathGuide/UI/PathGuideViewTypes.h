// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PathGuide/PathGuideTypes.h"
#include "PathGuideViewTypes.generated.h"

/** Guidance status for the HUD: idle or guiding, and where to. */
USTRUCT(BlueprintType)
struct EXPLORATIONSYSTEM_API FPathGuideGuidanceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	EPathGuideState State = EPathGuideState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	FGameplayTag PlaceTag;

	/** Destination name; empty while idle. */
	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	FText PlaceName;

	/** Remaining distance while guiding ("120 m"); empty while idle. */
	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	FText DistanceText;

	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	bool bIsGuiding = false;
};

/** Screen-space pin for the destination. Positions are in Slate units (already divided by the viewport scale). */
USTRUCT(BlueprintType)
struct EXPLORATIONSYSTEM_API FPathGuideWaypointView
{
	GENERATED_BODY()

	/** Where to place the pin (anchor its bottom centre here). Clamped to the screen edge when off-screen. */
	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	FVector2D ScreenPosition = FVector2D::ZeroVector;

	/** False when the destination is behind the visitor or outside the screen; the pin then sits on the edge. */
	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	bool bOnScreen = false;

	/** Direction from the screen centre to the destination: 0 = right, 90 = down. Rotate the edge arrow by this. */
	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	float EdgeAngleDegrees = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	FText DistanceText;

	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	float Opacity = 1.f;
};

USTRUCT(BlueprintType)
struct EXPLORATIONSYSTEM_API FPathGuideArrivalView
{
	GENERATED_BODY()

	/** "You have arrived at Giraffe Feeding Deck" */
	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	FText Message;

	UPROPERTY(BlueprintReadOnly, Category = "PathGuide")
	float DisplaySeconds = 0.f;
};
