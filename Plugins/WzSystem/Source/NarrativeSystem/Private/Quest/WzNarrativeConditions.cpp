// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quest/WzNarrativeConditions.h"

#include "Dialogue/WzDialogueSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Director/WzNarrativeSubsystem.h"
#include "Quest/WzNarrativeTarget.h"
#include "TimerManager.h"

// ====== Delay ====== //

void UWzDelayCondition::OnBegin_Implementation(const FWzNarrativeContext& Context)
{
	if (Seconds <= 0.f)
	{
		Complete();

		return;
	}

	// Complete after the delay
	World = Context.PlayerController ? Context.PlayerController->GetWorld() : nullptr;
	if (!World.IsValid()) return;

	const FTimerDelegate Done = FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		Complete();
	});
	World->GetTimerManager().SetTimer(TimerHandle, Done, Seconds, false);
}

void UWzDelayCondition::OnEnd_Implementation()
{
	if (World.IsValid()) World->GetTimerManager().ClearTimer(TimerHandle);
}

// ====== Event ====== //

void UWzEventCondition::OnBegin_Implementation(const FWzNarrativeContext& Context)
{
	Subsystem = Context.Subsystem.Get();
	if (!Subsystem.IsValid()) return;

	// Count the events
	Subsystem->OnEvent.AddUObject(this, &UWzEventCondition::HandleEvent);
	SetProgress(0, Count);
}

void UWzEventCondition::OnEnd_Implementation()
{
	if (Subsystem.IsValid()) Subsystem->OnEvent.RemoveAll(this);
}

void UWzEventCondition::HandleEvent(const FGameplayTag Event)
{
	if (Event.MatchesTag(Tag)) SetProgress(GetCurrent() + 1, GetRequired());
}

// ====== Dialogue Ended ====== //

void UWzDialogueEndedCondition::OnBegin_Implementation(const FWzNarrativeContext& Context)
{
	// Nothing to wait for
	Target = Dialogue ? Dialogue : Context.ObjectiveDialogue;
	if (!Target)
	{
		Complete();

		return;
	}

	// Wait for its end
	const ULocalPlayer* LocalPlayer = Context.PlayerController ? Context.PlayerController->GetLocalPlayer() : nullptr;
	DialogueSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UWzDialogueSubsystem>() : nullptr;
	if (DialogueSubsystem.IsValid()) DialogueSubsystem->OnDialogueEnded.AddUObject(this, &UWzDialogueEndedCondition::HandleDialogueEnded);
}

void UWzDialogueEndedCondition::OnEnd_Implementation()
{
	if (DialogueSubsystem.IsValid()) DialogueSubsystem->OnDialogueEnded.RemoveAll(this);
}

void UWzDialogueEndedCondition::HandleDialogueEnded(UWzDialogueAsset* Ended)
{
	if (Ended == Target) Complete();
}

// ====== Destroy Actors ====== //

void UWzDestroyActorsCondition::OnBegin_Implementation(const FWzNarrativeContext& Context)
{
	World = Context.PlayerController ? Context.PlayerController->GetWorld() : nullptr;
	if (!World.IsValid()) return;

	// Count the destroys
	DestroyedHandle = World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateUObject(this, &UWzDestroyActorsCondition::HandleActorDestroyed));
	SetProgress(0, Count);
}

void UWzDestroyActorsCondition::OnEnd_Implementation()
{
	if (World.IsValid()) World->RemoveOnActorDestroyedHandler(DestroyedHandle);
}

void UWzDestroyActorsCondition::HandleActorDestroyed(AActor* Actor)
{
	// Class and tag must match
	if (ActorClass && !Actor->IsA(ActorClass)) return;
	if (!Actor->Implements<UWzNarrativeTarget>()) return;

	if (IWzNarrativeTarget::Execute_GetNarrativeTags(Actor).HasTag(Tag)) SetProgress(GetCurrent() + 1, GetRequired());
}