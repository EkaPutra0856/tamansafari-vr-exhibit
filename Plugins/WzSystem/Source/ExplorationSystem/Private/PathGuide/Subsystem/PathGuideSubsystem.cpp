// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/Subsystem/PathGuideSubsystem.h"

#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "ExplorationSystem.h"
#include "Engine/World.h"
#include "PathGuide/Data/PathGuideConfig.h"
#include "PathGuide/Data/PathGuidePlacesAsset.h"
#include "PathGuide/Data/PathGuideUIConfig.h"
#include "PathGuide/Routing/PathGuideGeometry.h"
#include "PathGuide/Settings/PathGuideSettings.h"
#include "PathGuide/World/PathGuideDestination.h"

namespace
{
	// Internals nobody tunes per level (cm)
	constexpr int32 JoinCandidates = 3;
	constexpr float JoinMinSeparation = 500.f;
	constexpr float OffPathCostMultiplier = 4.f;
	const FVector NavProjectExtent{ 50.0, 50.0, 300.0 };
}

// ====== Lifecycle ====== //

bool UWzPathGuideSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWzPathGuideSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	LoadData();
	GatherDestinations();
	BuildWalkwayGraph();
}

// ====== API ====== //

AWzPathGuideDestination* UWzPathGuideSubsystem::FindDestination(const FGameplayTag& PlaceTag) const
{
	const auto* Found = Destinations.Find(PlaceTag);

	return Found ? Found->Get() : nullptr;
}

TArray<FPathGuidePlace> UWzPathGuideSubsystem::GetAvailablePlaces() const
{
	TArray<FPathGuidePlace> Result;
	if (!Places) return Result;

	for (const auto& Place : Places->GetPlaces()) if (Destinations.Contains(Place.PlaceTag)) Result.Add(Place);

	return Result;
}

bool UWzPathGuideSubsystem::IsInArrivalZone(const FVector& Location, const FVector& Center, const float Radius) const
{
	return Config && FVector::Dist2D(Location, Center) <= Radius && FMath::Abs(Location.Z - Center.Z) <= Config->ArrivalMaxHeightDelta;
}

void UWzPathGuideSubsystem::FindPlacesAt(const FVector& Location, TSet<FGameplayTag>& OutPlaces) const
{
	OutPlaces.Reset();
	if (!Places || !Config) return;

	for (const auto& [Tag, Anchor] : Destinations)
	{
		const FPathGuidePlace* Place = Places->FindPlacePtr(Tag);
		if (!Place || !Anchor.IsValid()) continue;

		// The same zone as a guided arrival
		if (IsInArrivalZone(Location, Anchor->GetActorLocation(), Place->ArrivalRadius)) OutPlaces.Add(Tag);
	}
}

bool UWzPathGuideSubsystem::PlanRoute(const FVector& Start, const FVector& Goal, TArray<FVector>& OutPolyline, float& OutLength) const
{
	FStartContext Context;
	BuildStartContext(Start, Context);

	return PlanFromContext(Context, Goal, OutPolyline, OutLength);
}

void UWzPathGuideSubsystem::EstimateRouteLengths(const FVector& Start, const TArray<FGameplayTag>& PlaceTags, TArray<float>& OutLengths) const
{
	OutLengths.Init(-1.f, PlaceTags.Num());
	if (!Config) return;

	// Visitor side, shared by all places
	FStartContext Context;
	BuildStartContext(Start, Context);

	for (int32 Index = 0; Index < PlaceTags.Num(); ++Index)
	{
		TArray<FVector> Polyline;
		float Length = 0.f;
		const AWzPathGuideDestination* Anchor = FindDestination(PlaceTags[Index]);
		if (Anchor && PlanFromContext(Context, Anchor->GetActorLocation(), Polyline, Length)) OutLengths[Index] = Length;
	}
}

const UWzPathGuideUIConfig& UWzPathGuideSubsystem::GetUIConfig() const
{
	return UIConfig ? *UIConfig : *GetDefault<UWzPathGuideUIConfig>();
}

// ====== Helpers ====== //

void UWzPathGuideSubsystem::LoadData()
{
	const UWzPathGuideSettings* Settings = GetDefault<UWzPathGuideSettings>();

	Places = Settings->PlacesAsset.LoadSynchronous();
	Config = Settings->ConfigAsset.LoadSynchronous();
	UIConfig = Settings->UIConfigAsset.LoadSynchronous();

	if (!Places) UE_LOG(LogPathGuide, Warning, TEXT("No places asset. Set Project Settings > Plugins > Path Guide > Places Asset."));
	if (!Config) UE_LOG(LogPathGuide, Warning, TEXT("No config asset. Set Project Settings > Plugins > Path Guide > Config Asset."));
	if (!UIConfig)
	{
		UE_LOG(LogPathGuide, Warning, TEXT("No UI config asset; using the defaults. Set Project Settings > Plugins > Path Guide > UI Config Asset."));
		UIConfig = GetMutableDefault<UWzPathGuideUIConfig>();
	}
}

void UWzPathGuideSubsystem::GatherDestinations()
{
	Destinations.Reset();

	// Index anchors by tag
	for (TActorIterator<AWzPathGuideDestination> It(GetWorld()); It; ++It)
	{
		AWzPathGuideDestination* Anchor = *It;
		if (!Anchor->PlaceTag.IsValid())
		{
			UE_LOG(LogPathGuide, Warning, TEXT("Destination '%s' has no PlaceTag."), *Anchor->GetName());
			continue;
		}
		if (Destinations.Contains(Anchor->PlaceTag))
		{
			UE_LOG(LogPathGuide, Warning, TEXT("Duplicate destination for tag '%s' ('%s')."), *Anchor->PlaceTag.ToString(), *Anchor->GetName());
			continue;
		}
		Destinations.Add(Anchor->PlaceTag, Anchor);
	}

	// Anchors without a place entry
	for (const auto& Pair : Destinations) if (Places && !Places->FindPlacePtr(Pair.Key)) UE_LOG(LogPathGuide, Warning, TEXT("Destination '%s' has no entry in the places asset."), *Pair.Key.ToString());

	UE_LOG(LogPathGuide, Log, TEXT("%d destination anchor(s), %d available place(s)."), Destinations.Num(), GetAvailablePlaces().Num());
}

void UWzPathGuideSubsystem::BuildWalkwayGraph()
{
	// Build and report
	const int32 WalkwayCount = FWzPathGuideWalkwayGraph::BuildFromWorld(GetWorld(), PathGuide::Network::FlattenTolerance, PathGuide::Network::JunctionSnap, Graph);
	const int32 Pieces = Graph.NumComponents();
	UE_LOG(LogPathGuide, Log, TEXT("Walkway graph: %d walkway(s) -> %d node(s), %d edge(s), %d connected piece(s)."), WalkwayCount, Graph.NumNodes(), Graph.NumEdges(), Pieces);

	if (Pieces > 1) UE_LOG(LogPathGuide, Warning, TEXT("Walkway network is split into %d pieces; places on different pieces cannot be routed along walkways."), Pieces);
}

bool UWzPathGuideSubsystem::FindNavPath(const FVector& From, const FVector& To, TArray<FVector>& OutPoints, float& OutLength) const
{
	OutPoints.Reset();
	OutLength = 0.f;

	UWorld* World = GetWorld();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation || !Config) return false;

	// Snap both ends onto the NavMesh
	FNavLocation ProjectedFrom;
	FNavLocation ProjectedTo;
	if (!Navigation->ProjectPointToNavigation(From, ProjectedFrom, NavProjectExtent) || !Navigation->ProjectPointToNavigation(To, ProjectedTo, NavProjectExtent)) return false;

	// Partial paths don't count
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, ProjectedFrom.Location, ProjectedTo.Location, nullptr, Config->NavFilterClass);
	if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2) return false;

	OutPoints = Path->PathPoints;
	OutLength = Path->GetPathLength();

	return true;
}

float UWzPathGuideSubsystem::MakeLeg(const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const
{
	// Straight when close, else a NavMesh path (-1 = none)
	const float Straight = static_cast<float>(FVector::Dist(From, To));
	if (Straight < PathGuide::Network::JunctionSnap)
	{
		OutPoints = { From, To };

		return Straight;
	}

	float Length = 0.f;

	return FindNavPath(From, To, OutPoints, Length) ? Length : -1.f;
}

void UWzPathGuideSubsystem::BuildStartContext(const FVector& Start, FStartContext& OutContext) const
{
	OutContext = FStartContext();
	OutContext.Location = Start;
	if (!Config || Graph.IsEmpty()) return;

	Graph.FindClosest(Start, JoinCandidates, PathGuide::Network::JoinRadius, JoinMinSeparation, OutContext.Candidates);

	// Access leg to every candidate
	const int32 Count = OutContext.Candidates.Num();
	OutContext.AccessLegs.SetNum(Count);
	OutContext.AccessLengths.SetNum(Count);
	for (int32 Index = 0; Index < Count; ++Index) OutContext.AccessLengths[Index] = MakeLeg(Start, OutContext.Candidates[Index].Point, OutContext.AccessLegs[Index]);
}

bool UWzPathGuideSubsystem::PlanFromContext(const FStartContext& Context, const FVector& Goal, TArray<FVector>& OutPolyline, float& OutLength) const
{
	OutPolyline.Reset();
	OutLength = 0.f;
	if (!Config) return false;

	const FVector& Start = Context.Location;
	TArray<FVector> NavPoints;
	float NavLength = 0.f;

	// Short hop: direct NavMesh route (a farther goal can't pass, and the 2D distance is a true lower bound of the nav length)
	const bool bTriedDirect = Config->DirectRouteMax > 0.f && FVector::Dist2D(Start, Goal) <= Config->DirectRouteMax;
	if (bTriedDirect && FindNavPath(Start, Goal, NavPoints, NavLength) && NavLength <= Config->DirectRouteMax)
	{
		OutPolyline = MoveTemp(NavPoints);
		OutLength = NavLength;

		return true;
	}

	// Walkway route: access leg, network path, exit leg
	if (!Context.Candidates.IsEmpty())
	{
		TArray<FPathGuideEdgePoint> EndCandidates;
		Graph.FindClosest(Goal, JoinCandidates, PathGuide::Network::JoinRadius, JoinMinSeparation, EndCandidates);

		// Exit leg from every end candidate
		const int32 EndCount = EndCandidates.Num();
		TArray<TArray<FVector>> ExitLegs;
		TArray<float> ExitLengths;
		ExitLegs.SetNum(EndCount);
		ExitLengths.SetNum(EndCount);
		for (int32 Index = 0; Index < EndCount; ++Index) ExitLengths[Index] = MakeLeg(EndCandidates[Index].Point, Goal, ExitLegs[Index]);

		// Cheapest pair (off-path legs cost extra)
		float BestCost = TNumericLimits<float>::Max();
		int32 BestStart = INDEX_NONE;
		int32 BestEnd = INDEX_NONE;
		TArray<FVector> BestNetworkPath;
		for (int32 StartIndex = 0; StartIndex < Context.Candidates.Num(); ++StartIndex)
		{
			if (Context.AccessLengths[StartIndex] < 0.f) continue;

			// One search reaches all end candidates
			TArray<TArray<FVector>> Paths;
			TArray<float> Lengths;
			Graph.FindPaths(Context.Candidates[StartIndex], EndCandidates, Paths, Lengths);
			for (int32 EndIndex = 0; EndIndex < EndCount; ++EndIndex)
			{
				if (ExitLengths[EndIndex] < 0.f || Lengths[EndIndex] < 0.f) continue;

				const float Cost = (Context.AccessLengths[StartIndex] + ExitLengths[EndIndex]) * OffPathCostMultiplier + Lengths[EndIndex];
				if (Cost >= BestCost) continue;

				BestCost = Cost;
				BestStart = StartIndex;
				BestEnd = EndIndex;
				BestNetworkPath = MoveTemp(Paths[EndIndex]);
			}
		}

		if (BestStart != INDEX_NONE)
		{
			// Stitch the three legs
			OutPolyline = Context.AccessLegs[BestStart];
			OutPolyline.Append(BestNetworkPath);
			OutPolyline.Append(ExitLegs[BestEnd]);

			TArray<float> Cumulative;
			PathGuide::Geometry::CumulativeDistances(OutPolyline, Cumulative);
			OutLength = Cumulative.IsEmpty() ? 0.f : Cumulative.Last();

			return true;
		}
	}

	// Fallback: plain NavMesh route
	if (!bTriedDirect) FindNavPath(Start, Goal, NavPoints, NavLength);
	if (NavPoints.Num() < 2) return false;

	UE_LOG(LogPathGuide, Warning, TEXT("No walkway route found; using a direct NavMesh route."));
	OutPolyline = MoveTemp(NavPoints);
	OutLength = NavLength;

	return true;
}