// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "WzNarrativeTarget.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UWzNarrativeTarget : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by an actor a narrative task can target, such as Destroy Actors (C++ or Blueprint). */
class NARRATIVESYSTEM_API IWzNarrativeTarget
{
	GENERATED_BODY()

public:
	// ====== Tags ====== //

	/** The tags this actor answers to. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Narrative")
	FGameplayTagContainer GetNarrativeTags() const;
};