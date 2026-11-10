// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Quest/WzQuest.h"
#include "WzNarrativeViewTypes.generated.h"

class UTexture2D;

/** Everything a widget needs to show one line, resolved from the Speaker Set. */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEM_API FWzDialogueLineView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FGameplayTag Speaker;

	/** Empty when the tag is not in the Speaker Set. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText SpeakerName;

	/** Null = hide the portrait. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<UTexture2D> Portrait;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FLinearColor NameColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText FullText;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	int32 LineIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	int32 NumLines = 0;
};

/** One tracker line. */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEM_API FWzTaskView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Task")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Task")
	bool bOptional = false;

	UPROPERTY(BlueprintReadOnly, Category = "Task")
	bool bDone = false;

	/** Progress; show "Current / Required" only when Required is above 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Task")
	int32 Current = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Task")
	int32 Required = 1;
};

/** The tracked quest as the tracker and the toast show it. Tasks without a description are left out. */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEM_API FWzQuestView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FText QuestName;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	EWzQuestType Type = EWzQuestType::Mandatory;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FText ObjectiveTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TArray<FWzTaskView> Tasks;
};