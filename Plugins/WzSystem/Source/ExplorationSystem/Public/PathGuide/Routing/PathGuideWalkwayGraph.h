// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace PathGuide::Network
{
	/** Spline flattening tolerance (cm). */
	inline constexpr float FlattenTolerance = 5.f;

	/** Endpoints closer than this become one junction (cm). */
	inline constexpr float JunctionSnap = 150.f;

	/** Farthest walkway point a visitor can step onto (cm). */
	inline constexpr float JoinRadius = 6000.f;
}

/** A point on the walkway network: which edge, where along it, and how far the query location was from it. */
struct EXPLORATIONSYSTEM_API FPathGuideEdgePoint
{
	int32 EdgeIndex = INDEX_NONE;
	float DistanceAlongEdge = 0.f;
	FVector Point = FVector::ZeroVector;

	/** Straight distance from the query location to Point. */
	float DistanceToQuery = 0.f;
};

/** Walkway network as a graph of polyline edges between junctions. Plain C++ so it can be unit tested. Lengths in cm. */
class EXPLORATIONSYSTEM_API FWzPathGuideWalkwayGraph
{
public:
	// ====== Build ====== //

	/** Merges endpoints within InSnapTolerance and splits a walkway where another ends on its side (T-junction). */
	void Build(const TArray<TArray<FVector>>& Walkways, float InSnapTolerance);

	/** Builds OutGraph from every walkway spline in the world. Returns the walkway count. */
	static int32 BuildFromWorld(const UWorld* World, float InWalkwayTolerance, float InSnapTolerance, FWzPathGuideWalkwayGraph& OutGraph);

	void Reset();

	// ====== Queries ====== //

	/** Separate connected pieces (1 = everything reachable). */
	int32 NumComponents() const;

	/** Piece index of every node (INDEX_NONE for nodes without edges). Returns the piece count. */
	int32 FindPieces(TArray<int32>& OutNodePiece) const;

	/** Up to MaxCount network points near Location, nearest first, at least MinSeparation apart. */
	void FindClosest(const FVector& Location, int32 MaxCount, float MaxRadius, float MinSeparation, TArray<FPathGuideEdgePoint>& OutPoints) const;

	/** Shortest paths from one point to many targets in one search. OutLengths[i] is -1 when unreachable. */
	void FindPaths(const FPathGuideEdgePoint& From, const TArray<FPathGuideEdgePoint>& Targets, TArray<TArray<FVector>>& OutPolylines, TArray<float>& OutLengths) const;

private:
	// ====== Types ====== //

	struct FEdge
	{
		int32 NodeA = INDEX_NONE;
		int32 NodeB = INDEX_NONE;
		TArray<FVector> Points;
		TArray<float> Cumulative;
		float Length = 0.f;
	};

	/** Result of one Dijkstra run from a point on the network (per junction). */
	struct FSearch
	{
		TArray<float> Cost;
		TArray<int32> PrevNode;
		TArray<int32> PrevEdge;
		TArray<float> SeedToDistance;
	};

	// ====== Helpers ====== //

	int32 FindOrAddNode(const FVector& Location, float Tolerance);
	void AddEdge(int32 NodeA, int32 NodeB, const TArray<FVector>& Points);
	void RunSearch(const FPathGuideEdgePoint& From, FSearch& Out) const;
	bool ResolvePath(const FSearch& Search, const FPathGuideEdgePoint& From, const FPathGuideEdgePoint& To, TArray<FVector>& OutPolyline, float& OutLength) const;

	// ====== State ====== //

	TArray<FVector> Nodes;
	TArray<FEdge> Edges;
	TArray<TArray<int32>> NodeEdges;

public:
	FORCEINLINE bool IsEmpty() const { return Edges.IsEmpty(); }
	FORCEINLINE int32 NumNodes() const { return Nodes.Num(); }
	FORCEINLINE int32 NumEdges() const { return Edges.Num(); }
	FORCEINLINE const FVector& GetNode(const int32 Node) const { return Nodes[Node]; }
	FORCEINLINE int32 GetNodeDegree(const int32 Node) const { return NodeEdges[Node].Num(); }
	FORCEINLINE const TArray<FVector>& GetEdgePoints(const int32 Edge) const { return Edges[Edge].Points; }
	FORCEINLINE int32 GetEdgeStartNode(const int32 Edge) const { return Edges[Edge].NodeA; }
};
