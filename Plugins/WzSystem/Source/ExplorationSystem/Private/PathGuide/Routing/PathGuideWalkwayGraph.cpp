// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/Routing/PathGuideWalkwayGraph.h"

#include "EngineUtils.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "PathGuide/Routing/PathGuideGeometry.h"
#include "PathGuide/World/PathGuideWalkway.h"

// ====== Helpers ====== //

namespace
{
	constexpr float MaxCost = TNumericLimits<float>::Max();

	void AppendUnique(TArray<FVector>& Out, const FVector& Point)
	{
		if (Out.IsEmpty() || !Out.Last().Equals(Point, 0.5)) Out.Add(Point);
	}

	/** Appends the part of the polyline between the two distances, in travel order (reversed when To < From). */
	void ExtractRange(const TArray<FVector>& Points, const TArray<float>& Cumulative, const float FromDist, const float ToDist, TArray<FVector>& Out)
	{
		// Start point, the samples in between (backwards when To < From), end point
		AppendUnique(Out, PathGuide::Geometry::PointAtDistance(Points, Cumulative, FromDist));
		if (ToDist >= FromDist)
		{
			for (int32 Index = 0; Index < Points.Num(); ++Index)
			{
				if (Cumulative[Index] > FromDist && Cumulative[Index] < ToDist) AppendUnique(Out, Points[Index]);
			}
		}
		else
		{
			for (int32 Index = Points.Num() - 1; Index >= 0; --Index)
			{
				if (Cumulative[Index] < FromDist && Cumulative[Index] > ToDist) AppendUnique(Out, Points[Index]);
			}
		}
		AppendUnique(Out, PathGuide::Geometry::PointAtDistance(Points, Cumulative, ToDist));
	}
}

// ====== Build ====== //

void FWzPathGuideWalkwayGraph::Build(const TArray<TArray<FVector>>& Walkways, const float InSnapTolerance)
{
	Reset();
	const float Tolerance = FMath::Max(InSnapTolerance, 1.f);

	for (int32 WalkwayIndex = 0; WalkwayIndex < Walkways.Num(); ++WalkwayIndex)
	{
		const TArray<FVector>& Poly = Walkways[WalkwayIndex];
		if (Poly.Num() < 2) continue;

		// Distance along this walkway at every sample
		TArray<float> Cumulative;
		PathGuide::Geometry::CumulativeDistances(Poly, Cumulative);
		const float Length = Cumulative.Last();

		// T-junctions: where another walkway ends on the side of this one, remember where to split
		TArray<float> Splits;
		for (int32 OtherIndex = 0; OtherIndex < Walkways.Num(); ++OtherIndex)
		{
			if (OtherIndex == WalkwayIndex || Walkways[OtherIndex].Num() < 2) continue;

			const FVector Ends[2] = { Walkways[OtherIndex][0], Walkways[OtherIndex].Last() };
			for (const auto& End : Ends)
			{
				const PathGuide::Geometry::FProjection Hit = PathGuide::Geometry::ClosestPointOnPolyline(End, Poly, Cumulative, false, 0, Poly.Num() - 2);
				if (Hit.Lateral <= Tolerance && Hit.DistanceAlong > Tolerance && Hit.DistanceAlong < Length - Tolerance) Splits.Add(Hit.DistanceAlong);
			}
		}
		Splits.Sort();

		// Merge splits closer than the tolerance, so one junction cuts the walkway once
		TArray<float> Cuts;
		for (const auto Split : Splits)
		{
			if (Cuts.IsEmpty() || Split - Cuts.Last() > Tolerance) Cuts.Add(Split);
		}
		Cuts.Add(Length);

		// Every piece between two cuts becomes an edge between its end junctions
		float Start = 0.f;
		for (const auto End : Cuts)
		{
			TArray<FVector> Piece;
			ExtractRange(Poly, Cumulative, Start, End, Piece);
			Start = End;
			if (Piece.Num() < 2) continue;

			const int32 NodeA = FindOrAddNode(Piece[0], Tolerance);
			const int32 NodeB = FindOrAddNode(Piece.Last(), Tolerance);
			AddEdge(NodeA, NodeB, Piece);
		}
	}

	// Edges touching each junction
	NodeEdges.SetNum(Nodes.Num());
	for (int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); ++EdgeIndex)
	{
		NodeEdges[Edges[EdgeIndex].NodeA].AddUnique(EdgeIndex);
		NodeEdges[Edges[EdgeIndex].NodeB].AddUnique(EdgeIndex);
	}
}

int32 FWzPathGuideWalkwayGraph::BuildFromWorld(const UWorld* World, const float InWalkwayTolerance, const float InSnapTolerance, FWzPathGuideWalkwayGraph& OutGraph)
{
	OutGraph.Reset();
	if (!World) return 0;

	// Flatten every walkway spline, then build the graph from the polylines
	TArray<TArray<FVector>> Polylines;
	for (TActorIterator<AWzPathGuideWalkway> It(World); It; ++It) It->Spline->ConvertSplineToPolyLine(ESplineCoordinateSpace::World, FMath::Square(InWalkwayTolerance), Polylines.AddDefaulted_GetRef());
	OutGraph.Build(Polylines, InSnapTolerance);

	return Polylines.Num();
}

void FWzPathGuideWalkwayGraph::Reset()
{
	Nodes.Reset();
	Edges.Reset();
	NodeEdges.Reset();
}

// ====== Queries ====== //

int32 FWzPathGuideWalkwayGraph::NumComponents() const
{
	TArray<int32> NodePiece;

	return FindPieces(NodePiece);
}

int32 FWzPathGuideWalkwayGraph::FindPieces(TArray<int32>& OutNodePiece) const
{
	// Union-find over the junctions: join the two ends of every edge
	TArray<int32> Parent;
	Parent.SetNumUninitialized(Nodes.Num());
	for (int32 Index = 0; Index < Parent.Num(); ++Index) Parent[Index] = Index;

	auto Find = [&Parent](int32 Node)
	{
		while (Parent[Node] != Node)
		{
			Parent[Node] = Parent[Parent[Node]];
			Node = Parent[Node];
		}

		return Node;
	};

	for (const auto& Edge : Edges) Parent[Find(Edge.NodeA)] = Find(Edge.NodeB);

	// One piece per root that has at least one edge
	int32 Count = 0;
	TMap<int32, int32> RootToPiece;
	OutNodePiece.Init(INDEX_NONE, Nodes.Num());
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		if (NodeEdges[Index].IsEmpty()) continue;

		const int32 Root = Find(Index);
		if (!RootToPiece.Contains(Root)) RootToPiece.Add(Root, Count++);
		OutNodePiece[Index] = RootToPiece[Root];
	}

	return Count;
}

void FWzPathGuideWalkwayGraph::FindClosest(const FVector& Location, const int32 MaxCount, const float MaxRadius, const float MinSeparation, TArray<FPathGuideEdgePoint>& OutPoints) const
{
	OutPoints.Reset();

	// Closest point on every segment of every edge within the radius
	TArray<FPathGuideEdgePoint> Candidates;
	for (int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); ++EdgeIndex)
	{
		const FEdge& Edge = Edges[EdgeIndex];
		for (int32 Segment = 0; Segment + 1 < Edge.Points.Num(); ++Segment)
		{
			const PathGuide::Geometry::FProjection Hit = PathGuide::Geometry::ClosestPointOnPolyline(Location, Edge.Points, Edge.Cumulative, false, Segment, Segment);
			if (Hit.Lateral > MaxRadius) continue;

			FPathGuideEdgePoint Candidate;
			Candidate.EdgeIndex = EdgeIndex;
			Candidate.Point = Hit.Point;
			Candidate.DistanceToQuery = Hit.Lateral;
			Candidate.DistanceAlongEdge = Hit.DistanceAlong;
			Candidates.Add(Candidate);
		}
	}

	Candidates.Sort([](const FPathGuideEdgePoint& A, const FPathGuideEdgePoint& B) { return A.DistanceToQuery < B.DistanceToQuery; });

	// Nearest first, skipping any closer than MinSeparation to one already taken
	const double SeparationSq = static_cast<double>(MinSeparation) * MinSeparation;
	for (const auto& Candidate : Candidates)
	{
		if (OutPoints.Num() >= MaxCount) break;

		bool bFarEnough = true;
		for (const auto& Chosen : OutPoints)
		{
			if (FVector::DistSquared(Chosen.Point, Candidate.Point) < SeparationSq)
			{
				bFarEnough = false;
				break;
			}
		}
		if (bFarEnough) OutPoints.Add(Candidate);
	}
}

void FWzPathGuideWalkwayGraph::FindPaths(const FPathGuideEdgePoint& From, const TArray<FPathGuideEdgePoint>& Targets, TArray<TArray<FVector>>& OutPolylines, TArray<float>& OutLengths) const
{
	OutPolylines.Reset();
	OutPolylines.SetNum(Targets.Num());
	OutLengths.Init(-1.f, Targets.Num());
	if (!Edges.IsValidIndex(From.EdgeIndex)) return;

	FSearch Search;
	RunSearch(From, Search);
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		float Length = 0.f;
		if (ResolvePath(Search, From, Targets[Index], OutPolylines[Index], Length)) OutLengths[Index] = Length;
	}
}

// ====== Helpers ====== //

int32 FWzPathGuideWalkwayGraph::FindOrAddNode(const FVector& Location, const float Tolerance)
{
	// Reuse a junction within the tolerance, else add one
	const double ToleranceSq = static_cast<double>(Tolerance) * Tolerance;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		if (FVector::DistSquared(Nodes[Index], Location) <= ToleranceSq) return Index;
	}

	return Nodes.Add(Location);
}

void FWzPathGuideWalkwayGraph::AddEdge(const int32 NodeA, const int32 NodeB, const TArray<FVector>& Points)
{
	FEdge Edge;
	Edge.NodeA = NodeA;
	Edge.NodeB = NodeB;
	Edge.Points = Points;

	// Pin the ends to the junctions, then measure
	Edge.Points[0] = Nodes[NodeA];
	Edge.Points.Last() = Nodes[NodeB];
	PathGuide::Geometry::CumulativeDistances(Edge.Points, Edge.Cumulative);
	Edge.Length = Edge.Cumulative.Last();

	// Edges shorter than 1 cm are dropped
	if (Edge.Length >= 1.f) Edges.Add(MoveTemp(Edge));
}

void FWzPathGuideWalkwayGraph::RunSearch(const FPathGuideEdgePoint& From, FSearch& Out) const
{
	const FEdge& FromEdge = Edges[From.EdgeIndex];
	const int32 NodeCount = Nodes.Num();
	Out.Cost.Init(MaxCost, NodeCount);
	Out.PrevNode.Init(INDEX_NONE, NodeCount);
	Out.PrevEdge.Init(INDEX_NONE, NodeCount);
	Out.SeedToDistance.Init(0.f, NodeCount);
	TArray<uint8> Closed;
	Closed.Init(0, NodeCount);

	// Start from both ends of the edge the point lies on
	auto Seed = [&Out](const int32 Node, const float SeedCost, const float ToDistance)
	{
		if (SeedCost >= Out.Cost[Node]) return;

		Out.Cost[Node] = SeedCost;
		Out.SeedToDistance[Node] = ToDistance;
	};
	Seed(FromEdge.NodeA, From.DistanceAlongEdge, 0.f);
	Seed(FromEdge.NodeB, FromEdge.Length - From.DistanceAlongEdge, FromEdge.Length);

	// Dijkstra over the junctions (linear scan: the network is small)
	while (true)
	{
		int32 Current = INDEX_NONE;
		float Lowest = MaxCost;
		for (int32 Node = 0; Node < NodeCount; ++Node)
		{
			if (!Closed[Node] && Out.Cost[Node] < Lowest)
			{
				Lowest = Out.Cost[Node];
				Current = Node;
			}
		}
		if (Current == INDEX_NONE) break;
		Closed[Current] = 1;

		for (const auto EdgeIndex : NodeEdges[Current])
		{
			const FEdge& Edge = Edges[EdgeIndex];
			const int32 Next = Edge.NodeA == Current ? Edge.NodeB : Edge.NodeA;
			const float NextCost = Out.Cost[Current] + Edge.Length;
			if (NextCost >= Out.Cost[Next]) continue;

			Out.Cost[Next] = NextCost;
			Out.PrevNode[Next] = Current;
			Out.PrevEdge[Next] = EdgeIndex;
		}
	}
}

bool FWzPathGuideWalkwayGraph::ResolvePath(const FSearch& Search, const FPathGuideEdgePoint& From, const FPathGuideEdgePoint& To, TArray<FVector>& OutPolyline, float& OutLength) const
{
	OutPolyline.Reset();
	OutLength = 0.f;
	if (!Edges.IsValidIndex(To.EdgeIndex)) return false;

	const FEdge& FromEdge = Edges[From.EdgeIndex];
	const FEdge& ToEdge = Edges[To.EdgeIndex];

	// Cheapest way to reach the target: along the shared edge, or out through one end of the target's edge
	float BestCost = MaxCost;
	bool bDirect = false;
	if (From.EdgeIndex == To.EdgeIndex)
	{
		BestCost = FMath::Abs(From.DistanceAlongEdge - To.DistanceAlongEdge);
		bDirect = true;
	}

	int32 EndNode = INDEX_NONE;
	float EndFromDistance = 0.f;
	auto TryEnd = [&](const int32 Node, const float TailCost, const float TailFromDistance)
	{
		if (Search.Cost[Node] >= MaxCost || Search.Cost[Node] + TailCost >= BestCost) return;

		BestCost = Search.Cost[Node] + TailCost;
		EndNode = Node;
		EndFromDistance = TailFromDistance;
		bDirect = false;
	};
	TryEnd(ToEdge.NodeA, To.DistanceAlongEdge, 0.f);
	TryEnd(ToEdge.NodeB, ToEdge.Length - To.DistanceAlongEdge, ToEdge.Length);

	if (BestCost >= MaxCost) return false;

	if (bDirect)
	{
		ExtractRange(FromEdge.Points, FromEdge.Cumulative, From.DistanceAlongEdge, To.DistanceAlongEdge, OutPolyline);
	}
	else
	{
		// Walk the predecessors back to the seed node (an edge is travelled forward when it was left from its NodeA)
		TArray<TPair<int32, bool>> Steps;
		int32 Node = EndNode;
		while (Search.PrevEdge[Node] != INDEX_NONE)
		{
			const int32 EdgeIndex = Search.PrevEdge[Node];
			Steps.Add(TPair<int32, bool>(EdgeIndex, Edges[EdgeIndex].NodeA == Search.PrevNode[Node]));
			Node = Search.PrevNode[Node];
		}

		// Start point -> seed node, each edge in travel order, last junction -> target point
		ExtractRange(FromEdge.Points, FromEdge.Cumulative, From.DistanceAlongEdge, Search.SeedToDistance[Node], OutPolyline);
		for (int32 Step = Steps.Num() - 1; Step >= 0; --Step)
		{
			const FEdge& Edge = Edges[Steps[Step].Key];
			if (Steps[Step].Value) ExtractRange(Edge.Points, Edge.Cumulative, 0.f, Edge.Length, OutPolyline);
			else ExtractRange(Edge.Points, Edge.Cumulative, Edge.Length, 0.f, OutPolyline);
		}
		ExtractRange(ToEdge.Points, ToEdge.Cumulative, EndFromDistance, To.DistanceAlongEdge, OutPolyline);
	}

	OutLength = BestCost;

	return true;
}