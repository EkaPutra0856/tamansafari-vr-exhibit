// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "PathGuide/PathGuideTypes.h"
#include "PathGuide/Routing/PathGuideWalkwayGraph.h"
#include "PathGuideSubsystem.generated.h"

class AWzPathGuideDestination;
class UWzPathGuideConfig;
class UWzPathGuidePlacesAsset;
class UWzPathGuideUIConfig;

/** Per-world Path Guide data and routing. */
UCLASS()
class EXPLORATIONSYSTEM_API UWzPathGuideSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

public:
	// ====== API ====== //

	/** The anchor placed for the place. Null when none. */
	AWzPathGuideDestination* FindDestination(const FGameplayTag& PlaceTag) const;

	/** Places with a destination in this level, in asset order. */
	TArray<FPathGuidePlace> GetAvailablePlaces() const;

	/** True when the location is within the radius of the center on the ground plane and within the config's height delta. */
	bool IsInArrivalZone(const FVector& Location, const FVector& Center, float Radius) const;

	/** The places whose arrival zone (radius and height) contains the location. */
	void FindPlacesAt(const FVector& Location, TSet<FGameplayTag>& OutPlaces) const;

	/** Walking route from Start to Goal. False when there is none. */
	bool PlanRoute(const FVector& Start, const FVector& Goal, TArray<FVector>& OutPolyline, float& OutLength) const;

	/** Walking distance to each place (-1 = no route). */
	void EstimateRouteLengths(const FVector& Start, const TArray<FGameplayTag>& PlaceTags, TArray<float>& OutLengths) const;

private:
	// ====== Types ====== //

	/** Planning work that only depends on where the visitor stands. */
	struct FStartContext
	{
		FVector Location = FVector::ZeroVector;
		TArray<FPathGuideEdgePoint> Candidates;
		TArray<TArray<FVector>> AccessLegs;
		TArray<float> AccessLengths;
	};

private:
	// ====== Helpers ====== //

	void LoadData();
	void GatherDestinations();
	void BuildWalkwayGraph();
	bool FindNavPath(const FVector& From, const FVector& To, TArray<FVector>& OutPoints, float& OutLength) const;
	float MakeLeg(const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const;
	void BuildStartContext(const FVector& Start, FStartContext& OutContext) const;
	bool PlanFromContext(const FStartContext& Context, const FVector& Goal, TArray<FVector>& OutPolyline, float& OutLength) const;

private:
	// ====== References ====== //

	UPROPERTY(Transient)
	TObjectPtr<UWzPathGuidePlacesAsset> Places;

	UPROPERTY(Transient)
	TObjectPtr<UWzPathGuideConfig> Config;

	UPROPERTY(Transient)
	TObjectPtr<UWzPathGuideUIConfig> UIConfig;

public:
	FORCEINLINE const UWzPathGuidePlacesAsset* GetPlaces() const { return Places; }
	FORCEINLINE const UWzPathGuideConfig* GetConfig() const { return Config; }

	/** Never null (falls back to the defaults). */
	const UWzPathGuideUIConfig& GetUIConfig() const;

private:
	// ====== State ====== //

	FWzPathGuideWalkwayGraph Graph;
	TMap<FGameplayTag, TWeakObjectPtr<AWzPathGuideDestination>> Destinations;
};