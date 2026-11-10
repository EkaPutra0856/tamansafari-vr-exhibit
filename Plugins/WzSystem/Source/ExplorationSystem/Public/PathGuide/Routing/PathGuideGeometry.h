// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Pure polyline maths (no UObjects), world space, cm. */
namespace PathGuide::Geometry
{
	// ====== Types ====== //

	/** Result of projecting a location onto a polyline. Lateral is the distance from the location to Point. */
	struct FProjection
	{
		float DistanceAlong = 0.f;
		float Lateral = 0.f;
		int32 SegmentIndex = 0;
		FVector Point = FVector::ZeroVector;
	};

	// ====== Polyline ====== //

	/** Drops consecutive points closer than MinDistance (first and last point are always kept). */
	EXPLORATIONSYSTEM_API void RemoveDuplicates(TArray<FVector>& Points, float MinDistance);

	EXPLORATIONSYSTEM_API void CumulativeDistances(const TArray<FVector>& Points, TArray<float>& OutDistances);

	/** Point at Distance along the polyline (clamped to its ends). Cumulative comes from CumulativeDistances. */
	EXPLORATIONSYSTEM_API FVector PointAtDistance(const TArray<FVector>& Points, const TArray<float>& Cumulative, float Distance);

	/** Evenly spaced points along the polyline, first and last point preserved. Spacing is adjusted slightly to fit. */
	EXPLORATIONSYSTEM_API void Resample(const TArray<FVector>& In, float Spacing, TArray<FVector>& Out);

	/** Centripetal Catmull-Rom through every point (no corner cutting). Samples are about Spacing apart. */
	EXPLORATIONSYSTEM_API void CatmullRom(const TArray<FVector>& In, float Spacing, TArray<FVector>& Out);

	/** Nearest point on segments FirstSegment..LastSegment. Lateral is horizontal when b2D, else straight. */
	EXPLORATIONSYSTEM_API FProjection ClosestPointOnPolyline(const FVector& Location, const TArray<FVector>& Points, const TArray<float>& Cumulative, bool b2D, int32 FirstSegment, int32 LastSegment);

	/** Horizontal nearest point within HintSegment +/- Window segments (Window < 0 searches all). */
	EXPLORATIONSYSTEM_API FProjection ProjectOntoPolyline(const FVector& Location, const TArray<FVector>& Points, const TArray<float>& Cumulative, int32 HintSegment, int32 Window);
}