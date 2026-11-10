// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZooCharacter.generated.h"

class UCameraComponent;

/** First-person visitor: realistic walking pace, a sprint on demand (with a small FOV kick), a small hop, no head bob. */
UCLASS(Blueprintable)
class AZooCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AZooCharacter();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;

public:
	// ====== Movement ====== //

	void Move(const FVector2D& Input);
	void Look(const FVector2D& Input);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Movement", meta = (ClampMin = "10"))
	float WalkSpeed = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Movement", meta = (ClampMin = "10"))
	float SprintSpeed = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Movement", meta = (ClampMin = "0"))
	float JumpVelocity = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Movement", meta = (ClampMin = "0"))
	float MaxAcceleration = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Movement", meta = (ClampMin = "0"))
	float BrakingDeceleration = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float AirControl = 0.2f;

private:
	/** Walk or sprint speed for the current state. */
	void ApplySpeed();

	bool bSprinting = false;

public:
	/** Sprint speed and the FOV kick while true, the walk otherwise. */
	void SetSprinting(bool bInSprinting);

protected:
	// ====== Camera ====== //

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoo|Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Camera", meta = (ClampMin = "100"))
	float EyeHeight = 165.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Camera", meta = (ClampMin = "10"))
	float CapsuleRadius = 34.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Camera", meta = (ClampMin = "30"))
	float CapsuleHalfHeight = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Camera", meta = (ClampMin = "0.05"))
	float LookSensitivity = 1.f;

	/** Degrees added to the FOV while sprinting. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Camera", meta = (ClampMin = "0"))
	float SprintFovKick = 5.f;

	/** How fast the FOV follows the sprint (FInterpTo speed). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Camera", meta = (ClampMin = "0.1"))
	float SprintFovSpeed = 8.f;

private:
	/** Capsule, eye height and movement from the tunables. */
	void ApplySettings();

	/** The FOV the camera has without a sprint (the Blueprint's), cached once. */
	float BaseFov = 90.f;
};
