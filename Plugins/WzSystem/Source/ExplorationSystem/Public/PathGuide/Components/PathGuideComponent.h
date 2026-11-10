// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "PathGuide/PathGuideTypes.h"
#include "PathGuideComponent.generated.h"

class AWzPathGuideRouteActor;
class UWzPathGuideSubsystem;

DECLARE_MULTICAST_DELEGATE_TwoParams(FPathGuideStateChanged, EPathGuideState /*NewState*/, const FGameplayTag& /*PlaceTag*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPathGuidePlaceEvent, const FGameplayTag& /*PlaceTag*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPathGuideProgress, float /*RemainingDistance*/);
DECLARE_MULTICAST_DELEGATE(FPathGuideRouteChanged);

/** MODEL of the Path Guide (on the PlayerController): state and route. */
UCLASS(ClassGroup = (PathGuide), meta = (BlueprintSpawnableComponent))
class EXPLORATIONSYSTEM_API UWzPathGuideComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UWzPathGuideComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// ====== Events ====== //

	FPathGuideStateChanged OnStateChanged;

	/** A new route is ready (first or re-route). */
	FPathGuideRouteChanged OnRouteChanged;

	FPathGuideProgress OnProgress;
	/** The guided destination was reached. */
	FPathGuidePlaceEvent OnArrived;

	/** The traveller walked into a place's arrival zone, guided or not. */
	FPathGuidePlaceEvent OnPlaceReached;

	FPathGuidePlaceEvent OnRouteFailed;

public:
	// ====== API ====== //

	/** Guide to the place (replaces any route). False when there is no route. */
	UFUNCTION(BlueprintCallable, Category = "PathGuide")
	bool StartGuidance(FGameplayTag PlaceTag);

	UFUNCTION(BlueprintCallable, Category = "PathGuide")
	void StopGuidance();

	/** Places of this level. */
	UFUNCTION(BlueprintCallable, Category = "PathGuide")
	TArray<FPathGuidePlace> GetAvailablePlaces() const;

	/** Active destination location. False when idle. */
	UFUNCTION(BlueprintCallable, Category = "PathGuide")
	bool GetBeaconLocation(FVector& OutLocation) const;

	/** Walking distance to each place (-1 = no route). */
	void EstimateRouteLengths(const TArray<FGameplayTag>& PlaceTags, TArray<float>& OutLengths) const;

private:
	// ====== Helpers ====== //

	FVector GetTravelerLocation() const;
	void SetState(EPathGuideState NewState, const FGameplayTag& PlaceTag);
	void Fail(const FGameplayTag& PlaceTag);
	void Arrive();
	void ClearRoute();

	/** Plans and smooths a route. Replaces the running one only on success. */
	bool BuildRoute(const FVector& Start, const FVector& Goal);

	/** Re-plans from here. True when the route was replaced. */
	bool Recalculate();

private:
	// ====== References ====== //

	UPROPERTY(Transient)
	TObjectPtr<UWzPathGuideSubsystem> Subsystem;

	UPROPERTY(Transient)
	TObjectPtr<AWzPathGuideRouteActor> RouteActor;

private:
	// ====== State ====== //

	EPathGuideState State = EPathGuideState::Idle;
	FGameplayTag ActivePlaceTag;
	FVector Destination = FVector::ZeroVector;
	float ArrivalRadius = 0.f;

	/** Smoothed centreline and the distance along it of each point. */
	TArray<FVector> Route;
	TArray<float> RouteDistances;

	/** Distance travelled along the route (cm). */
	float Progress = 0.f;

	int32 HintSegment = 0;
	float OffRouteTime = 0.f;
	double LastRecalculateTime = -1000.0;

	FTimerHandle PlaceScanTimer;

	/** The places whose arrival zone the traveller is in. */
	TSet<FGameplayTag> PlacesInside;

public:
	UFUNCTION(BlueprintPure, Category = "PathGuide")
	FORCEINLINE EPathGuideState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "PathGuide")
	FORCEINLINE FGameplayTag GetActivePlaceTag() const { return ActivePlaceTag; }

	/** Distance still to go. */
	UFUNCTION(BlueprintPure, Category = "PathGuide")
	FORCEINLINE float GetRemainingDistance() const { return RouteDistances.IsEmpty() ? 0.f : FMath::Max(RouteDistances.Last() - Progress, 0.f); }

	FORCEINLINE const TArray<FVector>& GetRoute() const { return Route; }
	FORCEINLINE float GetProgress() const { return Progress; }

	FORCEINLINE const TSet<FGameplayTag>& GetPlacesInside() const { return PlacesInside; }

private:
	// ====== Handlers ====== //

	/** Reports the places the traveller just walked into. */
	void HandlePlaceScan();
};