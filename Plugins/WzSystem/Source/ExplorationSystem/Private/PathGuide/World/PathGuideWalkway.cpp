// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/World/PathGuideWalkway.h"

#include "Components/SplineComponent.h"

#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "ExplorationSystem.h"
#include "Engine/World.h"
#include "PathGuide/Data/PathGuideConfig.h"
#include "PathGuide/Routing/PathGuideGroundTrace.h"
#include "PathGuide/Routing/PathGuideRibbonBuilder.h"
#include "PathGuide/Routing/PathGuideWalkwayGraph.h"
#include "PathGuide/Settings/PathGuideSettings.h"
#include "PathGuide/World/PathGuideDestination.h"

namespace
{
	constexpr float DebugDuration = 15.f;
	constexpr float DebugLift = 20.f;
	constexpr float DebugLineThickness = 12.f;
	constexpr float DebugNodeRadius = 40.f;
	constexpr float BuriedTraceUp = 1000.f;
}
#endif

// ====== Lifecycle ====== //

AWzPathGuideWalkway::AWzPathGuideWalkway()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);
	Spline->SetHiddenInGame(true);
}

#if WITH_EDITOR

// ====== Editor Tools ====== //

void AWzPathGuideWalkway::SnapToGround()
{
	const UWzPathGuideConfig* Config = GetDefault<UWzPathGuideSettings>()->ConfigAsset.LoadSynchronous();
	if (!Config) Config = GetDefault<UWzPathGuideConfig>();

	// Ground rules of the ribbon
	PathGuide::Ribbon::FParams Params;
	Params.TraceUp = Config->TraceUp;
	Params.TraceDown = Config->TraceDown;
	const PathGuide::FGroundTracer Trace(GetWorld(), Config->GroundTraceChannel, this);

	Modify();
	Spline->Modify();

	// Drop each point onto the ground
	int32 Snapped = 0;
	const int32 PointCount = Spline->GetNumberOfSplinePoints();
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		FVector Location = Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
		// A buried point raises the reference in steps, so the overhead guard stays on and ground above it is still found
		FVector Hit = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		bool bHit = false;
		for (double Lift = 0.0; !bHit && Lift <= BuriedTraceUp; Lift += Params.OverheadReject) bHit = PathGuide::Ribbon::TraceGround(Trace, Location.X, Location.Y, Location.Z + Lift, Params, Hit, Normal);
		if (!bHit) continue;

		Location.Z = Hit.Z;
		Spline->SetLocationAtSplinePoint(Index, Location, ESplineCoordinateSpace::World, false);
		++Snapped;
	}

	Spline->UpdateSpline();
	MarkPackageDirty();
	UE_LOG(LogPathGuide, Log, TEXT("%s: snapped %d of %d spline point(s) to the ground."), *GetName(), Snapped, PointCount);
}

void AWzPathGuideWalkway::ValidateNetwork()
{
	const UWorld* World = GetWorld();
	FWzPathGuideWalkwayGraph Graph;
	const int32 WalkwayCount = FWzPathGuideWalkwayGraph::BuildFromWorld(World, PathGuide::Network::FlattenTolerance, PathGuide::Network::JunctionSnap, Graph);

	// Main network = piece with the most edges, the rest are islands
	TArray<int32> NodePiece;
	const int32 Pieces = Graph.FindPieces(NodePiece);
	TArray<int32> PieceEdges;
	PieceEdges.Init(0, Pieces);
	for (int32 Edge = 0; Edge < Graph.NumEdges(); ++Edge) ++PieceEdges[NodePiece[Graph.GetEdgeStartNode(Edge)]];

	int32 MainPiece = 0;
	for (int32 Piece = 1; Piece < Pieces; ++Piece) if (PieceEdges[Piece] > PieceEdges[MainPiece]) MainPiece = Piece;

	// Edges: green, islands red
	const FVector Lift{ 0.0, 0.0, DebugLift };
	for (int32 Edge = 0; Edge < Graph.NumEdges(); ++Edge)
	{
		const FColor Color = NodePiece[Graph.GetEdgeStartNode(Edge)] == MainPiece ? FColor::Green : FColor::Red;
		const TArray<FVector>& Points = Graph.GetEdgePoints(Edge);
		for (int32 Index = 1; Index < Points.Num(); ++Index) DrawDebugLine(World, Points[Index - 1] + Lift, Points[Index] + Lift, Color, false, DebugDuration, 0, DebugLineThickness);
	}

	// Nodes: dead ends orange, junctions yellow
	int32 DeadEnds = 0;
	int32 Junctions = 0;
	for (int32 Node = 0; Node < Graph.NumNodes(); ++Node)
	{
		const int32 Degree = Graph.GetNodeDegree(Node);
		if (Degree == 1) ++DeadEnds;
		else if (Degree >= 3) ++Junctions;
		else continue;

		DrawDebugSphere(World, Graph.GetNode(Node) + Lift, DebugNodeRadius, 8, Degree == 1 ? FColor::Orange : FColor::Yellow, false, DebugDuration, 0, DebugLineThickness * 0.5f);
	}

	// Far destinations: magenta
	int32 FarDestinations = 0;
	for (TActorIterator<AWzPathGuideDestination> It(World); It; ++It)
	{
		TArray<FPathGuideEdgePoint> Nearest;
		Graph.FindClosest(It->GetActorLocation(), 1, PathGuide::Network::JoinRadius, 0.f, Nearest);
		if (!Nearest.IsEmpty()) continue;

		++FarDestinations;
		DrawDebugSphere(World, It->GetActorLocation() + Lift, DebugNodeRadius * 2.f, 8, FColor::Magenta, false, DebugDuration, 0, DebugLineThickness);
		UE_LOG(LogPathGuide, Warning, TEXT("Destination '%s' is farther than %.0f cm from every walkway."), *It->GetName(), PathGuide::Network::JoinRadius);
	}

	UE_LOG(LogPathGuide, Log, TEXT("Walkway network: %d walkway(s), %d edge(s), %d junction(s), %d dead end(s), %d piece(s), %d far destination(s)."),
		WalkwayCount, Graph.NumEdges(), Junctions, DeadEnds, Pieces, FarDestinations);
	if (Pieces > 1) UE_LOG(LogPathGuide, Warning, TEXT("The network is split: %d island(s) drawn in red."), Pieces - 1);
}

#endif