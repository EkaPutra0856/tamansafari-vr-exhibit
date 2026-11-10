// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ZooHUD.generated.h"

class UWzNarrativeWidgetController;
class UWzWidget;
class UZooPathGuideWidgetController;
class UZooQuestTrackerPeek;

/**
 * The game's HUD. Creates the Path Guide and narrative widgets and their widget controllers through CreateWzWidget.
 * Make a Blueprint child, set its widget classes and use it as the game mode's HUD.
 */
UCLASS(Blueprintable)
class AZooHUD : public AHUD
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ====== Path Guide ====== //

	/** The Path Guide widget Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category = "PathGuide")
	TSubclassOf<UWzWidget> PathGuideWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UWzWidget> PathGuideWidget;

	UPROPERTY(EditDefaultsOnly, Category = "PathGuide")
	TSubclassOf<UZooPathGuideWidgetController> PathGuideWidgetControllerClass;

	UPROPERTY(Transient)
	TObjectPtr<UZooPathGuideWidgetController> PathGuideWidgetController;

private:
	// ====== Narrative ====== //

	/** The narrative overlay Blueprint: dialogue, objective tracker and quest toast. */
	UPROPERTY(EditDefaultsOnly, Category = "Narrative")
	TSubclassOf<UWzWidget> NarrativeWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UWzWidget> NarrativeWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Narrative")
	TSubclassOf<UWzNarrativeWidgetController> NarrativeWidgetControllerClass;

	UPROPERTY(Transient)
	TObjectPtr<UWzNarrativeWidgetController> NarrativeWidgetController;

	/** Folds the quest tracker into a small button after its intro (and after each new objective). */
	UPROPERTY(Transient)
	TObjectPtr<UZooQuestTrackerPeek> TrackerPeek;
};
