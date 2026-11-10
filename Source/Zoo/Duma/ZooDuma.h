// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ZooDuma.generated.h"

class UFloatingPawnMovement;
class UNiagaraComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;
class USphereComponent;
class UZooHologramComponent;

/** What Duma is doing. The controller decides; the Animation Blueprint may read it. */
UENUM(BlueprintType)
enum class EZooDumaState : uint8
{
	/** Follows the visitor, floating in front of them. */
	Flying,
	/** Hovers in place and turns to the visitor (during his dialogue). */
	Facing,
};

/** Duma, the flying guide (VESSEL): components, his state and what he can do. The brain is AZooDumaController, the AR look is UZooHologramComponent. */
UCLASS(Blueprintable)
class AZooDuma : public APawn
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AZooDuma();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;

protected:
	// ====== Components ====== //

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoo|Duma")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoo|Duma")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoo|Duma")
	TObjectPtr<UFloatingPawnMovement> Movement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoo|Duma")
	TObjectPtr<UZooHologramComponent> Hologram;

public:
	FORCEINLINE UZooHologramComponent* GetHologram() const { return Hologram; }

protected:
	// ====== Ribbons ====== //

	/** Twin ribbons trailing from the ear tips; the Niagara asset is set on the Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoo|Duma|Ribbons")
	TObjectPtr<UNiagaraComponent> RibbonLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoo|Duma|Ribbons")
	TObjectPtr<UNiagaraComponent> RibbonRight;

	/** One-shot burst at his centre (a showpiece flourish). None = no burst. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Ribbons")
	TObjectPtr<UNiagaraSystem> SparkleBurst;

	/** How far each ribbon fans outward from straight back (0 = straight back, which is end-on to a visitor behind him). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Ribbons", meta = (ClampMin = "0"))
	float RibbonOutward = 0.7f;

	/** How much each ribbon sags down, like cloth. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Ribbons", meta = (ClampMin = "0"))
	float RibbonSag = 0.35f;

	/** The speed (cm/s) at which the ribbons are at full strength. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Ribbons", meta = (ClampMin = "1"))
	float RibbonFullSpeed = 500.f;

	/** Below this speed (cm/s) there are no ribbons at all (his idle drift is about 15). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Ribbons", meta = (ClampMin = "0"))
	float RibbonMinSpeed = 30.f;

	/** Seconds the ribbons stay after he slows below the minimum, so a stop-and-go walk never cuts them. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Ribbons", meta = (ClampMin = "0"))
	float RibbonHoldSeconds = 1.2f;

	/** Seconds a trail left behind by a jump (blink, teleport) keeps fading in the world before it is removed. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Ribbons", meta = (ClampMin = "0.1"))
	float RibbonFadeSeconds = 4.f;

public:
	/** Ribbons on or off. bClear also drops the live trail (a teleport must not streak). */
	void SetRibbons(bool bOn, bool bClear = false);

	void PlaySparkleBurst() const;

private:
	/** The switch itself, without touching his speed tracking. */
	void SwitchRibbons(bool bOn, bool bClear) const;

	/** A jump: the live ribbons are detached to fade where they are, fresh ones take their place on his ears. */
	void ReleaseRibbons();

	/** Ribbon strength 0..1. */
	void SetRibbonIntensity(float Intensity) const;

	/** Aims each ribbon back, outward and down from his facing, in the world: the ear bones' own axes never cross them. */
	void UpdateRibbonTrail() const;

	/** Every frame: the ribbons show only while he really moves and a trail is allowed, stronger the faster he goes. Otherwise they fade, then switch off and clear. */
	void TickRibbons(float DeltaTime);

	/** The smoothed ribbon strength. */
	float RibbonStrength = 0.f;

	/** Seconds of hold left since he last moved, and his pace with a slow decay (for the strength). */
	float RibbonHold = 0.f;
	float RibbonPace = 0.f;

	/** Where the posed mesh was last frame, for his speed. */
	FVector LastMeshLocation = FVector::ZeroVector;
	bool bHasLastMesh = false;

	/** No trail while false (the controller says so, e.g. during his glance). */
	bool bTrailAllowed = true;

public:
	FORCEINLINE void SetTrailAllowed(const bool bAllowed) { bTrailAllowed = bAllowed; }

public:
	// ====== Performance ====== //

	/** Moves the mesh only (the actor stays the follow anchor) by a pose in actor space, pivoting on his centre. */
	void SetPerformPose(const FVector& LocalOffset, const FQuat& LocalRotation) const;

private:
	/** The mesh's relative transform as the Blueprint set it: the pose composes onto it. */
	FTransform MeshBase;

private:
	// ====== State ====== //

	UPROPERTY(Transient)
	EZooDumaState State = EZooDumaState::Flying;

public:
	FORCEINLINE void SetState(const EZooDumaState NewState) { State = NewState; }

	/** For the Animation Blueprint. */
	UFUNCTION(BlueprintPure, Category = "Zoo|Duma")
	FORCEINLINE EZooDumaState GetState() const { return State; }
};