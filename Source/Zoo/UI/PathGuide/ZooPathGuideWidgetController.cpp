// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PathGuide/ZooPathGuideWidgetController.h"

#include "Algo/StableSort.h"
#include "Engine/Texture2D.h"
#include "PathGuide/Routing/PathGuideFormat.h"
#include "Player/ZooPlayerController.h"
#include "UI/PathGuide/ZooPlaceCardsAsset.h"

// ====== Setup ====== //

void UZooPathGuideWidgetController::BindCallbacksToDependencies()
{
	Super::BindCallbacksToDependencies();

	ZooPlayer = CastChecked<AZooPlayerController>(PlayerController);

	// Navigasi button
	OnGuidanceChanged.AddDynamic(this, &UZooPathGuideWidgetController::HandleGuidanceChanged);

	// Active card
	PathGuide->OnStateChanged.AddUObject(this, &UZooPathGuideWidgetController::HandleActivePlaceChanged);

	// Menu: measure distances on every open
	ZooPlayer->OnDestinationMenuChanged.AddUObject(this, &UZooPathGuideWidgetController::HandleMenuOpenChanged);
}

void UZooPathGuideWidgetController::BroadcastInitialValues()
{
	checkf(PlaceCards, TEXT("The Zoo Path Guide widget controller has no PlaceCards asset set"));

	// Thumbnails, loaded once
	for (const auto& Card : PlaceCards->Cards) Thumbnails.Add(Card.PlaceTag, Card.Thumbnail.LoadSynchronous());

	// The plugin broadcasts the guidance, which builds the button
	Super::BroadcastInitialValues();

	// Initial cards
	RefreshPlaceViews(false);

	OnPlacesChanged.Broadcast(PlaceViews);
	OnMenuOpenChanged.Broadcast(ZooPlayer->IsDestinationMenuOpen());
}

// ====== API ====== //

void UZooPathGuideWidgetController::ToggleMenu()
{
	ZooPlayer->SetDestinationMenuOpen(!ZooPlayer->IsDestinationMenuOpen());
}

void UZooPathGuideWidgetController::CloseMenu()
{
	ZooPlayer->SetDestinationMenuOpen(false);
}

void UZooPathGuideWidgetController::SelectPlace(const FGameplayTag PlaceTag)
{
	ZooPlayer->SetDestinationMenuOpen(false);
	PathGuide->StartGuidance(PlaceTag);
}

// ====== Helpers ====== //

void UZooPathGuideWidgetController::RefreshPlaceViews(const bool bWithDistances)
{
	PlaceViews.Reset();

	const bool bGuiding = PathGuide->GetState() == EPathGuideState::Guiding;
	const TArray<FPathGuidePlace> Places = PathGuide->GetAvailablePlaces();

	// Measure every place in one go: the routes share the visitor-side work
	TArray<float> Lengths;
	if (bWithDistances)
	{
		TArray<FGameplayTag> Tags;
		for (const auto& Place : Places) Tags.Add(Place.PlaceTag);
		PathGuide->EstimateRouteLengths(Tags, Lengths);
	}

	// One card per place; a place without a card shows only its name
	for (int32 Index = 0; Index < Places.Num(); ++Index)
	{
		const FPathGuidePlace& Place = Places[Index];
		FZooPlaceCardView& View = PlaceViews.AddDefaulted_GetRef();
		View.PlaceTag = Place.PlaceTag;
		View.DisplayName = Place.DisplayName;
		View.bIsActiveDestination = bGuiding && PathGuide->GetActivePlaceTag() == Place.PlaceTag;

		if (const FZooPlaceCard* Card = PlaceCards->FindCard(Place.PlaceTag))
		{
			View.Number = Card->Number;
			View.NumberText = FText::AsNumber(Card->Number);
			View.Description = Card->Description;
			View.Thumbnail = Thumbnails.FindRef(Place.PlaceTag);
		}

		// Distance and ETA, or "unreachable" when there is no route
		if (!bWithDistances) continue;

		View.bIsReachable = Lengths[Index] >= 0.f;
		if (View.bIsReachable) View.DistanceText = PathGuide::Format::DistanceAndEta(Lengths[Index], GetUIConfig());
	}

	// List order is the badge number
	Algo::StableSortBy(PlaceViews, &FZooPlaceCardView::Number);
}

// ====== Handlers ====== //

void UZooPathGuideWidgetController::HandleGuidanceChanged(const FPathGuideGuidanceView& Guidance)
{
	// Texts straight from the guidance
	FZooNavigateButtonView View;
	View.DistanceText = Guidance.DistanceText;
	View.bIsGuiding = Guidance.bIsGuiding;

	// Guiding: "Menuju: {Place}". Idle: the plain Navigasi button
	if (Guidance.bIsGuiding)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Place"), Guidance.PlaceName);
		View.ButtonText = FText::Format(GuidingButtonFormat, Arguments);
	}
	else View.ButtonText = NavigateButtonText;

	OnNavigateButtonChanged.Broadcast(View);
}

void UZooPathGuideWidgetController::HandleActivePlaceChanged(const EPathGuideState NewState, const FGameplayTag& PlaceTag)
{
	for (auto& View : PlaceViews) View.bIsActiveDestination = NewState == EPathGuideState::Guiding && View.PlaceTag == PlaceTag;
	OnPlacesChanged.Broadcast(PlaceViews);
}

void UZooPathGuideWidgetController::HandleMenuOpenChanged(const bool bOpen)
{
	// Measure distances on every open
	if (bOpen)
	{
		RefreshPlaceViews(true);
		OnPlacesChanged.Broadcast(PlaceViews);
	}

	OnMenuOpenChanged.Broadcast(bOpen);
}