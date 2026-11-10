// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PathGuide/PathGuideTypes.h"

/** Drapes the route ribbon on the ground. Pure maths (no UObjects). */
namespace PathGuide::Ribbon
{
	// ====== Types ====== //

	/** Line trace straight down on the ground channel. False on no hit. */
	using FGroundTraceFn = TFunctionRef<bool(const FVector& Start, const FVector& End, FVector& OutPosition, FVector& OutNormal)>;

	struct FParams
	{
		float Width = 70.f;
		float GroundOffset = 2.f;

		/** Trace window above the reference height. */
		float TraceUp = 150.f;

		/** Trace window below the reference height. */
		float TraceDown = 1000.f;

		/** Hits higher than this above the reference (rails, roofs) are not ground. */
		float OverheadReject = 100.f;

		/** Lane heights further than this from their local median are spikes. */
		float SpikeThreshold = 40.f;
	};

	// ====== API ====== //

	/** Ground height below (X, Y), skipping steep and overhead hits by retracing from just below them. */
	EXPLORATIONSYSTEM_API bool TraceGround(const FGroundTraceFn& Trace, double X, double Y, double ReferenceZ, const FParams& Params, FVector& OutPosition, FVector& OutNormal);

	/** Ribbon draped on the ground under an evenly spaced centreline, every lane vertex traced. */
	EXPLORATIONSYSTEM_API void Build(const TArray<FVector>& Centerline, const FParams& Params, const FGroundTraceFn& Trace, FPathGuideRibbon& OutRibbon);
}