// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Quest/WzNarrativeContext.h"
#include "UObject/Object.h"
#include "WzNarrativeCondition.generated.h"

class UWzNarrativeCondition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnConditionChanged, UWzNarrativeCondition* /*Condition*/);

/** Something that becomes true: a quest's start trigger or an objective's task. Subclass in C++ or Blueprint. */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class NARRATIVESYSTEM_API UWzNarrativeCondition : public UObject
{
	GENERATED_BODY()

public:
	// ====== Settings ====== //

	/** The tracker line. Empty = hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FText Description;

	/** An optional task never holds its objective back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	bool bOptional = false;

public:
	// ====== Events ====== //

	/** It started listening. Call Complete or SetProgress when it becomes true. */
	UFUNCTION(BlueprintNativeEvent, Category = "Condition")
	void OnBegin(const FWzNarrativeContext& Context);

	/** It stopped listening: remove whatever OnBegin added. */
	UFUNCTION(BlueprintNativeEvent, Category = "Condition")
	void OnEnd();

	/** Progress or completion changed. */
	FOnConditionChanged OnChanged;

public:
	// ====== API ====== //

	/** Starts it, once. Called by the narrative subsystem. */
	void Begin(const FWzNarrativeContext& Context);

	/** Stops it for good. Called by the narrative subsystem. */
	void End();

	UFUNCTION(BlueprintCallable, Category = "Condition")
	void Complete();

	/** Counts up to Required; completes when it is reached. */
	UFUNCTION(BlueprintCallable, Category = "Condition")
	void SetProgress(int32 InCurrent, int32 InRequired);

private:
	// ====== State ====== //

	enum class EPhase : uint8
	{
		Idle,
		Running,
		Ended
	};

	EPhase Phase = EPhase::Idle;
	bool bCompleted = false;
	int32 Current = 0;
	int32 Required = 1;

public:
	UFUNCTION(BlueprintPure, Category = "Condition")
	FORCEINLINE bool IsCompleted() const { return bCompleted; }

	UFUNCTION(BlueprintPure, Category = "Condition")
	FORCEINLINE int32 GetCurrent() const { return Current; }

	UFUNCTION(BlueprintPure, Category = "Condition")
	FORCEINLINE int32 GetRequired() const { return Required; }
};