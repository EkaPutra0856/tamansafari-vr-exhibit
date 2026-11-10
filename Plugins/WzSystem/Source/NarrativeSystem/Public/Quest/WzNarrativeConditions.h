// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "Quest/WzNarrativeCondition.h"
#include "WzNarrativeConditions.generated.h"

class AActor;
class UWorld;
class UWzDialogueAsset;
class UWzDialogueSubsystem;
class UWzNarrativeSubsystem;

/** True after a number of seconds. */
UCLASS(meta = (DisplayName = "Delay"))
class NARRATIVESYSTEM_API UWzDelayCondition : public UWzNarrativeCondition
{
	GENERATED_BODY()

public:
	// ====== Delay ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delay", meta = (ClampMin = "0"))
	float Seconds = 1.f;

public:
	// ====== Events ====== //

	virtual void OnBegin_Implementation(const FWzNarrativeContext& Context) override;
	virtual void OnEnd_Implementation() override;

private:
	// ====== State ====== //

	FTimerHandle TimerHandle;
	TWeakObjectPtr<UWorld> World;
};

/** True when the game sends the event tag (or a child of it) enough times: "go to a place", "reach a checkpoint". */
UCLASS(meta = (DisplayName = "Event"))
class NARRATIVESYSTEM_API UWzEventCondition : public UWzNarrativeCondition
{
	GENERATED_BODY()

public:
	// ====== Event ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FGameplayTag Tag;

	/** How many times the event has to happen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (ClampMin = "1"))
	int32 Count = 1;

public:
	// ====== Events ====== //

	virtual void OnBegin_Implementation(const FWzNarrativeContext& Context) override;
	virtual void OnEnd_Implementation() override;

private:
	// ====== State ====== //

	TWeakObjectPtr<UWzNarrativeSubsystem> Subsystem;

private:
	// ====== Handlers ====== //

	void HandleEvent(FGameplayTag Event);
};

/** True when a dialogue ends. */
UCLASS(meta = (DisplayName = "Dialogue Ended"))
class NARRATIVESYSTEM_API UWzDialogueEndedCondition : public UWzNarrativeCondition
{
	GENERATED_BODY()

public:
	// ====== Dialogue ====== //

	/** The dialogue that has to end. Empty = the objective's Start Dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<UWzDialogueAsset> Dialogue;

public:
	// ====== Events ====== //

	virtual void OnBegin_Implementation(const FWzNarrativeContext& Context) override;
	virtual void OnEnd_Implementation() override;

private:
	// ====== State ====== //

	UPROPERTY(Transient)
	TObjectPtr<UWzDialogueAsset> Target;

	TWeakObjectPtr<UWzDialogueSubsystem> DialogueSubsystem;

private:
	// ====== Handlers ====== //

	void HandleDialogueEnded(UWzDialogueAsset* Ended);
};

/** True when enough actors with the tag were destroyed (killed, picked up, ...). The actors implement IWzNarrativeTarget. */
UCLASS(meta = (DisplayName = "Destroy Actors"))
class NARRATIVESYSTEM_API UWzDestroyActorsCondition : public UWzNarrativeCondition
{
	GENERATED_BODY()

public:
	// ====== Target ====== //

	/** Only actors of this class count. None = any class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destroy")
	TSubclassOf<AActor> ActorClass;

	/** Only actors answering to this tag count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destroy")
	FGameplayTag Tag;

	/** Destroys counted after the task began. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destroy", meta = (ClampMin = "1"))
	int32 Count = 1;

public:
	// ====== Events ====== //

	virtual void OnBegin_Implementation(const FWzNarrativeContext& Context) override;
	virtual void OnEnd_Implementation() override;

private:
	// ====== State ====== //

	FDelegateHandle DestroyedHandle;
	TWeakObjectPtr<UWorld> World;

private:
	// ====== Handlers ====== //

	void HandleActorDestroyed(AActor* Actor);
};