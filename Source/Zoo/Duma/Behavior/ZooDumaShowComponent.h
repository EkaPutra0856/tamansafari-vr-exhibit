// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Duma/Behavior/ZooDumaMotion.h"
#include "ZooDumaShowComponent.generated.h"

/** A showpiece reached the point where Duma bursts into sparkles. */
DECLARE_MULTICAST_DELEGATE(FOnZooDumaBurst);

/** The peek-a-boo needs Duma to blink out now (answer with NotifyBlinkHidden once he is hidden). */
DECLARE_MULTICAST_DELEGATE(FOnZooDumaPeekBlink);

/** Duma's idle performance (MOTION only): an always-on drift plus a random showpiece every few seconds. The controller moves the mesh with GetPose(); it also decides when this is allowed. */
UCLASS(ClassGroup = (Zoo), meta = (BlueprintSpawnableComponent))
class UZooDumaShowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UZooDumaShowComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// ====== Events ====== //

	FOnZooDumaBurst OnBurst;
	FOnZooDumaPeekBlink OnPeekBlink;

protected:
	// ====== Tuning ====== //

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Show")
	FZooDumaDriftSettings Drift;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Show")
	TMap<EZooDumaShowpiece, FZooDumaShowpieceSettings> Showpieces;

	/** Seconds between two showpieces: a random gap in this range. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Show", meta = (ClampMin = "0"))
	float MinGap = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Show", meta = (ClampMin = "0"))
	float MaxGap = 12.f;

	/** Each play is this much (+/-, fraction) bigger or smaller, faster or slower. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Show", meta = (ClampMin = "0", ClampMax = "0.9"))
	float Jitter = 0.2f;

	/** Seconds to fade the whole performance in or out when it is allowed or not. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Show", meta = (ClampMin = "0.05"))
	float BlendSeconds = 0.3f;

	/** A peek gives up when the blink does not come within this many seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Show", meta = (ClampMin = "0.1"))
	float PeekBlinkTimeout = 1.5f;

	/** The "checking on the visitor" glance: its own, shorter timer between the showpieces. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Show")
	FZooDumaGlanceSettings Glance;

public:
	// ====== API ====== //

	/** The performance may run (flying and settled). Not allowed: it fades out and the running showpiece ends. */
	void SetAllowed(bool bInAllowed);

	/** The current pose in view space, drift plus showpiece, faded by the allowed blend. */
	FZooDumaPose GetPose(const FVector& FollowOffset) const;

	/** Ends the running showpiece with a fade (e.g. the view is blocked). */
	void Abort();

	/** Answer to OnPeekBlink: Duma is hidden now. */
	void NotifyBlinkHidden();

private:
	// ====== State ====== //

	enum class EPeekStage : uint8
	{
		/** Waiting for the blink out at home. */
		Out,
		/** At the side, wiggling. */
		Side,
		/** Waiting for the blink out at the side. */
		Home,
	};

	bool bAllowed = false;
	bool bPerforming = false;
	bool bAborting = false;
	bool bBurstDone = false;
	bool bHasLast = false;
	bool bWaitingBlink = false;
	bool bGlancing = false;
	float GlanceMirror = 1.f;
	EZooDumaShowpiece Current = EZooDumaShowpiece::Loop;
	EZooDumaShowpiece Last = EZooDumaShowpiece::Loop;
	EPeekStage Stage = EPeekStage::Out;
	FZooDumaVariation Variation;

	/** Seconds into the showpiece, or into the peek's current stage. */
	float Elapsed = 0.f;
	float BlinkWait = 0.f;

	/** Drift clock. */
	float Clock = 0.f;

	/** Seconds until the next showpiece. */
	float NextShowIn = 0.f;

	/** Seconds until the next glance, and seconds into the running one. */
	float NextGlanceIn = 0.f;
	float GlanceElapsed = 0.f;

	/** The allowed blend, 0..1. */
	float Weight = 0.f;

public:
	FORCEINLINE bool IsPerforming() const { return bPerforming; }
	FORCEINLINE bool IsGlancing() const { return bGlancing; }

private:
	// ====== Schedule ====== //

	/** Picks the gaps to the next showpiece and the next glance. */
	void ScheduleNext();

	/** Picks the gap to the next glance. */
	void ScheduleNextGlance();

	/** Weighted random pick (never the same twice in a row) and its variation. */
	void StartShowpiece();

	void EndShowpiece();

private:
	// ====== Glance ====== //

	void StartGlance();

	void EndGlance();

private:
	// ====== Showpiece ====== //

	void TickShowpiece(float DeltaTime);

	/** The peek's own flow: blink out, wiggle at the side, blink home. */
	void TickPeek(float DeltaTime, const FZooDumaShowpieceSettings& Settings);

	/** Seconds the running showpiece lasts (with its variation). */
	float GetDuration(const FZooDumaShowpieceSettings& Settings) const;

	/** The running showpiece's pose in view space. */
	FZooDumaPose GetShowpiecePose(const FVector& FollowOffset) const;
};