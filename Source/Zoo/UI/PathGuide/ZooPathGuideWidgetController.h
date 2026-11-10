// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PathGuide/UI/PathGuideWidgetController.h"
#include "ZooPathGuideWidgetController.generated.h"

class AZooPlayerController;
class UTexture2D;
class UZooPlaceCardsAsset;

/** Everything a destination card needs. All texts are ready to display. */
USTRUCT(BlueprintType)
struct FZooPlaceCardView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	FGameplayTag PlaceTag;

	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	int32 Number = 0;

	/** Number as text for the badge ("3"). */
	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	FText NumberText;

	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	TObjectPtr<UTexture2D> Thumbnail;

	/** "180 m · 3 mnt" measured along the walkways from where the visitor is now. Empty when unreachable. */
	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	FText DistanceText;

	/** True for the place the visitor is currently being guided to. */
	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	bool bIsActiveDestination = false;

	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	bool bIsReachable = true;
};

/** State of the HUD "Navigasi" button. */
USTRUCT(BlueprintType)
struct FZooNavigateButtonView
{
	GENERATED_BODY()

	/** "Navigasi" while idle, "Menuju: Kandang Jerapah" while guiding. */
	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	FText ButtonText;

	/** Remaining distance while guiding; empty while idle. */
	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	FText DistanceText;

	/** Show the distance and the Stop control instead of the key hint. */
	UPROPERTY(BlueprintReadOnly, Category = "Zoo")
	bool bIsGuiding = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZooPlacesChangedSignature, const TArray<FZooPlaceCardView>&, Places);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZooMenuOpenChangedSignature, bool, bOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZooNavigateButtonChangedSignature, const FZooNavigateButtonView&, Button);

/** CONTROLLER of the Zoo's Path Guide UI: the "Mau ke mana?" destination list and the "Navigasi" button, on top of the plugin's guidance. */
UCLASS(Blueprintable)
class UZooPathGuideWidgetController : public UWzPathGuideWidgetController
{
	GENERATED_BODY()

public:
	// ====== Setup ====== //

	virtual void BindCallbacksToDependencies() override;
	virtual void BroadcastInitialValues() override;

public:
	// ====== Events ====== //

	/** The destination list changed (opened, or the active destination changed). */
	UPROPERTY(BlueprintAssignable, Category = "Zoo|Events")
	FZooPlacesChangedSignature OnPlacesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Zoo|Events")
	FZooMenuOpenChangedSignature OnMenuOpenChanged;

	/** The Navigasi button changed (idle / guiding, texts). */
	UPROPERTY(BlueprintAssignable, Category = "Zoo|Events")
	FZooNavigateButtonChangedSignature OnNavigateButtonChanged;

protected:
	// ====== Config ====== //

	/** Number, description and thumbnail of every place in the list. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo")
	TObjectPtr<UZooPlaceCardsAsset> PlaceCards;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo")
	FText NavigateButtonText;

	/** Named argument: {Place} */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo")
	FText GuidingButtonFormat;

private:
	// ====== References ====== //

	UPROPERTY(Transient)
	TObjectPtr<AZooPlayerController> ZooPlayer;

private:
	// ====== State ====== //

	TArray<FZooPlaceCardView> PlaceViews;

	/** Loaded once in BroadcastInitialValues, so opening the list never hits the disk. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> Thumbnails;

public:
	// ====== API ====== //

	/** Opens the destination list when closed, closes it when open. */
	UFUNCTION(BlueprintCallable, Category = "Zoo")
	void ToggleMenu();

	UFUNCTION(BlueprintCallable, Category = "Zoo")
	void CloseMenu();

	/** Closes the list and starts guiding to the place. */
	UFUNCTION(BlueprintCallable, Category = "Zoo")
	void SelectPlace(FGameplayTag PlaceTag);

private:
	// ====== Helpers ====== //

	void RefreshPlaceViews(bool bWithDistances);

private:
	// ====== Handlers ====== //

	/** Rebuilds the Navigasi button from the guidance status. */
	UFUNCTION()
	void HandleGuidanceChanged(const FPathGuideGuidanceView& Guidance);

	/** Moves the active-card highlight; the cards keep their distances. */
	void HandleActivePlaceChanged(EPathGuideState NewState, const FGameplayTag& PlaceTag);

	/** Refreshes the cards with distances on open. */
	void HandleMenuOpenChanged(bool bOpen);
};