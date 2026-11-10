// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/Routing/PathGuideGeometry.h"

namespace PathGuide::Geometry
{
	// ====== Helpers ====== //

	namespace
	{
		/** Closest point on a segment (horizontal when b2D). OutT is 0..1 along it. Returns the distance. */
		float ClosestOnSegment(const FVector& P, const FVector& A, const FVector& B, const bool b2D, FVector& OutPoint, double& OutT)
		{
			// Segment and offset to the point, flattened for the horizontal search.
			FVector AB = B - A;
			FVector AP = P - A;
			if (b2D)
			{
				AB.Z = 0.0;
				AP.Z = 0.0;
			}

			// Clamped projection onto the segment.
			const double LengthSq = AB.SizeSquared();
			OutT = LengthSq > UE_DOUBLE_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(AP, AB) / LengthSq, 0.0, 1.0) : 0.0;
			OutPoint = FMath::Lerp(A, B, OutT);

			return static_cast<float>(b2D ? FVector::Dist2D(P, OutPoint) : FVector::Dist(P, OutPoint));
		}

		/** Centripetal knot step: square root of the chord. */
		double KnotStep(const FVector& A, const FVector& B)
		{
			return FMath::Sqrt(FVector::Dist(A, B));
		}

		/** Point at time T on the line from A (at TA) to B (at TB). */
		FVector LerpAt(const FVector& A, const FVector& B, const double TA, const double TB, const double T)
		{
			const double Span = TB - TA;

			return Span > UE_DOUBLE_SMALL_NUMBER ? A + (B - A) * ((T - TA) / Span) : A;
		}
	}

	// ====== Polyline ====== //

	void RemoveDuplicates(TArray<FVector>& Points, const float MinDistance)
	{
		if (Points.Num() < 2) return;

		// Keep a point only when it is far enough from the last kept one.
		const double MinSq = static_cast<double>(MinDistance) * MinDistance;
		TArray<FVector> Result;
		Result.Reserve(Points.Num());
		Result.Add(Points[0]);
		for (int32 Index = 1; Index < Points.Num() - 1; ++Index)
		{
			if (FVector::DistSquared(Result.Last(), Points[Index]) >= MinSq) Result.Add(Points[Index]);
		}

		// The last point always stays (it replaces the previous kept one when they are too close).
		const FVector& Last = Points.Last();
		if (Result.Num() > 1 && FVector::DistSquared(Result.Last(), Last) < MinSq) Result.Last() = Last;
		else Result.Add(Last);
		Points = MoveTemp(Result);
	}

	void CumulativeDistances(const TArray<FVector>& Points, TArray<float>& OutDistances)
	{
		OutDistances.SetNumUninitialized(Points.Num());
		float Sum = 0.f;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			if (Index > 0) Sum += static_cast<float>(FVector::Dist(Points[Index - 1], Points[Index]));
			OutDistances[Index] = Sum;
		}
	}

	FVector PointAtDistance(const TArray<FVector>& Points, const TArray<float>& Cumulative, const float Distance)
	{
		if (Distance <= 0.f) return Points[0];
		if (Distance >= Cumulative.Last()) return Points.Last();

		// The segment that holds the distance.
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			if (Distance <= Cumulative[Index])
			{
				const float Segment = Cumulative[Index] - Cumulative[Index - 1];
				const double T = Segment > UE_KINDA_SMALL_NUMBER ? (Distance - Cumulative[Index - 1]) / Segment : 0.0;

				return FMath::Lerp(Points[Index - 1], Points[Index], T);
			}
		}

		return Points.Last();
	}

	void Resample(const TArray<FVector>& In, const float Spacing, TArray<FVector>& Out)
	{
		Out.Reset();
		if (In.Num() < 2)
		{
			Out = In;

			return;
		}

		// Degenerate polyline: just its two ends.
		TArray<float> Cumulative;
		CumulativeDistances(In, Cumulative);
		const float Length = Cumulative.Last();
		if (Length < UE_KINDA_SMALL_NUMBER)
		{
			Out = { In[0], In.Last() };

			return;
		}

		// The segment count is rounded, so the spacing is adjusted slightly to hit the last point exactly.
		const int32 Segments = FMath::Max(1, FMath::RoundToInt(Length / FMath::Max(Spacing, 1.f)));
		Out.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index) Out.Add(PointAtDistance(In, Cumulative, Length * static_cast<float>(Index) / static_cast<float>(Segments)));
	}

	void CatmullRom(const TArray<FVector>& In, const float Spacing, TArray<FVector>& Out)
	{
		// Two points are already a straight line.
		Out.Reset();
		if (In.Num() < 3)
		{
			Out = In;

			return;
		}

		// One piece per segment (the ends mirror a neighbour)
		const int32 LastIndex = In.Num() - 1;
		Out.Add(In[0]);
		for (int32 Index = 0; Index < LastIndex; ++Index)
		{
			const FVector& P1 = In[Index];
			const FVector& P2 = In[Index + 1];
			const FVector P0 = Index > 0 ? In[Index - 1] : P1 * 2.0 - P2;
			const FVector P3 = Index + 1 < LastIndex ? In[Index + 2] : P2 * 2.0 - P1;

			// Knot times (alpha 0.5)
			const double T1 = KnotStep(P0, P1);
			const double T2 = T1 + KnotStep(P1, P2);
			const double T3 = T2 + KnotStep(P2, P3);

			// Inner samples, then the control point
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist(P1, P2) / FMath::Max(Spacing, 1.f)));
			for (int32 Step = 1; Step < Steps; ++Step)
			{
				const double T = FMath::Lerp(T1, T2, static_cast<double>(Step) / Steps);

				// Barry-Goldman pyramid.
				const FVector A1 = LerpAt(P0, P1, 0.0, T1, T);
				const FVector A2 = LerpAt(P1, P2, T1, T2, T);
				const FVector A3 = LerpAt(P2, P3, T2, T3, T);
				const FVector B1 = LerpAt(A1, A2, 0.0, T2, T);
				const FVector B2 = LerpAt(A2, A3, T1, T3, T);
				Out.Add(LerpAt(B1, B2, T1, T2, T));
			}
			Out.Add(P2);
		}
	}

	FProjection ClosestPointOnPolyline(const FVector& Location, const TArray<FVector>& Points, const TArray<float>& Cumulative, const bool b2D, const int32 FirstSegment, const int32 LastSegment)
	{
		FProjection Best;
		Best.Lateral = TNumericLimits<float>::Max();
		if (Points.Num() < 2) return Best;

		// The closest segment of the range wins.
		const int32 LastIndex = Points.Num() - 2;
		const int32 First = FMath::Clamp(FirstSegment, 0, LastIndex);
		const int32 Last = FMath::Clamp(LastSegment, 0, LastIndex);
		for (int32 Segment = First; Segment <= Last; ++Segment)
		{
			FVector Closest;
			double T = 0.0;
			const float Lateral = ClosestOnSegment(Location, Points[Segment], Points[Segment + 1], b2D, Closest, T);
			if (Lateral < Best.Lateral)
			{
				Best.Lateral = Lateral;
				Best.SegmentIndex = Segment;
				Best.Point = Closest;
				Best.DistanceAlong = Cumulative[Segment] + static_cast<float>(T) * (Cumulative[Segment + 1] - Cumulative[Segment]);
			}
		}

		return Best;
	}

	FProjection ProjectOntoPolyline(const FVector& Location, const TArray<FVector>& Points, const TArray<float>& Cumulative, const int32 HintSegment, const int32 Window)
	{
		// Segments to search: the window around the hint (a negative window searches the whole polyline).
		const int32 First = Window >= 0 ? HintSegment - Window : 0;
		const int32 Last = Window >= 0 ? HintSegment + Window : Points.Num() - 2;

		return ClosestPointOnPolyline(Location, Points, Cumulative, true, First, Last);
	}
}