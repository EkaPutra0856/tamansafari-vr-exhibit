// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Overlay.h"
#include "ZooVRPanelHost.generated.h"

class UImage;
class UOverlay;
class UWidget;

/** The host's overlay: logs who removes the hosted widget (diagnostics for widgets that close themselves). */
UCLASS()
class UZooVRPanelRoot : public UOverlay
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;
};

/**
 * Root of a VR panel: holds one screen widget moved off the viewport. Because the widget sits in a real parent slot,
 * the game's own RemoveFromParent (e.g. closing the extended media) really removes it, and the panel can tell.
 * Also draws the laser hover glow over a button of the hosted widget.
 */
UCLASS()
class UZooVRPanelHost : public UUserWidget
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual bool Initialize() override;

	// ====== API ====== //

	/** Puts the widget in the host, filling it. */
	void Host(UUserWidget* Widget);

	/** True once the hosted widget was removed (closed) by its owner. */
	bool IsEmpty() const;

	/** The hosted widget (null once closed). */
	UUserWidget* GetHostedWidget() const;

	/** Thin glow outline over Target (a button under the laser), faded by Alpha; a null Target or zero Alpha hides it. */
	void SetHighlight(const UWidget* Target, const FLinearColor& Color, float Alpha);

	// ====== Helpers ====== //

private:
	UImage* MakeGlow(float Width, float CornerRadius);
	void PlaceGlow(UImage* Glow, const FVector2D& Position, const FVector2D& Size, float Inflate, const FLinearColor& Color) const;

	// ====== References ====== //

	UPROPERTY(Transient)
	TObjectPtr<UZooVRPanelRoot> Root;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Hosted;

	/** Crisp inner line and a wide faint halo, together a thin glow. */
	UPROPERTY(Transient)
	TObjectPtr<UImage> GlowLine;

	UPROPERTY(Transient)
	TObjectPtr<UImage> GlowHalo;
};
