// Copyright Epic Games, Inc. All Rights Reserved.

#include "Duma/Behavior/ZooDumaMotion.h"

/** How much of the screen edge a showpiece may use. */
static constexpr float ViewMargin = 0.85f;

/** Smooth 0..1. */
static float Ease(const float Alpha)
{
	return FMath::SmoothStep(0.f, 1.f, Alpha);
}

/** 0 at both ends, 1 through the middle: a pull toward the view centre that fades in and out. */
static float Envelope(const float Alpha)
{
	return FMath::SmoothStep(0.f, 0.2f, Alpha) * (1.f - FMath::SmoothStep(0.8f, 1.f, Alpha));
}

/** The follow spot sits in a corner; this moves a showpiece toward the middle of the view. */
static FVector CenterPull(const FVector& FollowOffset, const float Weight, const float Alpha)
{
	return FVector{ 0.f, -FollowOffset.Y, -FollowOffset.Z } * (Weight * Envelope(Alpha));
}

// ====== Drift ====== //

FZooDumaPose ZooDuma::Drift(const float Time, const FZooDumaDriftSettings& Settings)
{
	// A figure-8: sideways once per lap, up and down twice
	const float Phase = UE_TWO_PI * Settings.Speed * Time;

	FZooDumaPose Pose;
	Pose.Offset.Y = Settings.Width * FMath::Sin(Phase);
	Pose.Offset.Z = Settings.Height * FMath::Sin(2.f * Phase) + Settings.Bob * FMath::Sin(0.6f * Phase + 1.3f);
	Pose.Rotation.Yaw = Settings.YawSway * FMath::Sin(0.7f * Phase);
	Pose.Rotation.Roll = Settings.Lean * FMath::Cos(Phase);

	return Pose;
}

// ====== Showpieces ====== //

FZooDumaPose ZooDuma::Showpiece(const EZooDumaShowpiece Type, const float Alpha, const FZooDumaVariation& Variation, const FZooDumaShowpieceSettings& Settings, const FVector& FollowOffset)
{
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	const float E = Ease(A);
	const float Size = Settings.Size * Variation.Scale;
	const float Mirror = Variation.Mirror;

	// Swoop and peek start in the corner and cross the view, so their side comes from the corner, not from the mirror
	const float Across = FollowOffset.Y >= 0.f ? -1.f : 1.f;

	FZooDumaPose Pose;
	switch (Type)
	{
	case EZooDumaShowpiece::Loop:
	{
		// A circle standing in the view plane, starting and ending at its bottom
		const float Theta = UE_TWO_PI * E;
		Pose.Offset = CenterPull(FollowOffset, 0.9f, A) + FVector{ 0.f, Mirror * Size * FMath::Sin(Theta), Size * (1.f - FMath::Cos(Theta)) };
		Pose.Rotation.Roll = -Mirror * FMath::RadiansToDegrees(Theta);
		break;
	}
	case EZooDumaShowpiece::BarrelRoll:
	{
		// A hop to the side while flipping once around the sideways axis
		Pose.Offset = CenterPull(FollowOffset, 0.6f, A) + FVector{ 0.f, Mirror * Size * FMath::Sin(UE_PI * E), 0.f };
		Pose.Rotation.Pitch = Mirror * 360.f * E;
		break;
	}
	case EZooDumaShowpiece::Swoop:
	{
		// Across the view, dipping low on the way out and arcing a little higher on the way back
		const float Lap = FMath::Sin(UE_TWO_PI * E);
		Pose.Offset = FVector{ 0.f, Across * Size * FMath::Sin(UE_PI * E), (Lap > 0.f ? -0.4f : -0.15f) * Size * Lap };
		Pose.Rotation.Roll = -Across * 25.f * FMath::Cos(UE_PI * E);
		break;
	}
	case EZooDumaShowpiece::Spiral:
	{
		// Two turns around a rising axis, wide in the middle, toward the visitor on each turn
		const float Theta = 2.f * UE_TWO_PI * E;
		const float Wide = FMath::Sin(UE_PI * E);
		Pose.Offset = CenterPull(FollowOffset, 0.7f, A) + FVector{ 0.5f * Size * Wide * (FMath::Cos(Theta) - 1.f), Mirror * Size * Wide * FMath::Sin(Theta), 2.f * Size * FMath::Sin(UE_PI * E) };
		Pose.Rotation.Roll = -Mirror * 15.f * Wide * FMath::Sin(Theta);
		break;
	}
	case EZooDumaShowpiece::PeekABoo:
	{
		// At the side of the view, wiggling (no easing: he is hidden while it jumps)
		Pose.Offset = FVector{ 0.f, Across * Size, 10.f };
		Pose.Rotation.Yaw = Mirror * 25.f * FMath::Sin(UE_TWO_PI * 2.f * A * Settings.Duration);
		break;
	}
	case EZooDumaShowpiece::Swing:
	{
		// A pendulum around the view centre: lowest in the middle, a decaying 1.5 laps
		const float Phi = FMath::DegreesToRadians(55.f) * FMath::Sin(UE_TWO_PI * 1.5f * A) * (1.f - A) * FMath::SmoothStep(0.f, 0.1f, A);
		Pose.Offset = CenterPull(FollowOffset, 0.8f, A) + FVector{ 0.f, Mirror * Size * FMath::Sin(Phi), Size * (1.f - FMath::Cos(Phi)) };
		Pose.Rotation.Roll = -Mirror * FMath::RadiansToDegrees(Phi);
		break;
	}
	}

	return Pose;
}

// ====== Glance ====== //

FZooDumaPose ZooDuma::Glance(const float Time, const FZooDumaGlanceSettings& Settings, const float Mirror, const FVector& FollowOffset)
{
	// The yaw that faces the camera from where he hovers, turned the way Mirror says (the long way round if it must)
	float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(-FollowOffset.Y, -FollowOffset.X));
	if (TargetYaw * Mirror < 0.f) TargetYaw += TargetYaw > 0.f ? -360.f : 360.f;

	// Turn in, hold, turn back
	const float Turn = FMath::Max(Settings.TurnSeconds, KINDA_SMALL_NUMBER);
	const float TurnedBack = Turn + Settings.HoldSeconds;
	const float Turned = Time < TurnedBack ? Ease(Time / Turn) : 1.f - Ease((Time - TurnedBack) / Turn);

	// A curious tilt that comes and goes while he holds
	const float HoldAlpha = Settings.HoldSeconds > 0.f ? FMath::Clamp((Time - Turn) / Settings.HoldSeconds, 0.f, 1.f) : 0.f;

	FZooDumaPose Pose;
	Pose.Rotation.Yaw = TargetYaw * Turned;
	Pose.Rotation.Roll = Mirror * Settings.Tilt * FMath::Sin(UE_PI * HoldAlpha);

	return Pose;
}

// ====== View ====== //

FVector ZooDuma::ClampToView(const FVector& ViewPoint, const float HorizontalFovDegrees, const float AspectRatio, const float MinAhead)
{
	// The screen's half extents at that depth, each axis on its own
	const float TanHorizontal = FMath::Tan(FMath::DegreesToRadians(HorizontalFovDegrees * 0.5f)) * ViewMargin;
	const float TanVertical = TanHorizontal / FMath::Max(AspectRatio, 0.1f);

	FVector Clamped = ViewPoint;
	Clamped.X = FMath::Max(ViewPoint.X, MinAhead);
	Clamped.Y = FMath::Clamp(ViewPoint.Y, -Clamped.X * TanHorizontal, Clamped.X * TanHorizontal);
	Clamped.Z = FMath::Clamp(ViewPoint.Z, -Clamped.X * TanVertical, Clamped.X * TanVertical);

	return Clamped;
}