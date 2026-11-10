// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PathGuideWalkway.generated.h"

class USplineComponent;

/** One piece of the walkway network. Touching endpoints form junctions. */
UCLASS(BlueprintType, HideCategories = (Rendering, Replication, Collision, Input, HLOD, Physics, Networking, Cooking, Actor))
class EXPLORATIONSYSTEM_API AWzPathGuideWalkway : public AActor
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AWzPathGuideWalkway();

public:
	// ====== References ====== //

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PathGuide")
	TObjectPtr<USplineComponent> Spline;

#if WITH_EDITOR
public:
	// ====== Editor Tools ====== //

	/** Drops every spline point onto the ground. */
	UFUNCTION(CallInEditor, Category = "PathGuide")
	void SnapToGround();

	/** Draws the network for 15 s (red = island, magenta = far destination). */
	UFUNCTION(CallInEditor, Category = "PathGuide")
	void ValidateNetwork();
#endif
};