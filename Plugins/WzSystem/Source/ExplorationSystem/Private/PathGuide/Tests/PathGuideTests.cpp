// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PathGuide/Data/PathGuideUIConfig.h"
#include "PathGuide/Routing/PathGuideFormat.h"
#include "PathGuide/Routing/PathGuideGeometry.h"
#include "PathGuide/Routing/PathGuideRibbonBuilder.h"
#include "PathGuide/Routing/PathGuideWalkwayGraph.h"
#include "UObject/Package.h"

// ====== Helpers ====== //

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Shortest walkway path between two points (the production API searches many targets at once). False when not connected. */
	bool FindPath(const FWzPathGuideWalkwayGraph& Graph, const FPathGuideEdgePoint& From, const FPathGuideEdgePoint& To, TArray<FVector>& OutPolyline, float& OutLength)
	{
		TArray<TArray<FVector>> Polylines;
		TArray<float> Lengths;
		Graph.FindPaths(From, { To }, Polylines, Lengths);
		OutPolyline = MoveTemp(Polylines[0]);
		OutLength = FMath::Max(Lengths[0], 0.f);

		return Lengths[0] >= 0.f;
	}

	/** Straight centreline along +X at z = 0, 41 samples 25 cm apart. */
	TArray<FVector> MakeCenterline()
	{
		TArray<FVector> Centerline;
		for (int32 Index = 0; Index <= 40; ++Index) Centerline.Add({ Index * 25.0, 0.0, 0.0 });

		return Centerline;
	}
}

// ====== Graph ====== //

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathGuideGraphJunctionTest, "PathGuide.Graph.Junctions", TestFlags)
bool FPathGuideGraphJunctionTest::RunTest(const FString&)
{
	// A straight walkway with a second one ending on its side: a T-junction at x = 500.
	TArray<TArray<FVector>> Walkways;
	Walkways.Add({ FVector(0, 0, 0), FVector(1000, 0, 0) });
	Walkways.Add({ FVector(500, 40, 0), FVector(500, 800, 0) });

	FWzPathGuideWalkwayGraph Graph;
	Graph.Build(Walkways, 150.f);

	TestEqual(TEXT("edges (walkway A split in two + walkway B)"), Graph.NumEdges(), 3);
	TestEqual(TEXT("nodes"), Graph.NumNodes(), 4);
	TestEqual(TEXT("connected"), Graph.NumComponents(), 1);

	TArray<FPathGuideEdgePoint> Closest;
	Graph.FindClosest(FVector(100, -300, 0), 1, 5000.f, 500.f, Closest);
	TestEqual(TEXT("one candidate"), Closest.Num(), 1);
	if (Closest.Num() == 1)
	{
		TestNearlyEqual(TEXT("nearest point x"), static_cast<float>(Closest[0].Point.X), 100.f, 1.f);
		TestNearlyEqual(TEXT("nearest distance"), Closest[0].DistanceToQuery, 300.f, 1.f);
	}

	// From A at x=100 to B at y=700: 400 along A + 700 up B (node snapping may shift by less than the tolerance).
	TArray<FPathGuideEdgePoint> FromPoints;
	TArray<FPathGuideEdgePoint> ToPoints;
	Graph.FindClosest(FVector(100, 0, 0), 1, 100.f, 0.f, FromPoints);
	Graph.FindClosest(FVector(500, 700, 0), 1, 100.f, 0.f, ToPoints);
	if (TestTrue(TEXT("found endpoints"), FromPoints.Num() == 1 && ToPoints.Num() == 1))
	{
		TArray<FVector> Path;
		float Length = 0.f;
		TestTrue(TEXT("path exists"), FindPath(Graph, FromPoints[0], ToPoints[0], Path, Length));
		TestNearlyEqual(TEXT("path length"), Length, 1100.f, 60.f);
		TestTrue(TEXT("path has points"), Path.Num() >= 3);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathGuideGraphSameEdgeTest, "PathGuide.Graph.SameEdge", TestFlags)
bool FPathGuideGraphSameEdgeTest::RunTest(const FString&)
{
	TArray<TArray<FVector>> Walkways;
	Walkways.Add({ FVector(0, 0, 0), FVector(500, 0, 0), FVector(1000, 0, 0) });

	FWzPathGuideWalkwayGraph Graph;
	Graph.Build(Walkways, 150.f);

	TArray<FPathGuideEdgePoint> A;
	TArray<FPathGuideEdgePoint> B;
	Graph.FindClosest(FVector(100, 30, 0), 1, 500.f, 0.f, A);
	Graph.FindClosest(FVector(900, -30, 0), 1, 500.f, 0.f, B);
	if (!TestTrue(TEXT("found endpoints"), A.Num() == 1 && B.Num() == 1)) return false;

	TArray<FVector> Forward;
	TArray<FVector> Backward;
	float ForwardLength = 0.f;
	float BackwardLength = 0.f;
	TestTrue(TEXT("forward"), FindPath(Graph, A[0], B[0], Forward, ForwardLength));
	TestTrue(TEXT("backward"), FindPath(Graph, B[0], A[0], Backward, BackwardLength));
	TestNearlyEqual(TEXT("forward length"), ForwardLength, 800.f, 1.f);
	TestNearlyEqual(TEXT("backward length"), BackwardLength, 800.f, 1.f);
	if (Forward.Num() >= 2 && Backward.Num() >= 2)
	{
		TestTrue(TEXT("forward runs +X"), Forward.Last().X > Forward[0].X);
		TestTrue(TEXT("backward runs -X"), Backward.Last().X < Backward[0].X);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathGuideGraphDisconnectedTest, "PathGuide.Graph.Disconnected", TestFlags)
bool FPathGuideGraphDisconnectedTest::RunTest(const FString&)
{
	TArray<TArray<FVector>> Walkways;
	Walkways.Add({ FVector(0, 0, 0), FVector(500, 0, 0) });
	Walkways.Add({ FVector(0, 5000, 0), FVector(500, 5000, 0) });

	FWzPathGuideWalkwayGraph Graph;
	Graph.Build(Walkways, 150.f);
	TestEqual(TEXT("two pieces"), Graph.NumComponents(), 2);

	TArray<FPathGuideEdgePoint> A;
	TArray<FPathGuideEdgePoint> B;
	Graph.FindClosest(FVector(100, 0, 0), 1, 500.f, 0.f, A);
	Graph.FindClosest(FVector(100, 5000, 0), 1, 500.f, 0.f, B);
	if (TestTrue(TEXT("found endpoints"), A.Num() == 1 && B.Num() == 1))
	{
		TArray<FVector> Path;
		float Length = 0.f;
		TestFalse(TEXT("no path between pieces"), FindPath(Graph, A[0], B[0], Path, Length));
	}

	return true;
}

// ====== Geometry ====== //

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathGuideCurveTest, "PathGuide.Geometry.ResampleAndCurve", TestFlags)
bool FPathGuideCurveTest::RunTest(const FString&)
{
	using namespace PathGuide::Geometry;

	// Resample: uniform spacing, both ends kept
	const TArray Line = { FVector(0, 0, 0), FVector(400, 0, 0), FVector(1000, 0, 0) };
	TArray<FVector> Resampled;
	Resample(Line, 100.f, Resampled);
	TestEqual(TEXT("11 points for 1000 at 100"), Resampled.Num(), 11);
	TestTrue(TEXT("first preserved"), Resampled[0].Equals(Line[0], 0.01));
	TestTrue(TEXT("last preserved"), Resampled.Last().Equals(Line.Last(), 0.01));
	for (int32 Index = 1; Index < Resampled.Num(); ++Index) TestNearlyEqual(TEXT("uniform spacing"), static_cast<float>(FVector::Dist(Resampled[Index - 1], Resampled[Index])), 100.f, 1.f);

	// Catmull-Rom: adds points, keeps the ends, passes through every control point (no corner cutting)
	const TArray Corner = { FVector(0, 0, 0), FVector(500, 0, 0), FVector(500, 500, 0), FVector(1000, 500, 0) };
	TArray<FVector> Curve;
	CatmullRom(Corner, 100.f, Curve);
	TestTrue(TEXT("curve adds points"), Curve.Num() > Corner.Num());
	TestTrue(TEXT("curve keeps start"), Curve[0].Equals(Corner[0], 0.01));
	TestTrue(TEXT("curve keeps end"), Curve.Last().Equals(Corner.Last(), 0.01));
	for (const auto& Control : Corner)
	{
		bool bPassesThrough = false;
		for (const auto& Sample : Curve) bPassesThrough |= Sample.Equals(Control, 0.01);
		TestTrue(TEXT("curve passes through a control point"), bPassesThrough);
	}

	// Collinear points stay on their line
	const TArray Straight = { FVector(0, 0, 0), FVector(300, 0, 0), FVector(1000, 0, 0) };
	TArray<FVector> StraightCurve;
	CatmullRom(Straight, 100.f, StraightCurve);
	bool bOnLine = true;
	for (const auto& Sample : StraightCurve) bOnLine &= FMath::IsNearlyZero(Sample.Y, 0.01) && FMath::IsNearlyZero(Sample.Z, 0.01);
	TestTrue(TEXT("collinear points stay on the line"), bOnLine);

	// Cumulative distances
	TArray<float> Distances;
	CumulativeDistances(Line, Distances);
	TestNearlyEqual(TEXT("cumulative total"), Distances.Last(), 1000.f, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathGuideClosestPointTest, "PathGuide.Geometry.ClosestPoint", TestFlags)
bool FPathGuideClosestPointTest::RunTest(const FString&)
{
	using namespace PathGuide::Geometry;

	const TArray Route = { FVector(0, 0, 0), FVector(1000, 0, 0) };
	TArray<float> Distances;
	CumulativeDistances(Route, Distances);

	// 2D ignores the height, 3D does not
	const FVector Location(300, 200, 400);
	const FProjection Flat = ClosestPointOnPolyline(Location, Route, Distances, true, 0, 0);
	TestNearlyEqual(TEXT("horizontal distance"), Flat.Lateral, 200.f, 0.5f);
	TestNearlyEqual(TEXT("distance along"), Flat.DistanceAlong, 300.f, 0.5f);

	const FProjection Straight = ClosestPointOnPolyline(Location, Route, Distances, false, 0, 0);
	TestNearlyEqual(TEXT("straight distance"), Straight.Lateral, FMath::Sqrt(200.f * 200.f + 400.f * 400.f), 0.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathGuideProjectionTest, "PathGuide.Geometry.Projection", TestFlags)
bool FPathGuideProjectionTest::RunTest(const FString&)
{
	using namespace PathGuide::Geometry;

	const TArray Route = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0) };
	TArray<float> Distances;
	CumulativeDistances(Route, Distances);

	const FProjection First = ProjectOntoPolyline(FVector(500, 200, 30), Route, Distances, 0, -1);
	TestNearlyEqual(TEXT("along first leg"), First.DistanceAlong, 500.f, 0.5f);
	TestNearlyEqual(TEXT("lateral is horizontal"), First.Lateral, 200.f, 0.5f);

	const FProjection Second = ProjectOntoPolyline(FVector(1300, 700, 0), Route, Distances, 0, -1);
	TestNearlyEqual(TEXT("along second leg"), Second.DistanceAlong, 1700.f, 0.5f);
	TestNearlyEqual(TEXT("lateral second leg"), Second.Lateral, 300.f, 0.5f);
	TestEqual(TEXT("segment"), Second.SegmentIndex, 1);

	// A window that excludes the true segment keeps the result on the hinted part
	const FProjection Windowed = ProjectOntoPolyline(FVector(1300, 700, 0), Route, Distances, 0, 0);
	TestEqual(TEXT("window keeps hint"), Windowed.SegmentIndex, 0);

	return true;
}

// ====== Ribbon ====== //

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathGuideRibbonGroundTest, "PathGuide.Ribbon.Ground", TestFlags)
bool FPathGuideRibbonGroundTest::RunTest(const FString&)
{
	using namespace PathGuide::Ribbon;

	const TArray<FVector> Centerline = MakeCenterline();

	FParams Params;
	Params.Width = 70.f;
	Params.GroundOffset = 3.f;

	// Spike: flat ground at z = 0 with a single 50 cm spike at x = 500
	auto SpikeTrace = [](const FVector& Start, const FVector&, FVector& OutPosition, FVector& OutNormal)
	{
		OutPosition = { Start.X, Start.Y, FMath::Abs(Start.X - 500.0) < 1.0 ? 50.0 : 0.0 };
		OutNormal = FVector::UpVector;

		return true;
	};

	FPathGuideRibbon Ribbon;
	Build(Centerline, Params, SpikeTrace, Ribbon);
	if (!TestTrue(TEXT("ribbon built"), Ribbon.IsValid()) || !TestEqual(TEXT("sample count"), Ribbon.Num(), Centerline.Num())) return false;

	float MaxError = 0.f;
	for (int32 Index = 0; Index < Ribbon.Num(); ++Index)
	{
		MaxError = FMath::Max3(MaxError, FMath::Abs(static_cast<float>(Ribbon.Centers[Index].Z) - Params.GroundOffset), FMath::Abs(static_cast<float>(Ribbon.Lefts[Index].Z) - Params.GroundOffset));
		MaxError = FMath::Max(MaxError, FMath::Abs(static_cast<float>(Ribbon.Rights[Index].Z) - Params.GroundOffset));
	}
	TestTrue(TEXT("the spike is removed"), MaxError < 0.1f);

	const int32 Mid = Ribbon.Num() / 2;
	const FVector Across = Ribbon.Rights[Mid] - Ribbon.Lefts[Mid];
	TestNearlyEqual(TEXT("edges are perpendicular to the route"), static_cast<float>(FMath::Abs(Across.X)), 0.f, 0.01f);
	TestNearlyEqual(TEXT("ribbon width"), static_cast<float>(Across.Size()), Params.Width, 0.1f);
	TestNearlyEqual(TEXT("total length"), Ribbon.TotalLength, 1000.f, 0.5f);

	// Dip: 50 cm deep and 2 m wide around x = 500, the ribbon must reach the bottom (nothing is averaged away)
	auto DipTrace = [](const FVector& Start, const FVector&, FVector& OutPosition, FVector& OutNormal)
	{
		const double U = (Start.X - 500.0) / 100.0;
		OutPosition = { Start.X, Start.Y, FMath::Abs(U) < 1.0 ? -50.0 * (1.0 - U * U) : 0.0 };
		OutNormal = FVector::UpVector;

		return true;
	};

	FPathGuideRibbon DipRibbon;
	Build(Centerline, Params, DipTrace, DipRibbon);
	if (TestTrue(TEXT("dip ribbon built"), DipRibbon.IsValid())) TestNearlyEqual(TEXT("ribbon follows the dip to the bottom"), static_cast<float>(DipRibbon.Centers[Mid].Z), -50.f + Params.GroundOffset, 3.f);

	// Side slope: ground rises 0.2 per cm to the right, so the edges are 14 cm apart in height
	auto SlopeTrace = [](const FVector& Start, const FVector&, FVector& OutPosition, FVector& OutNormal)
	{
		OutPosition = { Start.X, Start.Y, Start.Y * 0.2 };
		OutNormal = FVector(0.0, -0.2, 1.0).GetSafeNormal();

		return true;
	};

	FPathGuideRibbon SlopeRibbon;
	Build(Centerline, Params, SlopeTrace, SlopeRibbon);
	if (TestTrue(TEXT("slope ribbon built"), SlopeRibbon.IsValid()))
	{
		const FVector SlopeAcross = SlopeRibbon.Rights[Mid] - SlopeRibbon.Lefts[Mid];
		TestNearlyEqual(TEXT("edges follow the side slope"), static_cast<float>(SlopeAcross.Z), 14.f, 0.1f);
		TestNearlyEqual(TEXT("centre sits on the ground"), static_cast<float>(SlopeRibbon.Centers[Mid].Z), static_cast<float>(Params.GroundOffset * SlopeRibbon.Normals[Mid].Z), 0.1f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathGuideRibbonObstacleTest, "PathGuide.Ribbon.Obstacles", TestFlags)
bool FPathGuideRibbonObstacleTest::RunTest(const FString&)
{
	using namespace PathGuide::Ribbon;

	const TArray<FVector> Centerline = MakeCenterline();
	const int32 Mid = Centerline.Num() / 2;

	FParams Params;
	Params.GroundOffset = 3.f;

	// Wall: a steep hit above the ground must be skipped by looking again from below it
	auto WallTrace = [](const FVector& Start, const FVector&, FVector& OutPosition, FVector& OutNormal)
	{
		const bool bWall = Start.Z > 40.0;
		OutPosition = { Start.X, Start.Y, bWall ? 40.0 : 0.0 };
		OutNormal = bWall ? FVector::ForwardVector : FVector::UpVector;

		return true;
	};

	FPathGuideRibbon WallRibbon;
	Build(Centerline, Params, WallTrace, WallRibbon);
	if (TestTrue(TEXT("wall ribbon built"), WallRibbon.IsValid())) TestNearlyEqual(TEXT("ribbon sits on the ground, not on the wall"), static_cast<float>(WallRibbon.Centers[Mid].Z), Params.GroundOffset, 0.1f);

	// Overhead: a flat roof 120 cm above the reference is not ground
	auto RoofTrace = [](const FVector& Start, const FVector&, FVector& OutPosition, FVector& OutNormal)
	{
		OutPosition = { Start.X, Start.Y, Start.Z > 120.0 ? 120.0 : 0.0 };
		OutNormal = FVector::UpVector;

		return true;
	};

	FPathGuideRibbon RoofRibbon;
	Build(Centerline, Params, RoofTrace, RoofRibbon);
	if (TestTrue(TEXT("roof ribbon built"), RoofRibbon.IsValid())) TestNearlyEqual(TEXT("ribbon sits under the roof"), static_cast<float>(RoofRibbon.Centers[Mid].Z), Params.GroundOffset, 0.1f);

	return true;
}

// ====== Formatting ====== //

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathGuideFormatTest, "PathGuide.Format.DistanceAndEta", TestFlags)
bool FPathGuideFormatTest::RunTest(const FString&)
{
	const UWzPathGuideUIConfig* Config = GetDefault<UWzPathGuideUIConfig>();

	TestEqual(TEXT("7 m"), PathGuide::Format::Distance(700.f, *Config).ToString(), FString(TEXT("7 m")));
	TestEqual(TEXT("180 m"), PathGuide::Format::Distance(18000.f, *Config).ToString(), FString(TEXT("180 m")));
	TestEqual(TEXT("5 m steps"), PathGuide::Format::Distance(18300.f, *Config).ToString(), FString(TEXT("185 m")));
	TestEqual(TEXT("1.2 km"), PathGuide::Format::Distance(120000.f, *Config).ToString(), FString(TEXT("1.2 km")));

	TestEqual(TEXT("under a minute"), PathGuide::Format::Eta(5000.f, *Config).ToString(), FString(TEXT("< 1 min")));
	TestEqual(TEXT("rounds up"), PathGuide::Format::Eta(18000.f, *Config).ToString(), FString(TEXT("3 min")));

	TestEqual(TEXT("combined"), PathGuide::Format::DistanceAndEta(18000.f, *Config).ToString(), FString(TEXT("180 m · 3 min")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS