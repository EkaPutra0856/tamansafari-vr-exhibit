// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZooHUD.h"

#include "GameFramework/PlayerController.h"
#include "UI/PathGuide/ZooPathGuideWidgetController.h"
#include "UI/WzNarrativeWidgetController.h"
#include "UI/ZooQuestTrackerPeek.h"
#include "WzUILibrary.h"

// ====== Lifecycle ====== //

void AZooHUD::BeginPlay()
{
	Super::BeginPlay();

	checkf(PathGuideWidgetClass && PathGuideWidgetControllerClass, TEXT("ZooHUD has no Path Guide widget or widget controller class set"));
	checkf(NarrativeWidgetClass && NarrativeWidgetControllerClass, TEXT("ZooHUD has no narrative widget or widget controller class set"));

	// Path Guide: widget and controller, wired together by the UI system.
	const auto [Widget, Controller] = UWzUILibrary::CreateWzWidget(PlayerOwner, PathGuideWidgetClass, PathGuideWidgetControllerClass, { PlayerOwner, PlayerOwner->PlayerState });
	PathGuideWidget = Widget;
	PathGuideWidgetController = Controller;
	PathGuideWidget->AddToViewport();

	// Narrative: the same way.
	const auto [NarrativeView, NarrativeController] = UWzUILibrary::CreateWzWidget(PlayerOwner, NarrativeWidgetClass, NarrativeWidgetControllerClass, { PlayerOwner, PlayerOwner->PlayerState });
	NarrativeWidget = NarrativeView;
	NarrativeWidgetController = NarrativeController;
	NarrativeWidget->AddToViewport();

	// The quest tracker peeks, then folds out of the way.
	TrackerPeek = NewObject<UZooQuestTrackerPeek>(this);
	TrackerPeek->Setup(NarrativeWidget, NarrativeWidgetController);
}

void AZooHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PathGuideWidget->RemoveFromParent();
	NarrativeWidget->RemoveFromParent();

	Super::EndPlay(EndPlayReason);
}
