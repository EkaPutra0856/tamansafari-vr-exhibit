// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/UI/PathGuideWidgetController.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "PathGuide/Data/PathGuidePlacesAsset.h"
#include "PathGuide/Data/PathGuideUIConfig.h"
#include "GameFramework/PlayerController.h"
#include "PathGuide/Routing/PathGuideFormat.h"
#include "PathGuide/Subsystem/PathGuideSubsystem.h"

namespace
{
	/** Distance in whole metres (input in cm). The distance texts only change when this does. */
	int32 WholeMeters(const float Centimeters)
	{
		return FMath::RoundToInt(Centimeters / 100.f);
	}
}

// ====== Setup ====== //

void UWzPathGuideWidgetController::BindCallbacksToDependencies()
{
	// Model and subsystem
	PathGuide = PlayerController->FindComponentByClass<UWzPathGuideComponent>();
	checkf(PathGuide, TEXT("The PathGuide component must be on the PlayerController"));
	Subsystem = PlayerController->GetWorld()->GetSubsystem<UWzPathGuideSubsystem>();
	checkf(Subsystem, TEXT("The PathGuide subsystem must exist in the world"));

	// Guidance
	PathGuide->OnStateChanged.AddUObject(this, &UWzPathGuideWidgetController::HandleStateChanged);
	PathGuide->OnProgress.AddUObject(this, &UWzPathGuideWidgetController::HandleProgress);

	// Arrival and failure
	PathGuide->OnArrived.AddUObject(this, &UWzPathGuideWidgetController::HandleArrived);
	PathGuide->OnRouteFailed.AddWeakLambda(this, [this](const FGameplayTag&)
	{
		OnRouteFailed.Broadcast(GetUIConfig().RouteFailedText);
	});
}

void UWzPathGuideWidgetController::BroadcastInitialValues()
{
	GuidanceView = BuildGuidanceView();
	OnGuidanceChanged.Broadcast(GuidanceView);
}

// ====== API ====== //

void UWzPathGuideWidgetController::StopGuidance()
{
	if (PathGuide && PathGuide->GetState() == EPathGuideState::Guiding) PathGuide->StopGuidance();
}

// ====== Helpers ====== //

const UWzPathGuideUIConfig& UWzPathGuideWidgetController::GetUIConfig() const
{
	return Subsystem->GetUIConfig();
}

FText UWzPathGuideWidgetController::FindPlaceName(const FGameplayTag& PlaceTag) const
{
	const UWzPathGuidePlacesAsset* Places = Subsystem->GetPlaces();
	const FPathGuidePlace* Place = Places ? Places->FindPlacePtr(PlaceTag) : nullptr;

	return Place ? Place->DisplayName : FText::GetEmpty();
}

FPathGuideGuidanceView UWzPathGuideWidgetController::BuildGuidanceView() const
{
	FPathGuideGuidanceView View;
	if (!PathGuide) return View;

	View.State = PathGuide->GetState();
	View.PlaceTag = PathGuide->GetActivePlaceTag();
	View.bIsGuiding = View.State == EPathGuideState::Guiding;

	// Guiding: destination name and remaining distance
	if (View.bIsGuiding)
	{
		View.PlaceName = FindPlaceName(View.PlaceTag);
		View.DistanceText = PathGuide::Format::Distance(PathGuide->GetRemainingDistance(), GetUIConfig());
	}

	return View;
}

FPathGuideWaypointView UWzPathGuideWidgetController::BuildWaypointView() const
{
	FPathGuideWaypointView View;
	FVector Beacon;
	if (!PlayerController || !PathGuide || !PathGuide->GetBeaconLocation(Beacon)) return View;

	const UWzPathGuideUIConfig& UIConfig = GetUIConfig();

	// Texts come from the guidance view, rebuilt only when the shown distance changes
	View.Label = GuidanceView.PlaceName;
	View.DistanceText = GuidanceView.DistanceText;

	// Viewport size in pixels; the result is divided by the viewport scale at the end to get Slate units
	int32 SizeX = 0;
	int32 SizeY = 0;
	PlayerController->GetViewportSize(SizeX, SizeY);
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(PlayerController), UE_KINDA_SMALL_NUMBER);
	if (SizeX <= 0 || SizeY <= 0) return View;

	// Is the pin's anchor (above the beacon) inside the screen, minus the edge padding?
	const FVector Anchor = Beacon + FVector{ 0.0, 0.0, UIConfig.WaypointHeightOffset };
	const double Padding = UIConfig.ScreenEdgePadding;
	const FVector2D Size(SizeX, SizeY);
	const FVector2D Center = Size * 0.5;

	FVector2D Projected;
	const bool bProjected = PlayerController->ProjectWorldLocationToScreen(Anchor, Projected, false);
	const bool bInside = bProjected && Projected.X >= Padding && Projected.X <= Size.X - Padding && Projected.Y >= Padding && Projected.Y <= Size.Y - Padding;

	FVector2D ScreenPosition = Projected;
	if (!bInside)
	{
		// Direction to the destination in screen space, from the camera's view (works when it is behind us too)
		FVector CameraLocation;
		FRotator CameraRotation;
		PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
		const FVector Relative = CameraRotation.UnrotateVector(Anchor - CameraLocation);

		FVector2D Direction{ Relative.Y, -Relative.Z };
		if (Direction.SizeSquared() < UE_KINDA_SMALL_NUMBER) Direction = { 0.0, 1.0 };
		Direction = Direction.GetSafeNormal();

		// Slide the pin along that direction until it touches the padded screen border
		const double LimitX = FMath::Max(Center.X - Padding, 1.0);
		const double LimitY = FMath::Max(Center.Y - Padding, 1.0);
		const double ScaleX = FMath::Abs(Direction.X) > UE_KINDA_SMALL_NUMBER ? LimitX / FMath::Abs(Direction.X) : TNumericLimits<double>::Max();
		const double ScaleY = FMath::Abs(Direction.Y) > UE_KINDA_SMALL_NUMBER ? LimitY / FMath::Abs(Direction.Y) : TNumericLimits<double>::Max();
		ScreenPosition = Center + Direction * FMath::Min(ScaleX, ScaleY);
		View.EdgeAngleDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)));
	}

	View.bOnScreen = bInside;
	View.ScreenPosition = ScreenPosition / Scale;

	return View;
}

// ====== Handlers ====== //

void UWzPathGuideWidgetController::HandleStateChanged(const EPathGuideState NewState, const FGameplayTag& PlaceTag)
{
	// Guidance status
	GuidanceView = BuildGuidanceView();
	LastDistanceMeters = WholeMeters(PathGuide->GetRemainingDistance());
	OnGuidanceChanged.Broadcast(GuidanceView);
}

void UWzPathGuideWidgetController::HandleProgress(const float RemainingDistance)
{
	// Rebuild the distance texts only when the shown whole-metre value changes
	const int32 Meters = WholeMeters(RemainingDistance);
	if (Meters != LastDistanceMeters)
	{
		LastDistanceMeters = Meters;
		GuidanceView = BuildGuidanceView();
		OnGuidanceChanged.Broadcast(GuidanceView);
	}

	// The pin moves every call
	OnWaypointUpdated.Broadcast(BuildWaypointView());
}

void UWzPathGuideWidgetController::HandleArrived(const FGameplayTag& PlaceTag)
{
	const UWzPathGuideUIConfig& UIConfig = GetUIConfig();

	// "You have arrived at ..."
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Place"), FindPlaceName(PlaceTag));

	FPathGuideArrivalView View;
	View.Message = FText::Format(UIConfig.ArrivalFormat, Arguments);
	View.DisplaySeconds = UIConfig.ArrivalToastDuration;
	OnArrived.Broadcast(View);
}