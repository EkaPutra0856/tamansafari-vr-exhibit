// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/World/PathGuideDestination.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

// ====== Lifecycle ====== //

AWzPathGuideDestination::AWzPathGuideDestination()
{
	// No tick, never drawn in game
	PrimaryActorTick.bCanEverTick = false;
	AActor::SetActorHiddenInGame(true);

	// Root
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	// Editor sprite
	if (UBillboardComponent* Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard")))
	{
		Billboard->SetupAttachment(Root);
		Billboard->bIsScreenSizeScaled = true;
		if (!IsRunningCommandlet())
		{
			static ConstructorHelpers::FObjectFinderOptional<UTexture2D> Sprite(TEXT("/Engine/EditorResources/S_TargetPoint"));
			if (Sprite.Get()) Billboard->SetSprite(Sprite.Get());
		}
	}

	// Editor arrow (the facing on arrival)
	if (UArrowComponent* Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow")))
	{
		Arrow->SetupAttachment(Root);
		Arrow->ArrowSize = 1.5f;
		Arrow->bIsScreenSizeScaled = true;
	}
#endif
}
