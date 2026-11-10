// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/ZooCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// ====== Lifecycle ====== //

AZooCharacter::AZooCharacter()
{
	// Ticks only while the FOV is blending
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->bUsePawnControlRotation = true;

	GetCharacterMovement()->bOrientRotationToMovement = false;

	ApplySettings();
}

void AZooCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// The Blueprint's FOV is the one the kick starts from
	BaseFov = Camera->FieldOfView;
	ApplySettings();
}

void AZooCharacter::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Ease toward the sprint or the base FOV, and sleep once there
	const float TargetFov = bSprinting ? BaseFov + SprintFovKick : BaseFov;
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFov, DeltaTime, SprintFovSpeed));
	if (FMath::IsNearlyEqual(Camera->FieldOfView, TargetFov, 0.01f))
	{
		Camera->SetFieldOfView(TargetFov);
		SetActorTickEnabled(false);
	}
}

// ====== Movement ====== //

void AZooCharacter::Move(const FVector2D& Input)
{
	if (!Controller) return;

	const FRotator YawRotation{ 0.0, Controller->GetControlRotation().Yaw, 0.0 };
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

void AZooCharacter::Look(const FVector2D& Input)
{
	AddControllerYawInput(Input.X * LookSensitivity);
	AddControllerPitchInput(Input.Y * LookSensitivity);
}

void AZooCharacter::ApplySpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
}

void AZooCharacter::SetSprinting(const bool bInSprinting)
{
	bSprinting = bInSprinting;
	ApplySpeed();

	// The FOV starts easing
	SetActorTickEnabled(true);
}

// ====== Camera ====== //

void AZooCharacter::ApplySettings()
{
	GetCapsuleComponent()->SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	Camera->SetRelativeLocation({ 0.0, 0.0, EyeHeight - CapsuleHalfHeight });

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->JumpZVelocity = JumpVelocity;
	Movement->MaxAcceleration = MaxAcceleration;
	Movement->BrakingDecelerationWalking = BrakingDeceleration;
	Movement->AirControl = AirControl;
	ApplySpeed();
}