// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ZooDumaController.generated.h"

class AZooDuma;
class APlayerController;
class UWzBarkComponent;
class UWzDialogueAsset;
class UWzDialogueSubsystem;
class UZooDumaShowComponent;
struct FCollisionQueryParams;

/** Why Duma's hologram blinks out: where he comes back, or that he is away until Spawn. */
enum class EZooDumaBlink : uint8
{
	None,
	/** Back at the front of the view, facing the visitor (a dialogue line). */
	ToFacing,
	/** Back at the follow spot (the dialogue ended). */
	ToFlying,
	/** Stays where he is: the show moves his mesh while he is hidden (peek-a-boo). */
	Peek,
	/** Away until Spawn: the pawn is hidden and this brain paused. */
	Despawn,
};

/** Duma's brain (BEHAVIOR): floats in front of the visitor, blinks to face them during his dialogue, performs while idle (drift, showpieces, ribbons), recovers when lost, and decides when his hologram appears, glitches, glows and goes (Despawn hides him until Spawn). His dialogues are started by the narrative (UWzNarrativeSubsystem). No Behavior Tree: two states, one rule. */
UCLASS(Blueprintable)
class AZooDumaController : public AAIController
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AZooDumaController();

	virtual void Tick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

public:
	// ====== API ====== //

	/** Dematerializes Duma from the head to the feet, then hides him and pauses this brain until Spawn. */
	UFUNCTION(BlueprintCallable, Category = "Zoo|Duma")
	void Despawn();

	/** Brings him back at the follow spot (rematerializing from where he is while he still dematerializes). Does nothing while he is here. AppearSeconds <= 0 = the hologram profile's. */
	UFUNCTION(BlueprintCallable, Category = "Zoo|Duma")
	void Spawn(float AppearSeconds = -1.f);

protected:
	// ====== Follow ====== //

	/** Where Duma floats, in the visitor's view space: X ahead, Y right, Z up. Default: the middle of the view at eye height, far enough ahead that walking never clips him. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Follow")
	FVector FollowOffset{ 220.f, 0.f, 0.f };

	/** His size in the view (cm) when keeping clear of the HUD (dialogue, list, tracker, toasts). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Follow", meta = (ClampMin = "0"))
	float ClearRadius = 45.f;

	/** Extra gap kept between him and the HUD (degrees of view). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Follow", meta = (ClampMin = "0"))
	float ClearMargin = 2.f;

	/** How snappy the follow is: the distance to the spot shrinks by exp(-Stiffness * DeltaTime) per frame, so a long frame never overshoots. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Follow", meta = (ClampMin = "0.1"))
	float FollowStiffness = 6.f;

	/** How fast Duma turns (RInterpTo speed). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Follow", meta = (ClampMin = "0"))
	float TurnSpeed = 6.f;

protected:
	// ====== Lost ====== //

	/** Duma counts as out of view beyond this angle from the view direction (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Lost", meta = (ClampMin = "1", ClampMax = "179"))
	float ViewHalfAngle = 50.f;

	/** Farther than this from the visitor counts as lost. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Lost", meta = (ClampMin = "0"))
	float LostDistance = 600.f;

	/** Seconds lost before Duma jumps back to the follow spot (only when that spot is free). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Lost", meta = (ClampMin = "0"))
	float LostSeconds = 1.5f;

protected:
	// ====== Blink ====== //

	/** Seconds to dematerialize when he blinks between flying and facing. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Blink", meta = (ClampMin = "0.05"))
	float BlinkOutSeconds = 0.25f;

	/** Seconds to rematerialize after a blink. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Blink", meta = (ClampMin = "0.05"))
	float BlinkInSeconds = 0.35f;

	/** Where he rematerializes to face the visitor, in the view space: X ahead, Y right, Z up (eye height; the HUD pushes him aside if needed). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Blink")
	FVector FacingOffset{ 160.f, 0.f, 5.f };

	/** How far he sways (cm, sideways Y and up Z) while he faces the visitor and talks. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Blink")
	FVector FacingSway{ 0.f, 9.f, 5.f };

private:
	/** Asks for a blink. A newer purpose replaces a pending one; a despawn outranks all. */
	void BeginBlink(EZooDumaBlink Purpose);

	/** Ribbons off, then the hologram dematerializes (fast for a blink, the profile's for a despawn). */
	void StartBlinkOut();

	/** Moves him without a ribbon streak behind. */
	void Relocate(AZooDuma* Duma, const FVector& Where);

protected:
	// ====== Performance ====== //

	/** The idle performance: drift and showpieces. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoo|Duma|Performance")
	TObjectPtr<UZooDumaShowComponent> Show;

	/** The performance never brings him nearer to the visitor than this (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Performance", meta = (ClampMin = "0"))
	float MinAhead = 100.f;

	/** Degrees of lean per cm/s of sideways drift. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Performance", meta = (ClampMin = "0"))
	float BankPerSpeed = 0.08f;

	/** Most he leans (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Duma|Performance", meta = (ClampMin = "0"))
	float MaxBank = 30.f;

private:
	/** Drift and showpieces move his mesh only, kept inside the view, leaning into his sideways lag; no ribbons while he glances. */
	void Perform(AZooDuma* Duma, const APlayerController* PC, const FVector& CamLoc, const FRotator& CamRot, const FCollisionQueryParams& Params, float DeltaTime);

protected:
	// ====== Bark ====== //

	/** His fun facts, said as ambient dialogues while he is idle (the facts are set on the Blueprint). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoo|Duma|Bark")
	TObjectPtr<UWzBarkComponent> Bark;

private:
	// ====== Dialogue ====== //

	void SetFacing(bool bFacing);
	void SetSpeaking(bool bSpeaking);

	void HandleLineStarted(int32 LineIndex);
	void HandleDialogueEnded(UWzDialogueAsset* Dialogue);

private:
	// ====== Hologram ====== //

	/** His hologram finished rematerializing: the blink that was waiting starts. */
	void HandleAppeared();

	/** His hologram finished dematerializing: a despawn hides him until Spawn, a blink brings him back. */
	void HandleDisappeared();

private:
	// ====== View ====== //

	/** A point in the visitor's view space (X ahead, Y right, Z up) as a world location, moved clear of the HUD. False without a visitor. */
	bool GetViewSpot(const FVector& ViewOffset, FVector& OutSpot);

	/** The view offset shifted (up, then sideways) until no HUD element covers him; the distance is kept. */
	FVector KeepClearOfHUD(const FVector& ViewOffset, const APlayerController* PC);

	/** The shift that last kept him clear, tried first so he does not hop between free spots. */
	FVector2D LastClearShift = FVector2D::ZeroVector;

	/** Where he looks: at the visitor while facing, ahead (the way the visitor looks) while flying. */
	FRotator GetWantedRotation(const AZooDuma* Duma, const FVector& CamLoc, const FRotator& CamRot) const;

	/** Turns him at once to where he wants to look (after a teleport, so he never spins in view). */
	void SnapTurn(AZooDuma* Duma) const;

private:
	// ====== State ====== //

	/** The blink asked for or under way; it may be held back until the first appear ends. */
	EZooDumaBlink Blink = EZooDumaBlink::None;

	/** Dematerializing for a blink right now. */
	bool bBlinkOut = false;

	/** The first appear (the slow one) is still running: no blink cuts it. */
	bool bSpawnAppearing = false;

	bool bPlaced = false;

	/** Hidden and paused by a Despawn until Spawn. */
	bool bDespawned = false;

	float LostTime = 0.f;

	/** The first appear's seconds; the quest sets it. <= 0 = the hologram profile's. */
	float SpawnAppearSeconds = -1.f;

	/** For the lean: where the actor was last frame. */
	FVector LastLoc = FVector::ZeroVector;
	bool bHasLast = false;

	float Bank = 0.f;

public:
	FORCEINLINE void SetSpawnAppearSeconds(const float Seconds) { SpawnAppearSeconds = Seconds; }

	FORCEINLINE bool IsDespawned() const { return bDespawned; }
};