// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dialogue/Camera/WzDialogueCameraEffect.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

// ====== Events ====== //

void UWzDialogueCameraEffect::BeginEffect_Implementation(APlayerController*, AActor*) {}

void UWzDialogueCameraEffect::TickEffect_Implementation(APlayerController*, AActor*, float) {}

void UWzDialogueCameraEffect::EndEffect_Implementation(APlayerController*, AActor*) {}

void UWzLookAtSpeakerCameraEffect::TickEffect_Implementation(APlayerController* PlayerController, AActor* Speaker, const float DeltaTime)
{
	if (!PlayerController || !Speaker) return;

	// Eye and camera points
	FVector EyeLocation;
	FRotator EyeRotation;
	Speaker->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	// Aim at the eyes, no roll, eased
	FRotator Desired = (EyeLocation + TargetOffset - CameraLocation).Rotation();
	Desired.Roll = 0.f;
	PlayerController->SetControlRotation(FMath::RInterpTo(PlayerController->GetControlRotation(), Desired, DeltaTime, InterpSpeed));
}