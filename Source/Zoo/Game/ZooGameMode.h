// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZooGameMode.generated.h"

/** Zoo rules: first-person visitor pawn, controller and HUD (the HUD's overlay widget is set on a Blueprint child of the HUD). */
UCLASS(Blueprintable)
class AZooGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AZooGameMode();
};
