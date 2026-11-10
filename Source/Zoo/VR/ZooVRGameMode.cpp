// Copyright Epic Games, Inc. All Rights Reserved.

#include "VR/ZooVRGameMode.h"

#include "GameFramework/PlayerController.h"
#include "VR/ZooVRComponent.h"

// ====== Lifecycle ====== //

void AZooVRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer || !NewPlayer->IsLocalController() || !VRComponentClass) return;

	// VR layer on the player (it stays idle without a headset)
	UZooVRComponent* VR = NewObject<UZooVRComponent>(NewPlayer, VRComponentClass, TEXT("ZooVR"));
	VR->RegisterComponent();
}
