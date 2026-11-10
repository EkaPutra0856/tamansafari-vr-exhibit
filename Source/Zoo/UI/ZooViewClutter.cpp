// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZooViewClutter.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"

FZooViewClutter::FZooViewClutterSource FZooViewClutter::Source;

// ====== Rect ====== //

bool FZooViewRect::Touches(const FVector2D& Centre, const float Radius) const
{
	return Centre.X + Radius > Min.X && Centre.X - Radius < Max.X && Centre.Y + Radius > Min.Y && Centre.Y - Radius < Max.Y;
}

// ====== Query ====== //

void FZooViewClutter::Query(const APlayerController* Controller, TArray<FZooViewRect>& OutRects)
{
	OutRects.Reset();
	if (!Controller) return;

	// Another layer draws the HUD (VR panels)
	if (Source.IsBound())
	{
		Source.Execute(Controller, OutRects);

		return;
	}

	// The screen: every overlay in the viewport, its elements' rects from viewport pixels to angles through the camera's FOV
	const FGeometry Viewport = UWidgetLayoutLibrary::GetViewportWidgetGeometry(Controller);
	const FVector2D Size = Viewport.GetLocalSize();
	if (Size.X <= 0.f || Size.Y <= 0.f) return;

	const float HalfFov = FMath::DegreesToRadians((Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetFOVAngle() : 90.f) * 0.5f);
	const float TanHalf = FMath::Tan(HalfFov);
	TArray<UUserWidget*> Overlays;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(const_cast<APlayerController*>(Controller), Overlays, UUserWidget::StaticClass(), true);
	for (const UUserWidget* Overlay : Overlays)
	{
		if (!Overlay->IsInViewport()) continue;

		TArray<const UUserWidget*> Elements;
		CollectElements(Overlay, Elements);
		for (const UUserWidget* Element : Elements)
		{
			const FGeometry& Geometry = Element->GetCachedGeometry();
			const FVector2D TopLeft = Viewport.AbsoluteToLocal(Geometry.GetAbsolutePosition());
			const FVector2D BottomRight = Viewport.AbsoluteToLocal(Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize());

			// Pixels to a direction on the image plane, then angles
			const auto Angles = [&](const FVector2D& Pixel)
			{
				return AnglesOf(FVector{ 1.f, (Pixel.X / Size.X * 2.f - 1.f) * TanHalf, -(Pixel.Y / Size.Y * 2.f - 1.f) * TanHalf * Size.Y / Size.X });
			};
			const FVector2D A = Angles(TopLeft);
			const FVector2D B = Angles(BottomRight);
			OutRects.Add({ { FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y) }, { FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y) } });
		}
	}
}

void FZooViewClutter::CollectElements(const UUserWidget* Overlay, TArray<const UUserWidget*>& OutElements)
{
	const UPanelWidget* Root = Overlay && Overlay->WidgetTree ? Cast<UPanelWidget>(Overlay->WidgetTree->RootWidget) : nullptr;
	if (!Root) return;

	// Elements: user widgets right under the overlay's root, visible and laid out
	for (int32 Index = 0; Index < Root->GetChildrenCount(); ++Index)
	{
		const UUserWidget* Element = Cast<UUserWidget>(Root->GetChildAt(Index));
		if (Element && Element->IsVisible() && Element->GetRenderOpacity() > 0.05f && !Element->GetCachedGeometry().GetAbsoluteSize().IsNearlyZero()) OutElements.Add(Element);
	}
}

// ====== Angles ====== //

FVector2D FZooViewClutter::AnglesOf(const FVector& ViewDirection)
{
	const FVector Direction = ViewDirection.GetSafeNormal();

	return { FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)), FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Direction.Z, -1.f, 1.f))) };
}

FVector FZooViewClutter::DirectionOf(const FVector2D& Angles)
{
	const float Yaw = FMath::DegreesToRadians(Angles.X);
	const float Pitch = FMath::DegreesToRadians(Angles.Y);

	return { FMath::Cos(Pitch) * FMath::Cos(Yaw), FMath::Cos(Pitch) * FMath::Sin(Yaw), FMath::Sin(Pitch) };
}
