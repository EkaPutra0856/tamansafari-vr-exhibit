// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/WzNarrativeViewTypes.h"
#include "WidgetController.h"
#include "WzNarrativeWidgetController.generated.h"

class UWzDialogueSubsystem;
class UWzNarrativeSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWzDialogueActiveChangedSignature, bool, bActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWzDialogueLineChangedSignature, const FWzDialogueLineView&, Line);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWzDialogueSubtitleChangedSignature, const FText&, VisibleText, bool, bFullyRevealed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWzQuestViewSignature, const FWzQuestView&, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWzTaskViewSignature, int32, TaskIndex, const FWzTaskView&, Task);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWzTrackerClearedSignature);

/** CONTROLLER of the narrative MVC: turns Model events into view data for the widgets and forwards their clicks. */
UCLASS(BlueprintType, Blueprintable)
class NARRATIVESYSTEM_API UWzNarrativeWidgetController : public UWzWidgetController
{
	GENERATED_BODY()

public:
	// ====== Setup ====== //

	virtual void BindCallbacksToDependencies() override;
	virtual void BroadcastInitialValues() override;

public:
	// ====== Events ====== //

	/** A dialogue started (true) or ended (false): show / hide the dialogue. */
	UPROPERTY(BlueprintAssignable, Category = "Narrative|Dialogue")
	FWzDialogueActiveChangedSignature OnDialogueActiveChanged;

	/** A new line started: speaker name, portrait and colour. */
	UPROPERTY(BlueprintAssignable, Category = "Narrative|Dialogue")
	FWzDialogueLineChangedSignature OnLineChanged;

	/** The typewriter text changed. */
	UPROPERTY(BlueprintAssignable, Category = "Narrative|Dialogue")
	FWzDialogueSubtitleChangedSignature OnSubtitleChanged;

	/** A quest became active for the first time: play the toast. */
	UPROPERTY(BlueprintAssignable, Category = "Narrative|Quest")
	FWzQuestViewSignature OnQuestStarted;

	/** The tracker shows another quest or objective: rebuild its lines and slide them in. */
	UPROPERTY(BlueprintAssignable, Category = "Narrative|Quest")
	FWzQuestViewSignature OnObjectiveChanged;

	/** One tracker line changed (index into the quest view's Tasks): tick it or pulse its progress. */
	UPROPERTY(BlueprintAssignable, Category = "Narrative|Quest")
	FWzTaskViewSignature OnTaskChanged;

	/** No quest to track any more: fade the tracker out. */
	UPROPERTY(BlueprintAssignable, Category = "Narrative|Quest")
	FWzTrackerClearedSignature OnTrackerCleared;

private:
	// ====== References ====== //

	UPROPERTY(Transient)
	TObjectPtr<UWzDialogueSubsystem> DialogueSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UWzNarrativeSubsystem> NarrativeSubsystem;

private:
	// ====== State ====== //

	/** The current line's full text; the typewriter shows a prefix of it. */
	FString FullString;

	/** The quest the tracker shows. Invalid when none. */
	FGameplayTag TrackedQuest;

	/** Quests whose toast already played, so coming back from Released does not play it again. */
	TSet<FGameplayTag> AnnouncedQuests;

public:
	// ====== API ====== //

	UFUNCTION(BlueprintCallable, Category = "Narrative|Dialogue")
	void Skip();

	UFUNCTION(BlueprintCallable, Category = "Narrative|Dialogue")
	void EndDialogue();

private:
	// ====== Handlers ====== //

	/** Builds the line view from the active dialogue and the Speaker Set. */
	void HandleLineStarted(int32 LineIndex);

	void HandleRevealChanged(int32 RevealedChars);

	/** A quest started (toast + tracker) or changed what the tracker should show. */
	void HandleQuestStateChanged(FGameplayTag Quest, EWzQuestState State);

	void HandleTaskChanged(FGameplayTag Quest, int32 TaskIndex);

private:
	// ====== Helpers ====== //

	/** Follows the Model's tracked quest: shows the new one, or clears the tracker. */
	void Retrack();

	FWzQuestView BuildQuestView(const FGameplayTag& Quest) const;
	FWzTaskView BuildTaskView(const FGameplayTag& Quest, const FWzObjective& Objective, int32 TaskIndex) const;
};