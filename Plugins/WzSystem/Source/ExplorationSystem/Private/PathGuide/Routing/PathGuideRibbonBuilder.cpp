// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/Routing/PathGuideRibbonBuilder.h"

#include "PathGuide/Routing/PathGuideGeometry.h"

namespace PathGuide::Ribbon
{
	// ====== Helpers ====== //

	namespace
	{
		constexpr float MinNormalZ = 0.6f;			// steeper hits (rocks, walls, trunks) are not ground
		constexpr int32 MaxTraceAttempts = 3;		// retraces from below an obstacle before a miss
		constexpr int32 SpikeRadius = 2;			// the median covers 2 * this + 1 samples
		constexpr int32 NormalRadius = 2;			// normals are averaged over 2 * this + 1 samples

		constexpr int32 LeftLane = 0;
		constexpr int32 CentreLane = 1;
		constexpr int32 RightLane = 2;
		constexpr int32 LaneCount = 3;

		/** Ground height per sample for one lane. Valid is cleared where there is no usable hit. */
		struct FLane
		{
			TArray<float> Z;
			TArray<uint8> Valid;
		};

		/** Clears Valid where a height is further than Threshold from its neighbours' median. */
		void RejectSpikes(FLane& Lane, const float Threshold)
		{
			TArray<uint8> Keep = Lane.Valid;
			TArray<float> Window;
			for (int32 Index = 0; Index < Lane.Z.Num(); ++Index)
			{
				if (!Lane.Valid[Index]) continue;

				// Median of the valid heights around the sample (it counts itself).
				Window.Reset();
				for (int32 Neighbour = Index - SpikeRadius; Neighbour <= Index + SpikeRadius; ++Neighbour)
				{
					if (Lane.Valid.IsValidIndex(Neighbour) && Lane.Valid[Neighbour]) Window.Add(Lane.Z[Neighbour]);
				}
				Window.Sort();

				if (FMath::Abs(Lane.Z[Index] - Window[Window.Num() / 2]) > Threshold) Keep[Index] = 0;
			}
			Lane.Valid = MoveTemp(Keep);
		}

		/** Interpolates invalid samples from valid neighbours (Fallback when there are none). */
		void FillMissing(TArray<float>& Values, const TArray<uint8>& Valid, const TArray<float>& Fallback)
		{
			const int32 Count = Values.Num();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (Valid[Index]) continue;

				// Nearest valid neighbour on each side.
				int32 Prev = Index - 1;
				while (Prev >= 0 && !Valid[Prev]) --Prev;
				int32 Next = Index + 1;
				while (Next < Count && !Valid[Next]) ++Next;

				// Both sides: interpolate. One side: copy it. None: use the fallback.
				if (Prev >= 0 && Next < Count) Values[Index] = FMath::Lerp(Values[Prev], Values[Next], static_cast<float>(Index - Prev) / static_cast<float>(Next - Prev));
				else if (Prev >= 0) Values[Index] = Values[Prev];
				else if (Next < Count) Values[Index] = Values[Next];
				else Values[Index] = Fallback[Index];
			}
		}
	}

	// ====== API ====== //

	bool TraceGround(const FGroundTraceFn& Trace, const double X, const double Y, const double ReferenceZ, const FParams& Params, FVector& OutPosition, FVector& OutNormal)
	{
		const double EndZ = ReferenceZ - Params.TraceDown;
		double StartZ = ReferenceZ + Params.TraceUp;
		for (int32 Attempt = 0; Attempt < MaxTraceAttempts; ++Attempt)
		{
			FVector Position;
			FVector Normal;
			if (!Trace({ X, Y, StartZ }, { X, Y, EndZ }, Position, Normal)) return false;

			// Ground: flat enough and not overhead.
			if (Normal.Z >= MinNormalZ && Position.Z <= ReferenceZ + Params.OverheadReject)
			{
				OutPosition = Position;
				OutNormal = Normal;

				return true;
			}

			// Obstacle or overhead: retrace from just below it.
			StartZ = Position.Z - 1.0;
			if (StartZ <= EndZ) return false;
		}

		return false;
	}

	void Build(const TArray<FVector>& Centerline, const FParams& Params, const FGroundTraceFn& Trace, FPathGuideRibbon& OutRibbon)
	{
		OutRibbon = FPathGuideRibbon();

		const int32 Count = Centerline.Num();
		if (Count < 2) return;

		// Horizontal right vector at each sample.
		TArray<FVector> Rights2D;
		Rights2D.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector& Prev = Centerline[FMath::Max(Index - 1, 0)];
			const FVector& Next = Centerline[FMath::Min(Index + 1, Count - 1)];
			FVector Forward = Next - Prev;
			Forward.Z = 0.0;
			Forward = Forward.GetSafeNormal();
			if (Forward.IsNearlyZero()) Forward = Index > 0 ? (Centerline[Index] - Centerline[Index - 1]).GetSafeNormal2D() : FVector::ForwardVector;
			Rights2D[Index] = { -Forward.Y, Forward.X, 0.0 };
		}

		// Reference height per sample: the centreline itself.
		TArray<float> ReferenceZ;
		ReferenceZ.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index) ReferenceZ[Index] = static_cast<float>(Centerline[Index].Z);

		// Trace all three lanes
		const double HalfWidth = Params.Width * 0.5;
		TArray<FLane> Lanes;
		Lanes.SetNum(LaneCount);
		for (auto& Lane : Lanes)
		{
			Lane.Z.SetNumZeroed(Count);
			Lane.Valid.SetNumZeroed(Count);
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			for (int32 LaneIndex = 0; LaneIndex < LaneCount; ++LaneIndex)
			{
				const FVector Side = Rights2D[Index] * (HalfWidth * (LaneIndex - CentreLane));
				FVector Position;
				FVector Normal;
				if (!TraceGround(Trace, Centerline[Index].X + Side.X, Centerline[Index].Y + Side.Y, Centerline[Index].Z, Params, Position, Normal)) continue;

				Lanes[LaneIndex].Z[Index] = static_cast<float>(Position.Z);
				Lanes[LaneIndex].Valid[Index] = 1;
			}
		}

		// A missed edge borrows the centre height.
		for (const auto EdgeIndex : { LeftLane, RightLane })
		{
			FLane& Edge = Lanes[EdgeIndex];
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (Edge.Valid[Index] || !Lanes[CentreLane].Valid[Index]) continue;

				Edge.Z[Index] = Lanes[CentreLane].Z[Index];
				Edge.Valid[Index] = 1;
			}
		}

		// Drop spikes, fill misses (no averaging, so dips stay)
		for (auto& Lane : Lanes)
		{
			RejectSpikes(Lane, Params.SpikeThreshold);
			FillMissing(Lane.Z, Lane.Valid, ReferenceZ);
		}

		// World positions
		OutRibbon.Lefts.SetNumUninitialized(Count);
		OutRibbon.Centers.SetNumUninitialized(Count);
		OutRibbon.Rights.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector& C = Centerline[Index];
			const FVector Side = Rights2D[Index] * HalfWidth;
			OutRibbon.Lefts[Index] = { C.X - Side.X, C.Y - Side.Y, Lanes[LeftLane].Z[Index] };
			OutRibbon.Centers[Index] = { C.X, C.Y, Lanes[CentreLane].Z[Index] };
			OutRibbon.Rights[Index] = { C.X + Side.X, C.Y + Side.Y, Lanes[RightLane].Z[Index] };
		}

		// Surface normals (always up)
		TArray<FVector> RawNormals;
		RawNormals.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Along = OutRibbon.Centers[FMath::Min(Index + 1, Count - 1)] - OutRibbon.Centers[FMath::Max(Index - 1, 0)];
			const FVector Across = OutRibbon.Rights[Index] - OutRibbon.Lefts[Index];
			FVector Normal = FVector::CrossProduct(Along, Across).GetSafeNormal();
			if (Normal.IsNearlyZero()) Normal = FVector::UpVector;
			if (Normal.Z < 0.0) Normal = -Normal;
			RawNormals[Index] = Normal;
		}

		// Smooth the normals along the route
		OutRibbon.Normals.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Reach = FMath::Min3(NormalRadius, Index, Count - 1 - Index);
			FVector Sum = FVector::ZeroVector;
			for (int32 Offset = -Reach; Offset <= Reach; ++Offset) Sum += RawNormals[Index + Offset];

			const FVector Smoothed = Sum.GetSafeNormal();
			OutRibbon.Normals[Index] = Smoothed.IsZero() ? FVector::UpVector : Smoothed;
		}

		// Lift off the ground
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Lift = OutRibbon.Normals[Index] * Params.GroundOffset;
			OutRibbon.Lefts[Index] += Lift;
			OutRibbon.Centers[Index] += Lift;
			OutRibbon.Rights[Index] += Lift;
		}

		// Distances from the input centreline
		Geometry::CumulativeDistances(Centerline, OutRibbon.Distances);
		OutRibbon.TotalLength = OutRibbon.Distances.Last();
	}
}