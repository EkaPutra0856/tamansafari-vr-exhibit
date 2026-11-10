// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/ZooGameMode.h"

#include "Player/ZooCharacter.h"
#include "Player/ZooPlayerController.h"
#include "UI/ZooHUD.h"

AZooGameMode::AZooGameMode()
{
	DefaultPawnClass = AZooCharacter::StaticClass();
	PlayerControllerClass = AZooPlayerController::StaticClass();
	HUDClass = AZooHUD::StaticClass();
}
