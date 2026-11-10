// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZooDumaMotion.generated.h"

/** Duma's idle showpieces: short flourishes in front of the visitor. */
UENUM(BlueprintType)
enum class EZooDumaShowpiece : uint8
{
	/** A vertical loop in the view plane. */
	Loop,
	/** A hop to the side with a full flip. */
	BarrelRoll,
	/** Dives across the lower view, arcs up, glides back. */
	Swoop,
	/** Spirals up and back, with a sparkle burst at the top. */
	Spiral,
	/** Blinks out, wiggles in from the side, blinks home. */
	PeekABoo,
	/** A damped pendulum swing across the view. */
	Swing,
};

/** One showpiece's tuning. */
USTRUCT(BlueprintType)
struct FZooDumaShowpieceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;

	/** Chance weight against the other showpieces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** Seconds (for a peek: seconds at the side). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float Duration = 2.f;

	/** Size in cm: radius, width or hop, depending on the showpiece. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Size = 40.f;
};

/** The always-on idle motion: a slow figure-8 with a gentle bob and sway. */
USTRUCT(BlueprintType)
struct FZooDumaDriftSettings
{
	GENERATED_BODY()

	/** Sideways reach of the figure-8 (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Width = 14.f;

	/** Vertical reach of the figure-8 (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Height = 9.f;

	/** Extra slow bob (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Bob = 5.f;

	/** Figure-8 laps per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Speed = 0.17f;

	/** Yaw sway (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float YawSway = 8.f;

	/** Lean into the sideways drift (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Lean = 6.f;
};

/** The "checking on the visitor" glance: he turns to face the camera, holds with a curious tilt, then turns back. */
USTRUCT(BlueprintType)
struct FZooDumaGlanceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;

	/** Seconds between two glances: a random gap in this range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MinGap = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MaxGap = 7.f;

	/** Seconds to turn toward the visitor (and the same to turn back). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float TurnSeconds = 0.6f;

	/** Seconds spent facing the visitor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float HoldSeconds = 1.2f;

	/** Curious head tilt while facing him (degrees of roll). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Tilt = 12.f;
};

/** Where the mesh is moved from the follow spot, in the visitor's view space (X ahead, Y right, Z up). */
struct FZooDumaPose
{
	FVector Offset = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;

	FZooDumaPose& operator+=(const FZooDumaPose& Other)
	{
		Offset += Other.Offset;
		Rotation += Other.Rotation;

		return *this;
	}
};

/** What makes each play different: mirrored left/right, a little bigger or faster. */
struct FZooDumaVariation
{
	/** +1 or -1: which side a centred showpiece goes to (a swoop or peek always crosses from the corner). */
	float Mirror = 1.f;
	float Scale = 1.f;
	float Speed = 1.f;
};

/** Pure motion math: no UObjects, no world. */
namespace ZooDuma
{
	/** The idle figure-8 at Time seconds. */
	FZooDumaPose Drift(float Time, const FZooDumaDriftSettings& Settings);

	/** A showpiece at Alpha 0..1 (a peek: progress at the side). Zero at both ends, so it returns seamlessly. FollowOffset lets it pull toward the view centre. */
	FZooDumaPose Showpiece(EZooDumaShowpiece Type, float Alpha, const FZooDumaVariation& Variation, const FZooDumaShowpieceSettings& Settings, const FVector& FollowOffset);

	/** Seconds a whole glance lasts: turn, hold, turn back. */
	FORCEINLINE float GlanceDuration(const FZooDumaGlanceSettings& Settings) { return 2.f * Settings.TurnSeconds + Settings.HoldSeconds; }

	/** The glance at Time seconds: turns toward the camera (the way Mirror says, +1 right, -1 left) and tilts while facing it. Zero at both ends. FollowOffset is where he hovers, so he faces the camera and not straight back. */
	FZooDumaPose Glance(float Time, const FZooDumaGlanceSettings& Settings, float Mirror, const FVector& FollowOffset);

	/** Keeps a view-space point inside the screen: at least MinAhead in front, each axis inside the real FOV and aspect ratio (with a margin). */
	FVector ClampToView(const FVector& ViewPoint, float HorizontalFovDegrees, float AspectRatio, float MinAhead);
}