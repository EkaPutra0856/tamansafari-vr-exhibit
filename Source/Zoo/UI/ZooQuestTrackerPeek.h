// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Tickable.h"
#include "UObject/Object.h"
#include "UI/WzNarrativeViewTypes.h"
#include "ZooQuestTrackerPeek.generated.h"

class UButton;
class UCanvasPanelSlot;
class UUserWidget;
class UWidget;
class UWzNarrativeWidgetController;

/**
 * Keeps the quest tracker (the mission panel in the corner) out of the way: it shows for a moment at the start and
 * whenever the objective changes, then folds into a small "MISI" button. A click unfolds it, and it folds again once
 * the pointer (mouse or VR laser) has stayed off it for a few seconds. Built by the HUD on top of the narrative overlay.
 */
UCLASS()
class UZooQuestTrackerPeek : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UZooQuestTrackerPeek, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return Tracker != nullptr; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	// ====== API ====== //

	/** Hooks the tracker named TrackerName inside the overlay and builds the fold button next to it. */
	void Setup(UUserWidget* Overlay, UWzNarrativeWidgetController* Controller, FName TrackerName = TEXT("Tracker"));

	/** Unfolds the tracker; it folds once the pointer left it for HideDelay (or after Seconds when nothing points at it). */
	void Expand();

	void Minimize();

protected:
	// ====== Tuning ====== //

	/** Seconds the tracker stays open at the start and after an objective change. */
	float IntroSeconds = 6.f;

	/** Seconds without the pointer on it before it folds. */
	float HideDelay = 5.f;

private:
	// ====== Handlers ====== //

	UFUNCTION()
	void HandlePeekClicked();

	UFUNCTION()
	void HandleObjectiveChanged(const FWzQuestView& Quest);

	UFUNCTION()
	void HandleQuestStarted(const FWzQuestView& Quest);

	// ====== References ====== //

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> OverlayWidget;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Tracker;

	/** The small folded button ("MISI"). */
	UPROPERTY(Transient)
	TObjectPtr<UButton> PeekButton;

	/** Invisible button over the open tracker: its hover is "the pointer is on the tracker" (mouse or laser). */
	UPROPERTY(Transient)
	TObjectPtr<UButton> HitArea;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> HitSlot;

	// ====== State ====== //

	bool bExpanded = true;
	float HideTimer = 0.f;
	ESlateVisibility TrackerVisibility = ESlateVisibility::HitTestInvisible;
};
