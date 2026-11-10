// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/Components/PathGuideComponent.h"

#include "ExplorationSystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PathGuide/Data/PathGuideConfig.h"
#include "PathGuide/Data/PathGuidePlacesAsset.h"
#include "PathGuide/Routing/PathGuideGeometry.h"
#include "PathGuide/Subsystem/PathGuideSubsystem.h"
#include "PathGuide/World/PathGuideDestination.h"
#include "PathGuide/World/PathGuideRouteActor.h"

namespace
{
	// Re-routing (seconds)
	constexpr float OffRouteGraceTime = 0.5f;
	constexpr float RecalcCooldown = 1.f;

	// How often the places are checked (seconds)
	constexpr float PlaceScanInterval = 0.25f;

	// Route shape (cm)
	constexpr float DuplicateDistance = 5.f;
	constexpr float RouteSampleSpacing = 20.f;
	constexpr int32 ProjectionWindow = 60;
}

// ====== Lifecycle ====== //

UWzPathGuideComponent::UWzPathGuideComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UWzPathGuideComponent::BeginPlay()
{
	Super::BeginPlay();

	// Local player only
	const APlayerController* Owner = Cast<APlayerController>(GetOwner());
	if (!Owner || !Owner->IsLocalController()) return;

	Subsystem = GetWorld()->GetSubsystem<UWzPathGuideSubsystem>();
	const UWzPathGuideConfig* Config = Subsystem ? Subsystem->GetConfig() : nullptr;
	if (!Config) return;

	// Spawn the route View
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const TSubclassOf<AWzPathGuideRouteActor> ActorClass = Config->RouteActorClass ? Config->RouteActorClass : TSubclassOf<AWzPathGuideRouteActor>(AWzPathGuideRouteActor::StaticClass());
	RouteActor = GetWorld()->SpawnActor<AWzPathGuideRouteActor>(ActorClass, FTransform::Identity, SpawnParameters);

	// Report the places walked into, guided or not
	GetWorld()->GetTimerManager().SetTimer(PlaceScanTimer, this, &UWzPathGuideComponent::HandlePlaceScan, PlaceScanInterval, true);
}

void UWzPathGuideComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(PlaceScanTimer);
	if (RouteActor) RouteActor->Destroy();
	Super::EndPlay(EndPlayReason);
}

void UWzPathGuideComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWzPathGuideConfig* Config = Subsystem ? Subsystem->GetConfig() : nullptr;
	if (State != EPathGuideState::Guiding || Route.Num() < 2 || !Config) return;

	const FVector Traveler = GetTravelerLocation();

	// Arrival
	if (Subsystem->IsInArrivalZone(Traveler, Destination, ArrivalRadius))
	{
		Arrive();

		return;
	}

	// Project near the last position, else on the whole route
	auto Projection = PathGuide::Geometry::ProjectOntoPolyline(Traveler, Route, RouteDistances, HintSegment, ProjectionWindow);
	if (Projection.Lateral > Config->OffRouteRecalc) Projection = PathGuide::Geometry::ProjectOntoPolyline(Traveler, Route, RouteDistances, HintSegment, -1);

	HintSegment = Projection.SegmentIndex;

	// Off route: re-route after a grace time and cooldown
	if (Projection.Lateral > Config->OffRouteRecalc)
	{
		OffRouteTime += DeltaTime;
		const bool bCooledDown = GetWorld()->GetTimeSeconds() - LastRecalculateTime >= RecalcCooldown;
		if (OffRouteTime >= OffRouteGraceTime && bCooledDown && Recalculate())
		{
			OnProgress.Broadcast(GetRemainingDistance());

			return;
		}
	}
	else
	{
		OffRouteTime = 0.f;
	}

	// Publish progress
	Progress = Projection.DistanceAlong;
	OnProgress.Broadcast(GetRemainingDistance());
}

// ====== API ====== //

bool UWzPathGuideComponent::StartGuidance(FGameplayTag PlaceTag)
{
	// Resolve place and anchor
	const UWzPathGuidePlacesAsset* Places = Subsystem ? Subsystem->GetPlaces() : nullptr;
	const FPathGuidePlace* Place = Places ? Places->FindPlacePtr(PlaceTag) : nullptr;
	const AWzPathGuideDestination* Anchor = Subsystem ? Subsystem->FindDestination(PlaceTag) : nullptr;
	if (!Place || !Anchor)
	{
		UE_LOG(LogPathGuide, Warning, TEXT("StartGuidance: cannot resolve place '%s'."), *PlaceTag.ToString());
		Fail(PlaceTag);

		return false;
	}

	// Plan first: a failure keeps the running route
	if (!BuildRoute(GetTravelerLocation(), Anchor->GetActorLocation()))
	{
		UE_LOG(LogPathGuide, Warning, TEXT("StartGuidance: no route to '%s'."), *PlaceTag.ToString());
		Fail(PlaceTag);

		return false;
	}

	// Track the new destination
	ArrivalRadius = Place->ArrivalRadius;
	HintSegment = 0;
	OffRouteTime = 0.f;
	LastRecalculateTime = GetWorld()->GetTimeSeconds();

	SetState(EPathGuideState::Guiding, PlaceTag);
	OnRouteChanged.Broadcast();
	SetComponentTickEnabled(true);
	OnProgress.Broadcast(GetRemainingDistance());

	return true;
}

void UWzPathGuideComponent::StopGuidance()
{
	if (State == EPathGuideState::Idle) return;

	SetComponentTickEnabled(false);
	ClearRoute();
	SetState(EPathGuideState::Idle, FGameplayTag());
}

TArray<FPathGuidePlace> UWzPathGuideComponent::GetAvailablePlaces() const
{
	return Subsystem ? Subsystem->GetAvailablePlaces() : TArray<FPathGuidePlace>();
}

bool UWzPathGuideComponent::GetBeaconLocation(FVector& OutLocation) const
{
	if (State != EPathGuideState::Guiding) return false;
	OutLocation = Destination;

	return true;
}

void UWzPathGuideComponent::EstimateRouteLengths(const TArray<FGameplayTag>& PlaceTags, TArray<float>& OutLengths) const
{
	OutLengths.Init(-1.f, PlaceTags.Num());
	if (Subsystem) Subsystem->EstimateRouteLengths(GetTravelerLocation(), PlaceTags, OutLengths);
}

// ====== Helpers ====== //

FVector UWzPathGuideComponent::GetTravelerLocation() const
{
	const APlayerController* Owner = Cast<APlayerController>(GetOwner());
	if (const APawn* Pawn = Owner ? Owner->GetPawn() : nullptr) return Pawn->GetNavAgentLocation();

	return Owner ? Owner->GetFocalLocation() : FVector::ZeroVector;
}

void UWzPathGuideComponent::SetState(const EPathGuideState NewState, const FGameplayTag& PlaceTag)
{
	State = NewState;
	ActivePlaceTag = PlaceTag;
	OnStateChanged.Broadcast(State, ActivePlaceTag);
}

void UWzPathGuideComponent::Fail(const FGameplayTag& PlaceTag)
{
	// A running route stays untouched
	if (State == EPathGuideState::Guiding)
	{
		OnRouteFailed.Broadcast(PlaceTag);

		return;
	}

	// Report, then idle
	SetState(EPathGuideState::Failed, PlaceTag);
	OnRouteFailed.Broadcast(PlaceTag);
	SetState(EPathGuideState::Idle, FGameplayTag());
}

void UWzPathGuideComponent::Arrive()
{
	const FGameplayTag ArrivedTag = ActivePlaceTag;

	// Stop tracking
	SetComponentTickEnabled(false);
	ClearRoute();

	// Announce, then idle
	SetState(EPathGuideState::Arrived, ArrivedTag);
	OnArrived.Broadcast(ArrivedTag);
	SetState(EPathGuideState::Idle, FGameplayTag());
}

void UWzPathGuideComponent::ClearRoute()
{
	Route.Reset();
	RouteDistances.Reset();
	Progress = 0.f;
}

bool UWzPathGuideComponent::BuildRoute(const FVector& Start, const FVector& Goal)
{
	// Plan
	TArray<FVector> Plan;
	float Length = 0.f;
	if (!Subsystem || !Subsystem->PlanRoute(Start, Goal, Plan, Length)) return false;

	PathGuide::Geometry::RemoveDuplicates(Plan, DuplicateDistance);
	if (Plan.Num() < 2) return false;

	// Smooth, then space evenly
	TArray<FVector> Smooth;
	TArray<FVector> Fine;
	PathGuide::Geometry::CatmullRom(Plan, RouteSampleSpacing, Smooth);
	PathGuide::Geometry::Resample(Smooth, RouteSampleSpacing, Fine);
	if (Fine.Num() < 2) return false;

	// Commit
	Route = MoveTemp(Fine);
	PathGuide::Geometry::CumulativeDistances(Route, RouteDistances);
	Destination = Goal;
	Progress = 0.f;

	return true;
}

bool UWzPathGuideComponent::Recalculate()
{
	LastRecalculateTime = GetWorld()->GetTimeSeconds();
	OffRouteTime = 0.f;

	if (!BuildRoute(GetTravelerLocation(), Destination)) return false;

	HintSegment = 0;
	OnRouteChanged.Broadcast();

	return true;
}

// ====== Handlers ====== //

void UWzPathGuideComponent::HandlePlaceScan()
{
	// Where the traveller stands now
	TSet<FGameplayTag> Inside;
	Subsystem->FindPlacesAt(GetTravelerLocation(), Inside);

	// Report only the places just entered
	const TSet<FGameplayTag> Entered = Inside.Difference(PlacesInside);
	PlacesInside = MoveTemp(Inside);
	for (const auto& Place : Entered) OnPlaceReached.Broadcast(Place);
}