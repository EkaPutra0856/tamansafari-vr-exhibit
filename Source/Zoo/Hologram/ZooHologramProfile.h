// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ZooHologramProfile.generated.h"

class UCurveFloat;
class UNiagaraSystem;
class USoundBase;

/** Everything one AR creature's hologram plays: timings, effects and sounds. Pure data, read by UZooHologramComponent. */
UCLASS(BlueprintType)
class UZooHologramProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	// ====== Timing ====== //

	/** Seconds the reveal band takes from the feet to the top. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Timing", meta = (ClampMin = "0.1"))
	float AppearSeconds = 1.8f;

	/** Seconds the reveal band takes from the top down to the feet. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Timing", meta = (ClampMin = "0.1"))
	float DisappearSeconds = 1.4f;

	/** Seconds the effects get to finish after the band ends, so the last particles are not cut off. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Timing", meta = (ClampMin = "0"))
	float LingerSeconds = 0.8f;

	/** Seconds one glitch burst lasts (it fades out over this time). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Timing", meta = (ClampMin = "0.05"))
	float GlitchSeconds = 0.35f;

public:
	// ====== Reveal ====== //

	/** Eases the reveal progress (0..1 in, 0..1 out). None = smoothstep. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Reveal")
	TObjectPtr<UCurveFloat> RevealCurve;

	/** How far the reveal line ends above the mesh top (mesh units), so the glow band leaves the mesh. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Reveal", meta = (ClampMin = "0"))
	float BandOvershoot = 10.f;

	/** Ring radius = widest half-extent of the mesh times this. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Reveal", meta = (ClampMin = "0.1"))
	float RingRadiusScale = 1.3f;

	/** The ring never grows past this (world cm), however big the mesh is scaled. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Reveal", meta = (ClampMin = "10"))
	float MaxRingRadius = 160.f;

	/** The ring and the hover disc sit on the ground under the mesh (scanning the terrain) instead of at its pivot. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Reveal")
	bool bRingOnGround = false;

	/** How far below the pivot the ground is looked for (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Reveal", meta = (ClampMin = "0"))
	float GroundSearch = 800.f;

public:
	// ====== Effects ====== //

	/** Ring + assembling particles, played while he appears or disappears. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Effects")
	TObjectPtr<UNiagaraSystem> MaterializeEffect;

	/** Hover disc + data motes, played from the appear. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Effects")
	TObjectPtr<UNiagaraSystem> AmbientEffect;

	/** Seconds the hover disc stays after the appear completes (a spawn ring, not a permanent one); 0 or less = until the disappear. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Effects", meta = (ClampMin = "0"))
	float AmbientSeconds = 2.5f;

	/** Accent color handed to the effects (the material keeps its own tint). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Effects")
	FLinearColor AccentTint{ 0.35f, 0.85f, 1.f };

	/** Custom stencil value this animal writes (1 Duma, 2 Giraffe, 3 Wombat): the camera halo reads it to pick the animal's color. 0 = default halo color. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Effects", meta = (ClampMin = "0", ClampMax = "255"))
	uint8 HaloStencil = 0;

public:
	// ====== Sounds ====== //

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Sounds")
	TObjectPtr<USoundBase> AppearSound;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Sounds")
	TObjectPtr<USoundBase> DisappearSound;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Sounds")
	TObjectPtr<USoundBase> GlitchSound;

public:
	// ====== Speaking ====== //

	/** How fast the speaking glow blends in and out. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Hologram|Speaking", meta = (ClampMin = "0.1"))
	float SpeakingBlendSpeed = 6.f;
};