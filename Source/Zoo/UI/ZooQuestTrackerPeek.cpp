// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZooQuestTrackerPeek.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UI/WzNarrativeWidgetController.h"

// ====== Lifecycle ====== //

UWorld* UZooQuestTrackerPeek::GetTickableGameObjectWorld() const
{
	return OverlayWidget ? OverlayWidget->GetWorld() : nullptr;
}

void UZooQuestTrackerPeek::Tick(const float DeltaTime)
{
	if (!Tracker || !HitArea || !PeekButton) return;

	// Folded: the overlay's own animations may show the tracker again; keep it folded
	if (!bExpanded)
	{
		if (Tracker->GetVisibility() != ESlateVisibility::Collapsed) Tracker->SetVisibility(ESlateVisibility::Collapsed);

		return;
	}

	// Open: the hit area covers the tracker; the pointer on it keeps it open
	if (HitSlot) HitSlot->SetSize(Tracker->GetCachedGeometry().GetLocalSize());
	HideTimer = HitArea->IsHovered() ? HideDelay : HideTimer - DeltaTime;
	if (HideTimer <= 0.f) Minimize();
}

// ====== API ====== //

void UZooQuestTrackerPeek::Setup(UUserWidget* Overlay, UWzNarrativeWidgetController* Controller, const FName TrackerName)
{
	OverlayWidget = Overlay;
	Tracker = Overlay && Overlay->WidgetTree ? Overlay->WidgetTree->FindWidget(TrackerName) : nullptr;
	UCanvasPanel* Canvas = Tracker ? Cast<UCanvasPanel>(Tracker->GetParent()) : nullptr;
	const UCanvasPanelSlot* TrackerSlot = Tracker ? Cast<UCanvasPanelSlot>(Tracker->Slot) : nullptr;
	if (!Canvas || !TrackerSlot)
	{
		Tracker = nullptr;

		return;
	}
	TrackerVisibility = Tracker->GetVisibility();

	// The folded button, in the tracker's corner: a small outlined pill
	UWidgetTree* Tree = Overlay->WidgetTree;
	PeekButton = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ZooTrackerPeek"));
	FSlateBrush Pill;
	Pill.DrawAs = ESlateBrushDrawType::RoundedBox;
	Pill.TintColor = FLinearColor{ 0.02f, 0.06f, 0.09f, 0.8f };
	Pill.OutlineSettings = FSlateBrushOutlineSettings{ FVector4{ 8.f, 8.f, 8.f, 8.f }, FSlateColor{ FLinearColor{ 0.35f, 0.8f, 1.f } }, 1.5f };
	Pill.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	FButtonStyle Style = PeekButton->GetStyle();
	Style.SetNormal(Pill);
	Pill.TintColor = FLinearColor{ 0.08f, 0.2f, 0.28f, 0.9f };
	Style.SetHovered(Pill);
	Pill.TintColor = FLinearColor{ 0.2f, 0.45f, 0.6f, 0.95f };
	Style.SetPressed(Pill);
	Style.SetNormalPadding(FMargin{ 16.f, 7.f });
	Style.SetPressedPadding(FMargin{ 16.f, 8.f, 16.f, 6.f });
	PeekButton->SetStyle(Style);
	UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ZooTrackerPeekLabel"));
	Label->SetText(NSLOCTEXT("Zoo", "QuestTrackerPeek", "MISI  ▸"));
	Label->SetColorAndOpacity(FSlateColor{ FLinearColor{ 0.85f, 0.95f, 1.f } });
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = 15;
	Font.LetterSpacing = 150;
	Label->SetFont(Font);
	PeekButton->AddChild(Label);
	PeekButton->OnClicked.AddDynamic(this, &UZooQuestTrackerPeek::HandlePeekClicked);
	UCanvasPanelSlot* PeekSlot = Canvas->AddChildToCanvas(PeekButton);
	PeekSlot->SetLayout(TrackerSlot->GetLayout());
	PeekSlot->SetAutoSize(true);

	// The invisible hit area over the open tracker
	HitArea = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ZooTrackerHitArea"));
	FSlateBrush Clear;
	Clear.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle ClearStyle;
	ClearStyle.SetNormal(Clear);
	ClearStyle.SetHovered(Clear);
	ClearStyle.SetPressed(Clear);
	ClearStyle.SetNormalPadding(FMargin{ 0.f });
	ClearStyle.SetPressedPadding(FMargin{ 0.f });
	HitArea->SetStyle(ClearStyle);
	HitSlot = Canvas->AddChildToCanvas(HitArea);
	HitSlot->SetLayout(TrackerSlot->GetLayout());
	HitSlot->SetAutoSize(false);
	HitSlot->SetSize({ 10.f, 10.f });

	// Open for the intro, again on every new objective
	if (Controller)
	{
		Controller->OnObjectiveChanged.AddDynamic(this, &UZooQuestTrackerPeek::HandleObjectiveChanged);
		Controller->OnQuestStarted.AddDynamic(this, &UZooQuestTrackerPeek::HandleQuestStarted);
	}
	Expand();
	HideTimer = IntroSeconds;
}

void UZooQuestTrackerPeek::Expand()
{
	if (!Tracker) return;

	bExpanded = true;
	HideTimer = HideDelay;
	Tracker->SetVisibility(TrackerVisibility);
	HitArea->SetVisibility(ESlateVisibility::Visible);
	PeekButton->SetVisibility(ESlateVisibility::Collapsed);
}

void UZooQuestTrackerPeek::Minimize()
{
	if (!Tracker) return;

	bExpanded = false;
	Tracker->SetVisibility(ESlateVisibility::Collapsed);
	HitArea->SetVisibility(ESlateVisibility::Collapsed);
	PeekButton->SetVisibility(ESlateVisibility::Visible);
}

// ====== Handlers ====== //

void UZooQuestTrackerPeek::HandlePeekClicked()
{
	Expand();
}

void UZooQuestTrackerPeek::HandleObjectiveChanged(const FWzQuestView& Quest)
{
	// A new objective: show it for the intro time
	Expand();
	HideTimer = IntroSeconds;
}

void UZooQuestTrackerPeek::HandleQuestStarted(const FWzQuestView& Quest)
{
	Expand();
	HideTimer = IntroSeconds;
}
