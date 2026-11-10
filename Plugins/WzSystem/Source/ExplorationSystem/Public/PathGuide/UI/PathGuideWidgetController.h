// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PathGuide/Components/PathGuideComponent.h"
#include "PathGuide/UI/PathGuideViewTypes.h"
#include "WidgetController.h"
#include "PathGuideWidgetController.generated.h"

class UWzPathGuideSubsystem;
class UWzPathGuideUIConfig;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPathGuideGuidanceChangedSignature, const FPathGuideGuidanceView&, Guidance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPathGuideWaypointUpdatedSignature, const FPathGuideWaypointView&, Waypoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPathGuideArrivedSignature, const FPathGuideArrivalView&, Arrival);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPathGuideRouteFailedSignature, const FText&, Message);

/** CONTROLLER of the Path Guide MVC: turns guidance events into view structs (status, waypoint pin, arrival). The game adds its own pickers and buttons in a subclass. */
UCLASS(BlueprintType, Blueprintable)
class EXPLORATIONSYSTEM_API UWzPathGuideWidgetController : public UWzWidgetController
{
	GENERATED_BODY()

public:
	// ====== Setup ====== //

	virtual void BindCallbacksToDependencies() override;
	virtual void BroadcastInitialValues() override;

public:
	// ====== Events ====== //

	/** Guidance status changed (idle / guiding, destination name, remaining distance). */
	UPROPERTY(BlueprintAssignable, Category = "PathGuide|Events")
	FPathGuideGuidanceChangedSignature OnGuidanceChanged;

	/** Every frame while guiding: where the destination pin belongs on screen. */
	UPROPERTY(BlueprintAssignable, Category = "PathGuide|Events")
	FPathGuideWaypointUpdatedSignature OnWaypointUpdated;

	UPROPERTY(BlueprintAssignable, Category = "PathGuide|Events")
	FPathGuideArrivedSignature OnArrived;

	UPROPERTY(BlueprintAssignable, Category = "PathGuide|Events")
	FPathGuideRouteFailedSignature OnRouteFailed;

protected:
	// ====== References ====== //

	/** The Model; a game subclass reads guidance from it too. */
	UPROPERTY(Transient)
	TObjectPtr<UWzPathGuideComponent> PathGuide;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWzPathGuideSubsystem> Subsystem;

private:
	// ====== State ====== //

	FPathGuideGuidanceView GuidanceView;
	int32 LastDistanceMeters = INDEX_NONE;

public:
	// ====== API ====== //

	UFUNCTION(BlueprintCallable, Category = "PathGuide")
	void StopGuidance();

protected:
	// ====== Helpers ====== //

	const UWzPathGuideUIConfig& GetUIConfig() const;

private:
	FText FindPlaceName(const FGameplayTag& PlaceTag) const;
	FPathGuideGuidanceView BuildGuidanceView() const;
	FPathGuideWaypointView BuildWaypointView() const;

private:
	// ====== Handlers ====== //

	/** Rebuilds the guidance status. */
	void HandleStateChanged(EPathGuideState NewState, const FGameplayTag& PlaceTag);

	/** Refreshes the distance texts when the shown whole-metre value changes, and the pin every call. */
	void HandleProgress(float RemainingDistance);

	/** Shows the arrival toast. */
	void HandleArrived(const FGameplayTag& PlaceTag);
};