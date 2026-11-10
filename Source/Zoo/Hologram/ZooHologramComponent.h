// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZooHologramComponent.generated.h"

class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;
class USoundBase;
class UZooHologramProfile;

DECLARE_MULTICAST_DELEGATE(FOnZooHologramAppeared);
DECLARE_MULTICAST_DELEGATE(FOnZooHologramDisappeared);

/** Where a hologram is in its life. */
UENUM(BlueprintType)
enum class EZooHologramPhase : uint8
{
	/** Invisible: before Appear, after Disappear. */
	Hidden,
	/** The reveal line climbs from the feet to the top. */
	Appearing,
	/** Fully visible. */
	Shown,
	/** The reveal line falls from the top to the feet. */
	Disappearing,
};

/** The AR look of a skinned mesh (PRESENTATION only): drives the hologram material, the Niagara effects and the sounds from a UZooHologramProfile. The owner's controller decides when to Appear, Disappear, Glitch and speak. */
UCLASS(ClassGroup = (Zoo), meta = (BlueprintSpawnableComponent))
class UZooHologramComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UZooHologramComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// ====== Events ====== //

	FOnZooHologramAppeared OnAppeared;
	FOnZooHologramDisappeared OnDisappeared;

public:
	// ====== References ====== //

	/** What this hologram plays. None = no effects: Appear and Disappear are instant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zoo|Hologram")
	TObjectPtr<UZooHologramProfile> Profile;

public:
	// ====== API ====== //

	/** Binds the skinned mesh: makes its dynamic material, measures it and (with a profile) hides it until Appear. Call it before any BeginPlay. */
	void SetTarget(USkeletalMeshComponent* InTarget);

	/** Reveals the mesh from the feet to the head. Seconds <= 0 = the profile's. */
	void Appear(float Seconds = -1.f);

	/** Dematerializes the mesh from the head to the feet, then fires OnDisappeared. Seconds <= 0 = the profile's; bLinger false skips the particle linger. */
	void Disappear(float Seconds = -1.f, bool bLinger = true);

	/** A short glitch burst (a teleport hides behind it). */
	void Glitch();

	/** Makes the glow pulse while he speaks. */
	void SetSpeaking(bool bInSpeaking);

private:
	// ====== State ====== //

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Target;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> MaterializeComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> AmbientComponent;

	UPROPERTY(Transient)
	EZooHologramPhase Phase = EZooHologramPhase::Hidden;

	/** Mesh-local unscaled height of the mesh top above its pivot. */
	float HoloHeight = 0.f;

	/** Ring radius in world units (sprite sizes ignore the component scale). */
	float RingRadius = 0.f;

	/** Seconds into the current Appearing / Disappearing phase. */
	float PhaseTime = 0.f;

	/** The current phase's length and particle linger: the profile's, or what the caller asked for. */
	float AppearSeconds = 0.f;
	float DisappearSeconds = 0.f;
	float LingerSeconds = 0.f;

	/** Reveal line position before easing: 0 = feet, 1 = top. */
	float RevealAlpha = 0.f;

	/** Seconds of glitch left. */
	float GlitchRemaining = 0.f;

	/** Seconds the spawn disc (ambient effect) still shows after the appear; 0 = not counting. */
	float AmbientRemaining = 0.f;

	/** Speaking glow, blended toward its goal (0..1). */
	float SpeakingValue = 0.f;
	bool bSpeaking = false;

public:
	FORCEINLINE EZooHologramPhase GetPhase() const { return Phase; }

private:
	// ====== Helpers ====== //

	void TickPhase(float DeltaTime);
	void TickGlitch(float DeltaTime);
	void TickSpeaking(float DeltaTime);
	void TickAmbient(float DeltaTime);

	/** True while something still needs the tick. */
	bool IsAnimating() const;

	/** Applies the reveal height (mesh units) to the material and the materialize effect. */
	void SetRevealHeight(float Height) const;

	/** Reveal height (mesh units) for a line position (0 = feet, 1 = top), eased by the profile. */
	float RevealHeightAt(float Alpha) const;

	void FinishAppear();
	void FinishDisappear();

	/** Spawns a system attached to the target with the shared user parameters set (not yet active). */
	UNiagaraComponent* SpawnEffect(UNiagaraSystem* System) const;

	/** Where the effects sit: the mesh pivot, or the ground under it when the profile asks. */
	FVector GetEffectLocation() const;

	void SpawnMaterialize(float Direction);

	/** Lets the live particles finish, then forgets the effect (it cleans itself up). */
	static void StopEffect(TObjectPtr<UNiagaraComponent>& Effect);

	void PlaySound(USoundBase* Sound) const;
};