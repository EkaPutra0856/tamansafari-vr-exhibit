// Copyright Epic Games, Inc. All Rights Reserved.

#include "VR/ZooVRPanelHost.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelSlot.h"
#include "UObject/Stack.h"

// ====== Root ====== //

void UZooVRPanelRoot::OnSlotRemoved(UPanelSlot* InSlot)
{
	Super::OnSlotRemoved(InSlot);

	// Who closed the hosted widget (the Blueprint function, when a script did)
	UE_LOG(LogTemp, Log, TEXT("ZooVR panel: %s removed from its host by %s"), *GetNameSafe(InSlot ? InSlot->Content : nullptr), *FFrame::GetScriptCallstack(true).TrimStartAndEnd());
}

// ====== Lifecycle ====== //

bool UZooVRPanelHost::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// Overlay root, built once
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UZooVRPanelRoot>(UZooVRPanelRoot::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
	}

	return bSuperInitialized;
}

// ====== API ====== //

void UZooVRPanelHost::Host(UUserWidget* Widget)
{
	if (!Root || !Widget) return;

	// Fill, so the screen layout keeps its anchors
	UOverlaySlot* FillSlot = Root->AddChildToOverlay(Widget);
	FillSlot->SetHorizontalAlignment(HAlign_Fill);
	FillSlot->SetVerticalAlignment(VAlign_Fill);
	Hosted = Widget;
}

bool UZooVRPanelHost::IsEmpty() const
{
	return !GetHostedWidget();
}

UUserWidget* UZooVRPanelHost::GetHostedWidget() const
{
	return Root && Hosted && Hosted->GetParent() == Root.Get() ? Hosted.Get() : nullptr;
}

void UZooVRPanelHost::SetHighlight(const UWidget* Target, const FLinearColor& Color, const float Alpha)
{
	if (!Root) return;

	// Hidden: nothing hovered, or faded out
	const bool bShow = Target && Alpha > KINDA_SMALL_NUMBER;
	if (!bShow)
	{
		if (GlowLine) GlowLine->SetVisibility(ESlateVisibility::Collapsed);
		if (GlowHalo) GlowHalo->SetVisibility(ESlateVisibility::Collapsed);

		return;
	}

	// Built on first use, above the hosted widget
	if (!GlowHalo) GlowHalo = MakeGlow(10.f, 18.f);
	if (!GlowLine) GlowLine = MakeGlow(2.f, 12.f);

	// The button's rectangle in this host's space (it already carries the hover scale)
	const FGeometry& HostGeometry = GetCachedGeometry();
	const FGeometry& TargetGeometry = Target->GetCachedGeometry();
	const FVector2D TopLeft = HostGeometry.AbsoluteToLocal(TargetGeometry.GetAbsolutePosition());
	const FVector2D BottomRight = HostGeometry.AbsoluteToLocal(TargetGeometry.GetAbsolutePosition() + TargetGeometry.GetAbsoluteSize());

	PlaceGlow(GlowHalo, TopLeft, BottomRight - TopLeft, 8.f, Color.CopyWithNewOpacity(0.22f * Alpha));
	PlaceGlow(GlowLine, TopLeft, BottomRight - TopLeft, 2.f, Color.CopyWithNewOpacity(0.85f * Alpha));
}

// ====== Helpers ====== //

UImage* UZooVRPanelHost::MakeGlow(const float Width, const float CornerRadius)
{
	// Outline only: a rounded box with a transparent fill
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FLinearColor::Transparent;
	Brush.OutlineSettings = FSlateBrushOutlineSettings{ FVector4{ CornerRadius, CornerRadius, CornerRadius, CornerRadius }, FSlateColor{ FLinearColor::White }, Width };
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;

	UImage* Glow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Glow->SetBrush(Brush);
	Glow->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* GlowSlot = Root->AddChildToOverlay(Glow);
	GlowSlot->SetHorizontalAlignment(HAlign_Left);
	GlowSlot->SetVerticalAlignment(VAlign_Top);

	return Glow;
}

void UZooVRPanelHost::PlaceGlow(UImage* Glow, const FVector2D& Position, const FVector2D& Size, const float Inflate, const FLinearColor& Color) const
{
	// Around the button, a little outside its edge; never in the way of the laser
	Glow->SetDesiredSizeOverride(Size + FVector2D{ 2.f * Inflate });
	Glow->SetColorAndOpacity(Color);
	Glow->SetVisibility(ESlateVisibility::HitTestInvisible);
	Cast<UOverlaySlot>(Glow->Slot)->SetPadding(FMargin{ Position.X - Inflate, Position.Y - Inflate, 0.f, 0.f });
}
