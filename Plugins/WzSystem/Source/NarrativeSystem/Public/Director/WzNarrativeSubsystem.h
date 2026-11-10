// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "Quest/WzNarrativeContext.h"
#include "Quest/WzQuest.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "WzNarrativeSubsystem.generated.h"

class APlayerController;
class UWzDialogueAsset;
class UWzDialogueSubsystem;
class UWzNarrativeCondition;
class UWzNarrativeDirector;
class UWzNarrativeScript;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnNarrativeEvent, FGameplayTag /*Event*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnQuestStateChanged, FGameplayTag /*Quest*/, EWzQuestState /*State*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnObjectiveChanged, FGameplayTag /*Quest*/, int32 /*ObjectiveIndex*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTaskChanged, FGameplayTag /*Quest*/, int32 /*TaskIndex*/);
DECLARE_MULTICAST_DELEGATE(FOnNarrativeListening);
DECLARE_MULTICAST_DELEGATE(FOnNarrativeCompleted);

/** Where one quest stands. */
USTRUCT()
struct FWzQuestProgress
{
	GENERATED_BODY()

	UPROPERTY()
	EWzQuestState State = EWzQuestState::Locked;

	UPROPERTY()
	int32 ObjectiveIndex = 0;
};

/** Everything worth saving about the narrative. */
USTRUCT(BlueprintType)
struct FWzNarrativeState
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FGameplayTag, FWzQuestProgress> Quests;
};

/** What runs for one quest right now. */
USTRUCT()
struct FWzQuestRuntime
{
	GENERATED_BODY()

	/** Its start condition, while it waits for the free slot. */
	UPROPERTY(Transient)
	TObjectPtr<UWzNarrativeCondition> Trigger;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWzNarrativeScript>> QuestScripts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWzNarrativeScript>> ObjectiveScripts;

	/** The current objective's tasks, while they listen. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWzNarrativeCondition>> Tasks;

	/** The Start Dialogue the objective waits on. Null when none or not started. */
	UPROPERTY(Transient)
	TObjectPtr<UWzDialogueAsset> IntroDialogue;

	/** The Complete Dialogue the objective waits to end. */
	UPROPERTY(Transient)
	TObjectPtr<UWzDialogueAsset> PendingDialogue;

	FTimerHandle IntroTimer;

	/** The delay and the Start Dialogue are done: the tasks may listen. */
	bool bIntroDone = false;
};

/** MODEL of the narrative MVC: runs the Director's quests for one local player. */
UCLASS()
class NARRATIVESYSTEM_API UWzNarrativeSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

public:
	// ====== Events ====== //

	/** The game told us something happened. */
	FOnNarrativeEvent OnEvent;

	FOnQuestStateChanged OnQuestStateChanged;

	/** A new objective began. */
	FOnObjectiveChanged OnObjectiveChanged;

	/** A task's progress or completion changed. */
	FOnTaskChanged OnTaskChanged;

	/** A start condition or the tasks began listening: the game may send again what is already true (the place the player stands in). */
	FOnNarrativeListening OnListening;

	/** Every mandatory quest is completed. */
	FOnNarrativeCompleted OnNarrativeCompleted;

public:
	// ====== API ====== //

	/** Tells the narrative something happened ("reached a place", ...). Event tasks listening for the tag count it. */
	UFUNCTION(BlueprintCallable, Category = "Narrative")
	void SendEvent(FGameplayTag Event);

private:
	// ====== State ====== //

	UPROPERTY(Transient)
	TObjectPtr<UWzNarrativeDirector> Director;

	/** Survives level changes: the narrative picks up where it was. */
	UPROPERTY(Transient)
	FWzNarrativeState State;

	UPROPERTY(Transient)
	TMap<FGameplayTag, FWzQuestRuntime> Runtimes;

	/** The controller whose pawn we wait for. */
	TWeakObjectPtr<APlayerController> BoundController;

	FDelegateHandle DialogueEndedHandle;
	bool bStarted = false;
	bool bRefreshing = false;
	bool bRefreshAgain = false;
	bool bNarrativeCompleted = false;

public:
	// ====== Getters ====== //

	UFUNCTION(BlueprintPure, Category = "Narrative")
	EWzQuestState GetQuestState(const FGameplayTag& Quest) const;

	UFUNCTION(BlueprintPure, Category = "Narrative")
	bool IsQuestCompleted(const FGameplayTag& Quest) const;

	/** The quest the player should see: the active one, else the first released one. Invalid when none. */
	UFUNCTION(BlueprintPure, Category = "Narrative")
	FGameplayTag GetTrackedQuest() const;

	UFUNCTION(BlueprintPure, Category = "Narrative")
	UWzQuest* FindQuest(const FGameplayTag& Quest) const;

	UFUNCTION(BlueprintPure, Category = "Narrative")
	int32 GetObjectiveIndex(const FGameplayTag& Quest) const;

	/** The quest's current objective, or null when it has none running. */
	const FWzObjective* GetObjective(const FGameplayTag& Quest) const;

	/** The tasks of the quest's current objective: listening, or done and kept until the next objective. */
	const TArray<TObjectPtr<UWzNarrativeCondition>>& GetTasks(const FGameplayTag& Quest) const;

private:
	// ====== Helpers ====== //

	UWzDialogueSubsystem* GetDialogue() const;
	UWorld* GetPlayerWorld() const;
	FWzNarrativeContext MakeContext(UWzDialogueAsset* ObjectiveDialogue);
	bool HasActiveQuest() const;
	bool IsCurrentObjectiveDone(const FGameplayTag& Quest) const;
	void SetQuestState(const FGameplayTag& Quest, EWzQuestState NewState);

private:
	// ====== Session ====== //

	/** Starts once the player has a pawn. */
	void BeginNarrative();

	/** Stops everything that runs. The progress stays. */
	void TearDown();

	/** Unlocks quests and decides who listens. A call made during a pass runs after it. */
	void Refresh();
	void RefreshOnce();

private:
	// ====== Listening ====== //

	/** A Ready quest's start condition. */
	void SetTriggerListening(const FGameplayTag& Quest, bool bListen);

	/** The current objective's tasks. */
	void SetTasksListening(const FGameplayTag& Quest, bool bListen);

private:
	// ====== Scripts ====== //

	void StartQuestScripts(const FGameplayTag& Quest);

	/** Runs copies of the templates and keeps them in Running, to end later. */
	void BeginScripts(const TArray<TObjectPtr<UWzNarrativeScript>>& Templates, TArray<TObjectPtr<UWzNarrativeScript>>& Running);

	void EndScripts(TArray<TObjectPtr<UWzNarrativeScript>>& Scripts);

private:
	// ====== Quest Flow ====== //

	void ActivateQuest(const FGameplayTag& Quest);
	void BeginObjective(const FGameplayTag& Quest, int32 Index);

	/** The delay passed: Start Dialogue, then the tasks. */
	void FinishIntro(FGameplayTag Quest);

	/** The tasks are done: Complete Dialogue, then FinishObjective. */
	void CompleteObjective(const FGameplayTag& Quest);

	void FinishObjective(const FGameplayTag& Quest);
	void CompleteQuest(const FGameplayTag& Quest);

private:
	// ====== Handlers ====== //

	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	void HandleTriggerChanged(UWzNarrativeCondition* Condition, FGameplayTag Quest);
	void HandleTaskChanged(UWzNarrativeCondition* Condition, FGameplayTag Quest);
	void HandleDialogueEnded(UWzDialogueAsset* Ended);
};