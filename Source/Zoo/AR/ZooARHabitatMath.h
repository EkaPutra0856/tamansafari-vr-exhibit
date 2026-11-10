#pragma once

#include "CoreMinimal.h"

namespace ZooAR
{
// Signed distance to the actual rounded exhibit footprint, in world centimetres.
inline double HabitatDistance(const FVector2D& Point, const FBox& Bounds, double CornerRadius)
{
    const FVector2D Center((Bounds.Min.X + Bounds.Max.X) * .5, (Bounds.Min.Y + Bounds.Max.Y) * .5);
    const FVector2D Half((Bounds.Max.X - Bounds.Min.X) * .5, (Bounds.Max.Y - Bounds.Min.Y) * .5);
    const double Radius = FMath::Clamp(CornerRadius, 0., FMath::Min(Half.X, Half.Y));
    const FVector2D Q(FMath::Abs(Point.X - Center.X) - Half.X + Radius,
                      FMath::Abs(Point.Y - Center.Y) - Half.Y + Radius);
    return FVector2D(FMath::Max(Q.X, 0.), FMath::Max(Q.Y, 0.)).Length()
        + FMath::Min(FMath::Max(Q.X, Q.Y), 0.) - Radius;
}

inline bool InsideHabitat(const FVector& Point, const FBox& Bounds, double CornerRadius, double Margin = 0.)
{
    return Bounds.IsValid && Point.Z >= Bounds.Min.Z + Margin && Point.Z <= Bounds.Max.Z - Margin
        && HabitatDistance(FVector2D(Point), Bounds, CornerRadius) <= -Margin;
}

inline bool RayBox(const FVector& Start, const FVector& Direction, const FBox& Box, double MaxDistance)
{
    double Near = 0., Far = MaxDistance;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Abs(Direction[Axis]) < 1.e-8)
        {
            if (Start[Axis] < Box.Min[Axis] || Start[Axis] > Box.Max[Axis]) return false;
        }
        else
        {
            double A = (Box.Min[Axis] - Start[Axis]) / Direction[Axis];
            double B = (Box.Max[Axis] - Start[Axis]) / Direction[Axis];
            if (A > B) Swap(A, B);
            Near = FMath::Max(Near, A);
            Far = FMath::Min(Far, B);
            if (Far < Near) return false;
        }
    }
    return Far >= 0. && Near <= MaxDistance;
}

// Rounded rectangle = two rectangular prisms + four vertical corner cylinders.
// This excludes the paths, deck, and the empty corners of a coarse fence AABB.
inline bool RayIntersectsHabitat(const FVector& Start, const FVector& Direction, const FBox& Bounds,
                                double CornerRadius, double MaxDistance = 15000.)
{
    if (!Bounds.IsValid || Direction.IsNearlyZero()) return false;
    const FVector D = Direction.GetSafeNormal();
    const double R = FMath::Clamp(CornerRadius, 0., FMath::Min(Bounds.GetExtent().X, Bounds.GetExtent().Y));
    if (!RayBox(Start, D, Bounds, MaxDistance)) return false;
    if (R <= 0.) return true;
    if (RayBox(Start, D, FBox(FVector(Bounds.Min.X + R, Bounds.Min.Y, Bounds.Min.Z),
                              FVector(Bounds.Max.X - R, Bounds.Max.Y, Bounds.Max.Z)), MaxDistance)
        || RayBox(Start, D, FBox(FVector(Bounds.Min.X, Bounds.Min.Y + R, Bounds.Min.Z),
                                 FVector(Bounds.Max.X, Bounds.Max.Y - R, Bounds.Max.Z)), MaxDistance)) return true;
    for (double X : { Bounds.Min.X + R, Bounds.Max.X - R })
    for (double Y : { Bounds.Min.Y + R, Bounds.Max.Y - R })
    {
        const double A = D.X * D.X + D.Y * D.Y;
        const double B = 2. * ((Start.X - X) * D.X + (Start.Y - Y) * D.Y);
        const double C = FMath::Square(Start.X - X) + FMath::Square(Start.Y - Y) - R * R;
        double Near = 0., Far = MaxDistance;
        if (A < 1.e-12) { if (C > 0.) continue; }
        else
        {
            const double Disc = B * B - 4. * A * C;
            if (Disc < 0.) continue;
            Near = FMath::Max(Near, (-B - FMath::Sqrt(Disc)) / (2. * A));
            Far = FMath::Min(Far, (-B + FMath::Sqrt(Disc)) / (2. * A));
        }
        if (FMath::Abs(D.Z) < 1.e-8)
        {
            if (Start.Z < Bounds.Min.Z || Start.Z > Bounds.Max.Z) continue;
        }
        else
        {
            double Z0 = (Bounds.Min.Z - Start.Z) / D.Z, Z1 = (Bounds.Max.Z - Start.Z) / D.Z;
            if (Z0 > Z1) Swap(Z0, Z1);
            Near = FMath::Max(Near, Z0); Far = FMath::Min(Far, Z1);
        }
        if (Near <= Far) return true;
    }
    return false;
}
}
