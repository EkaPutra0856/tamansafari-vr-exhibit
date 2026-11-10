// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WzDialogueAsset.generated.h"

class UAnimSequenceBase;
class USoundBase;
class UWzDialogueCameraEffect;

/** How fast a line types out and how long it stays. */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEM_API FWzDialogueTiming
{
	GENERATED_BODY()

	/** Typewriter speed. 0 = the whole line appears at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0"))
	float TypewriterCharsPerSecond = 40.f;

	/** Reading speed used for the buffer after Skip reveals unread text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "1"))
	float ReadingCharsPerSecond = 15.f;

	/** Seconds a finished line stays up; also the minimum reading buffer after Skip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0"))
	float MinHold = 1.5f;
};

/** One thing said in a conversation. */
USTRUCT(BlueprintType)
struct NARRATIVESYSTEM_API FWzDialogueLine
{
	GENERATED_BODY()

	/** Who says it. Display data comes from the Speaker Set; the world actor is found by this tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line")
	FGameplayTag Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (MultiLine = true))
	FText Text;

	/** Optional. Plays attached to the speaker actor (2D for a narrator). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line")
	TSoftObjectPtr<USoundBase> Voice;

	/** Optional. Played on the speaker's skeletal mesh (slot from the settings) while the line runs. The same animation that is already playing is not restarted, only re-synced when it is off by more than the sync tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line")
	TSoftObjectPtr<UAnimSequenceBase> Animation;

	/** Where in the animation this line starts, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (ClampMin = "0"))
	float AnimationStartTime = 0.f;

	/** Optional camera effect for this line. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Line")
	TObjectPtr<UWzDialogueCameraEffect> CameraEffect;
};

/** One whole conversation: its lines in order plus how it plays. */
UCLASS(BlueprintType)
class NARRATIVESYSTEM_API UWzDialogueAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FWzDialogueLine> Lines;

	/** True: lines move on by themselves. False: a finished line waits for Skip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	bool bAutoAdvance = true;

	/** A bark: never queues, never blocks input or camera, cut by any other dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	bool bAmbient = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "!bAmbient"))
	bool bBlockMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "!bAmbient"))
	bool bBlockLook = true;

	UPROPERTY(EditAnywhere, Category = "Dialogue", meta = (InlineEditConditionToggle))
	bool bOverrideTiming = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (EditCondition = "bOverrideTiming"))
	FWzDialogueTiming Timing;

	/** This dialogue's timing, or the project default from the settings. */
	FWzDialogueTiming GetEffectiveTiming() const;

	/** True when any line has a camera effect (look input is then blocked and the view restored at the end). Never for an ambient one. */
	bool HasCameraEffects() const;
};