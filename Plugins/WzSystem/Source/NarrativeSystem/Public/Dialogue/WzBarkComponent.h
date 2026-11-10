// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "WzBarkComponent.generated.h"

class UWzDialogueAsset;
class UWzDialogueSubsystem;

/** Says an ambient dialogue now and then (barks, fun facts) in the silence between regular ones. Add it to any actor and fill Barks; the owner gates it with SetAllowed. A regular dialogue is never delayed: the subsystem cuts a bark for it. */
UCLASS(ClassGroup = (Wz), meta = (BlueprintSpawnableComponent))
class NARRATIVESYSTEM_API UWzBarkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	// ====== Barks ====== //

	/** Ambient dialogues (bAmbient). All of them play before any repeats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark")
	TArray<TObjectPtr<UWzDialogueAsset>> Barks;

	/** Seconds of silence before the next bark: a random gap in this range, counted from the end of any dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark", meta = (ClampMin = "0"))
	float MinGap = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark", meta = (ClampMin = "0"))
	float MaxGap = 40.f;

	/** Seconds before trying again when it may not speak right now. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark", meta = (ClampMin = "0.5"))
	float RetrySeconds = 3.f;

	/** Stays quiet until a regular dialogue has ended once (so a story opening is never talked over). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark")
	bool bWaitForFirstDialogue = true;

private:
	// ====== State ====== //

	TWeakObjectPtr<UWzDialogueSubsystem> Dialogue;
	FTimerHandle Timer;

	/** Bark indices still to say, the next one last. */
	TArray<int32> Bag;
	int32 LastBark = INDEX_NONE;

	/** Barks may be scheduled (no first dialogue to wait for, or it ended). */
	bool bUnlocked = false;

	/** The owner says it may speak now. */
	bool bAllowed = false;

public:
	/** The owner's gate: false holds the next bark back (the countdown retries). */
	UFUNCTION(BlueprintCallable, Category = "Bark")
	FORCEINLINE void SetAllowed(const bool bInAllowed) { bAllowed = bInAllowed; }

private:
	// ====== Schedule ====== //

	/** Counts down to the next bark. */
	void Schedule(float Seconds);

	void Cancel();

	/** Says the next bark, or tries again shortly. */
	void HandleTimer();

private:
	// ====== Bag ====== //

	/** The next bark in the shuffled order (refilled when empty). Null when there is none. */
	UWzDialogueAsset* PeekBark();

	void RefillBag();

private:
	// ====== Dialogue ====== //

	/** Silence is over; the next bark is counted from this dialogue's end. */
	void HandleDialogueStarted(UWzDialogueAsset* Started);
	void HandleDialogueEnded(UWzDialogueAsset* Ended);
};