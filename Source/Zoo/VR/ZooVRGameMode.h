// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/ZooGameMode.h"
#include "ZooVRGameMode.generated.h"

class UZooVRComponent;

/** The Zoo's rules plus VR: adds the VR component to every local player. Pawn, controller and HUD classes stay the Zoo's. */
UCLASS(Blueprintable)
class AZooVRGameMode : public AZooGameMode
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual void PostLogin(APlayerController* NewPlayer) override;

	// ====== VR ====== //

protected:
	/** The VR component added to each local player controller (a Blueprint child sets its assets). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR")
	TSubclassOf<UZooVRComponent> VRComponentClass;
};
