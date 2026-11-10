// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UUserWidget;

/** A screen element in view angles: yaw (right +) and pitch (up +) from the view direction, degrees. */
struct FZooViewRect
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;

	/** True when a disc (centre, radius in degrees) touches the rect. */
	bool Touches(const FVector2D& Centre, float Radius) const;
};

/**
 * Where the HUD sits in the visitor's view, so world things (Duma) can stay clear of it. Every visible sub-widget of a
 * screen overlay counts (dialogue, tracker, toast, destination list, Navigasi button, extended media). The VR layer, whose
 * panels float in the world, registers its own source.
 */
class ZOO_API FZooViewClutter
{
public:
	DECLARE_DELEGATE_TwoParams(FZooViewClutterSource, const APlayerController* /*Controller*/, TArray<FZooViewRect>& /*OutRects*/);

	/** Set by a layer that draws the HUD elsewhere than the screen (VR); unbound = the screen. */
	static FZooViewClutterSource Source;

	/** The HUD's visible elements in view angles. */
	static void Query(const APlayerController* Controller, TArray<FZooViewRect>& OutRects);

	/** The visible user widgets one level under the overlay's root (the HUD's elements), with their geometry. */
	static void CollectElements(const UUserWidget* Overlay, TArray<const UUserWidget*>& OutElements);

	/** Direction (unit, view space: X ahead, Y right, Z up) to view angles. */
	static FVector2D AnglesOf(const FVector& ViewDirection);

	/** View angles to a unit direction in view space. */
	static FVector DirectionOf(const FVector2D& Angles);
};
