// Copyright Epic Games, Inc. All Rights Reserved.

#include "Duma/Narrative/ZooSpawnDumaScript.h"

#include "Duma/ZooDuma.h"
#include "Duma/ZooDumaController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogZooSpawnDuma, Log, All);

// ====== Events ====== //

void UZooSpawnDumaScript::OnBegin_Implementation(const FWzNarrativeContext& Context)
{
	const APawn* Pawn = Context.PlayerController ? Context.PlayerController->GetPawn() : nullptr;
	if (!Pawn) return;

	// He already exists (a despawn only hid him): bring him back instead of making a second one
	AZooDuma* Existing = Cast<AZooDuma>(UGameplayStatics::GetActorOfClass(Pawn->GetWorld(), AZooDuma::StaticClass()));
	if (Existing)
	{
		Duma = Existing;
		if (AZooDumaController* ExistingBrain = Existing->GetController<AZooDumaController>()) ExistingBrain->Spawn(AppearSeconds);

		return;
	}

	if (!DumaClass)
	{
		UE_LOG(LogZooSpawnDuma, Warning, TEXT("No Duma class set"));

		return;
	}

	// At the pawn, even when something is in the way
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform Transform = Pawn->GetActorTransform();
	Transform.SetLocation(Transform.TransformPositionNoScale(Offset));
	Duma = Pawn->GetWorld()->SpawnActor<AZooDuma>(DumaClass, Transform, Params);

	// His brain already possesses him and has not ticked yet: tell it how slowly to appear
	const AZooDuma* Spawned = Duma.Get();
	AZooDumaController* Brain = Spawned ? Spawned->GetController<AZooDumaController>() : nullptr;
	if (Brain) Brain->SetSpawnAppearSeconds(AppearSeconds);
}

void UZooSpawnDumaScript::OnEnd_Implementation(const FWzNarrativeContext& Context)
{
	if (!bDespawnOnEnd) return;

	// The full dematerialize, then he is hidden until the next Spawn
	const AZooDuma* Spawned = Duma.Get();
	AZooDumaController* Brain = Spawned ? Spawned->GetController<AZooDumaController>() : nullptr;
	if (Brain) Brain->Despawn();
}