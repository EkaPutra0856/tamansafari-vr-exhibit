// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"
#include "WzNarrativeDirector.generated.h"

class UEdGraph;
class UWzQuest;

/** The game's narrative: every quest and the arrows between them. One per game (Project Settings > Wz Narrative). */
UCLASS(BlueprintType)
class NARRATIVESYSTEM_API UWzNarrativeDirector : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ====== Quests ====== //

	/** An arrow into a quest is one of its Prerequisites. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Director")
	TArray<TObjectPtr<UWzQuest>> Quests;

#if WITH_EDITORONLY_DATA
	/** What the graph editor shows (node positions). The quests above are the truth. */
	UPROPERTY()
	TObjectPtr<UEdGraph> EdGraph;
#endif

public:
	// ====== Lifecycle ====== //

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	// ====== Getters ====== //

	/** The quest with the tag, or null. */
	UWzQuest* FindQuest(const FGameplayTag& QuestTag) const;
};