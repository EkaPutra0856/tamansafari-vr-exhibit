// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quest/WzNarrativeScript.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

// ====== Events ====== //

void UWzNarrativeScript::OnBegin_Implementation(const FWzNarrativeContext& Context) {}

void UWzNarrativeScript::OnEnd_Implementation(const FWzNarrativeContext& Context) {}

void UWzSpawnActorsScript::OnBegin_Implementation(const FWzNarrativeContext& Context)
{
	const APawn* Pawn = Context.PlayerController ? Context.PlayerController->GetPawn() : nullptr;
	if (!Pawn) return;

	// At the pawn, even when something is in the way
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform Transform = Pawn->GetActorTransform();
	Transform.SetLocation(Transform.TransformPositionNoScale(Offset));

	// Spawn
	for (const auto& Class : Classes)
	{
		AActor* Actor = Class ? Pawn->GetWorld()->SpawnActor<AActor>(Class, Transform, Params) : nullptr;
		if (Actor) Spawned.Add(Actor);
	}
}

void UWzSpawnActorsScript::OnEnd_Implementation(const FWzNarrativeContext& Context)
{
	if (!bDestroyOnEnd) return;

	// Remove what it spawned
	for (const auto& Actor : Spawned) if (IsValid(Actor)) Actor->Destroy();
	Spawned.Reset();
}