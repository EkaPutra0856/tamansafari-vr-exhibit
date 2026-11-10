// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WzNarrativeContext.generated.h"

class APlayerController;
class UWzDialogueAsset;
class UWzNarrativeSubsystem;

/** What a condition or script needs to reach the game while it runs. */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEM_API FWzNarrativeContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Narrative")
	TObjectPtr<UWzNarrativeSubsystem> Subsystem;

	UPROPERTY(BlueprintReadOnly, Category = "Narrative")
	TObjectPtr<APlayerController> PlayerController;

	/** The Start Dialogue of the objective this belongs to. Null when none or not an objective. */
	UPROPERTY(BlueprintReadOnly, Category = "Narrative")
	TObjectPtr<UWzDialogueAsset> ObjectiveDialogue;
};
