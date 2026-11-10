// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "WzQuest.generated.h"

class UWzDialogueAsset;
class UWzNarrativeCondition;
class UWzNarrativeScript;

UENUM(BlueprintType)
enum class EWzQuestType : uint8
{
	/** The main story: the narrative is finished when all of these are done. */
	Mandatory,
	Optional
};

UENUM(BlueprintType)
enum class EWzQuestState : uint8
{
	/** A prerequisite quest is not completed yet. */
	Locked,

	/** Can start: waiting for its start condition while no quest is active. */
	Ready,

	Active,

	/** Left the active slot on purpose (Releases Quest); comes back when its tasks are done. */
	Released,

	Completed
};

/** One step of a quest. Objectives run one after another. */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEM_API FWzObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText Title;

	/** Seconds after the objective begins before its dialogue and tasks start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (ClampMin = "0"))
	float StartDelay = 0.f;

	/** Plays when the objective begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<UWzDialogueAsset> StartDialogue;

	/** Plays when the tasks are done; the objective ends after it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<UWzDialogueAsset> CompleteDialogue;

	/** The quest leaves the active slot while this runs, so the player may start another. Its tasks only listen while no quest is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bReleasesQuest = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Objective")
	TArray<TObjectPtr<UWzNarrativeScript>> Scripts;

	/** All at once. The objective is done when every required one is true. None = done when the Start Dialogue ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Objective")
	TArray<TObjectPtr<UWzNarrativeCondition>> Tasks;
};

/** A story quest: a node of the Director graph. */
UCLASS(EditInlineNew, DefaultToInstanced)
class NARRATIVESYSTEM_API UWzQuest : public UObject
{
	GENERATED_BODY()

public:
	// ====== Identity ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (Categories = "Narrative.Quest"))
	FGameplayTag QuestTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	EWzQuestType Type = EWzQuestType::Mandatory;

public:
	// ====== Flow ====== //

	/** Quests that must be completed first (the arrows pointing at this one). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (Categories = "Narrative.Quest"))
	TArray<FGameplayTag> Prerequisites;

	/** What starts the quest once it can. None = as soon as no quest is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Quest")
	TObjectPtr<UWzNarrativeCondition> StartCondition;

	/** Run when the quest first starts, ended when it completes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Quest")
	TArray<TObjectPtr<UWzNarrativeScript>> Scripts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FWzObjective> Objectives;
};