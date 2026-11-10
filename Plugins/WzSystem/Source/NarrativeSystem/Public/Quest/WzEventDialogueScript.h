// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Quest/WzNarrativeScript.h"
#include "WzEventDialogueScript.generated.h"

class UWzDialogueAsset;
class UWzDialogueSubsystem;
class UWzNarrativeSubsystem;

/** Plays a dialogue once when its narrative event arrives while the quest or objective is active: "arrive at a place, someone comments". */
UCLASS(meta = (DisplayName = "Event Dialogue"))
class NARRATIVESYSTEM_API UWzEventDialogueScript : public UWzNarrativeScript
{
	GENERATED_BODY()

public:
	// ====== Dialogues ====== //

	/** The event tag (or a child of it) and the dialogue it plays. Each plays at most once per run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogues")
	TMap<FGameplayTag, TObjectPtr<UWzDialogueAsset>> Dialogues;

public:
	// ====== Events ====== //

	virtual void OnBegin_Implementation(const FWzNarrativeContext& Context) override;
	virtual void OnEnd_Implementation(const FWzNarrativeContext& Context) override;

private:
	// ====== State ====== //

	TWeakObjectPtr<UWzNarrativeSubsystem> Subsystem;
	TWeakObjectPtr<UWzDialogueSubsystem> DialogueSubsystem;

	/** Keys of Dialogues that already played in this run. */
	TSet<FGameplayTag> Played;

private:
	// ====== Handlers ====== //

	void HandleEvent(FGameplayTag Event);
};