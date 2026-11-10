// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dialogue/Data/WzDialogueAsset.h"
#include "Engine/DeveloperSettings.h"
#include "WzNarrativeSettings.generated.h"

class UWzNarrativeDirector;
class UWzSpeakerSet;

/** Project Settings > Plugins > Wz Narrative. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Wz Narrative"))
class NARRATIVESYSTEM_API UWzNarrativeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

public:
	// ====== Narrative ====== //

	/** The game's quests. None = no narrative. */
	UPROPERTY(Config, EditAnywhere, Category = "Narrative")
	TSoftObjectPtr<UWzNarrativeDirector> Director;

public:
	// ====== Dialogue ====== //

	/** Name, portrait and colour of the speaker tags. */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue")
	TSoftObjectPtr<UWzSpeakerSet> SpeakerSet;

	/** Used by every dialogue that does not override its timing. */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue")
	FWzDialogueTiming DefaultTiming;

	/** Animation slot on the speaker's Animation Blueprint. */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue")
	FName AnimationSlot = TEXT("DefaultSlot");

	/** Blend in / out seconds of a line animation. */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue", meta = (ClampMin = "0"))
	float AnimationBlendTime = 0.25f;

	/** A playing line animation is re-synced only when it is off its start time by more than this (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue", meta = (ClampMin = "0"))
	float AnimationSyncTolerance = 0.3f;

	/** How fast the view glides back after a dialogue (RInterpTo speed). 0 = snap back. */
	UPROPERTY(Config, EditAnywhere, Category = "Dialogue", meta = (ClampMin = "0"))
	float RestoreInterpSpeed = 4.f;
};