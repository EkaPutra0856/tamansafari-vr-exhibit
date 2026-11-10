// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quest/WzEventDialogueScript.h"

#include "Dialogue/WzDialogueSubsystem.h"
#include "Director/WzNarrativeSubsystem.h"

// ====== Events ====== //

void UWzEventDialogueScript::OnBegin_Implementation(const FWzNarrativeContext& Context)
{
	Subsystem = Context.Subsystem.Get();
	DialogueSubsystem = UWzDialogueSubsystem::Get(Context.PlayerController);
	if (!Subsystem.IsValid()) return;

	// Listen for the events, each dialogue plays again in the next run
	Played.Reset();
	Subsystem->OnEvent.AddUObject(this, &UWzEventDialogueScript::HandleEvent);
}

void UWzEventDialogueScript::OnEnd_Implementation(const FWzNarrativeContext& Context)
{
	if (Subsystem.IsValid()) Subsystem->OnEvent.RemoveAll(this);
}

// ====== Handlers ====== //

void UWzEventDialogueScript::HandleEvent(const FGameplayTag Event)
{
	if (!DialogueSubsystem.IsValid()) return;

	// The first unplayed dialogue answering to the event
	for (const auto& Entry : Dialogues)
	{
		if (!Entry.Value || Played.Contains(Entry.Key) || !Event.MatchesTag(Entry.Key)) continue;

		// Play it
		Played.Add(Entry.Key);
		DialogueSubsystem->StartDialogue(Entry.Value);

		return;
	}
}