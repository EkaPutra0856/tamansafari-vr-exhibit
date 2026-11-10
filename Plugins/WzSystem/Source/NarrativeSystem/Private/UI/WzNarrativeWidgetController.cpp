// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/WzNarrativeWidgetController.h"

#include "Dialogue/Data/WzDialogueAsset.h"
#include "Dialogue/Data/WzSpeakerSet.h"
#include "Dialogue/WzDialogueSubsystem.h"
#include "Director/WzNarrativeSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Quest/WzNarrativeCondition.h"
#include "WzNarrativeSettings.h"

namespace
{
	/** A task shows on the tracker when its template has a description. */
	bool IsTaskShown(const FWzObjective& Objective, const int32 TaskIndex)
	{
		return Objective.Tasks.IsValidIndex(TaskIndex) && Objective.Tasks[TaskIndex] && !Objective.Tasks[TaskIndex]->Description.IsEmpty();
	}
}

// ====== Setup ====== //

void UWzNarrativeWidgetController::BindCallbacksToDependencies()
{
	// Models
	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	DialogueSubsystem = LocalPlayer->GetSubsystem<UWzDialogueSubsystem>();
	NarrativeSubsystem = LocalPlayer->GetSubsystem<UWzNarrativeSubsystem>();
	checkf(DialogueSubsystem && NarrativeSubsystem, TEXT("The narrative widget controller needs the dialogue and narrative subsystems of its local player"));

	// Dialogue
	DialogueSubsystem->OnDialogueStarted.AddWeakLambda(this, [this](UWzDialogueAsset*)
	{
		OnDialogueActiveChanged.Broadcast(true);
	});
	DialogueSubsystem->OnDialogueEnded.AddWeakLambda(this, [this](UWzDialogueAsset*)
	{
		OnDialogueActiveChanged.Broadcast(false);
	});
	DialogueSubsystem->OnLineStarted.AddUObject(this, &UWzNarrativeWidgetController::HandleLineStarted);
	DialogueSubsystem->OnRevealChanged.AddUObject(this, &UWzNarrativeWidgetController::HandleRevealChanged);

	// Quest
	NarrativeSubsystem->OnQuestStateChanged.AddUObject(this, &UWzNarrativeWidgetController::HandleQuestStateChanged);
	NarrativeSubsystem->OnObjectiveChanged.AddWeakLambda(this, [this](const FGameplayTag Quest, int32)
	{
		if (Quest == TrackedQuest) OnObjectiveChanged.Broadcast(BuildQuestView(Quest));
	});
	NarrativeSubsystem->OnTaskChanged.AddUObject(this, &UWzNarrativeWidgetController::HandleTaskChanged);
}

void UWzNarrativeWidgetController::BroadcastInitialValues()
{
	// Quest: the tracker catches up with what is already running
	TrackedQuest = NarrativeSubsystem->GetTrackedQuest();
	if (TrackedQuest.IsValid())
	{
		AnnouncedQuests.Add(TrackedQuest);
		OnObjectiveChanged.Broadcast(BuildQuestView(TrackedQuest));
	}

	// Dialogue: catch up when one is already running
	const bool bActive = DialogueSubsystem->IsDialogueActive();
	OnDialogueActiveChanged.Broadcast(bActive);
	if (!bActive) return;

	HandleLineStarted(DialogueSubsystem->GetLineIndex());
	HandleRevealChanged(DialogueSubsystem->GetRevealedChars());
}

// ====== API ====== //

void UWzNarrativeWidgetController::Skip()
{
	DialogueSubsystem->Skip();
}

void UWzNarrativeWidgetController::EndDialogue()
{
	DialogueSubsystem->EndDialogue();
}

// ====== Handlers ====== //

void UWzNarrativeWidgetController::HandleLineStarted(const int32 LineIndex)
{
	const UWzDialogueAsset* Dialogue = DialogueSubsystem->GetActiveDialogue();
	if (!Dialogue || !Dialogue->Lines.IsValidIndex(LineIndex)) return;

	// Line
	const FWzDialogueLine& Line = Dialogue->Lines[LineIndex];
	FullString = Line.Text.ToString();

	FWzDialogueLineView View;
	View.Speaker = Line.Speaker;
	View.FullText = Line.Text;
	View.LineIndex = LineIndex;
	View.NumLines = Dialogue->Lines.Num();

	// Name, portrait and colour come from the Speaker Set; a tag not in it shows neither
	const UWzSpeakerSet* Set = GetDefault<UWzNarrativeSettings>()->SpeakerSet.LoadSynchronous();
	if (const FWzSpeakerInfo* Info = Set ? Set->Speakers.Find(Line.Speaker) : nullptr)
	{
		View.SpeakerName = Info->DisplayName;
		View.Portrait = Info->Portrait.LoadSynchronous();
		View.NameColor = Info->NameColor;
	}

	OnLineChanged.Broadcast(View);
}

void UWzNarrativeWidgetController::HandleRevealChanged(const int32 RevealedChars)
{
	// ponytail: Left() counts UTF-16 units; fine for Latin text, use grapheme breaks for complex scripts
	OnSubtitleChanged.Broadcast(FText::FromString(FullString.Left(RevealedChars)), RevealedChars >= FullString.Len());
}

void UWzNarrativeWidgetController::HandleQuestStateChanged(const FGameplayTag Quest, const EWzQuestState State)
{
	// First time active: the toast. The tracker follows through the objective event right after.
	if (State == EWzQuestState::Active && !AnnouncedQuests.Contains(Quest))
	{
		AnnouncedQuests.Add(Quest);
		TrackedQuest = Quest;
		OnQuestStarted.Broadcast(BuildQuestView(Quest));

		return;
	}

	// Released, back, or completed: the tracked quest may change
	Retrack();
}

void UWzNarrativeWidgetController::HandleTaskChanged(const FGameplayTag Quest, const int32 TaskIndex)
{
	// Only the tracked quest's tasks that the tracker shows
	const FWzObjective* Objective = Quest == TrackedQuest ? NarrativeSubsystem->GetObjective(Quest) : nullptr;
	if (!Objective || !IsTaskShown(*Objective, TaskIndex)) return;

	// The line's place among the shown ones
	int32 ShownIndex = 0;
	for (int32 Index = 0; Index < TaskIndex; ++Index) if (IsTaskShown(*Objective, Index)) ++ShownIndex;

	OnTaskChanged.Broadcast(ShownIndex, BuildTaskView(Quest, *Objective, TaskIndex));
}

// ====== Helpers ====== //

void UWzNarrativeWidgetController::Retrack()
{
	// Same quest: nothing to redraw
	const FGameplayTag Tracked = NarrativeSubsystem->GetTrackedQuest();
	if (Tracked == TrackedQuest) return;

	// Another quest, or none
	TrackedQuest = Tracked;
	if (Tracked.IsValid()) OnObjectiveChanged.Broadcast(BuildQuestView(Tracked));
	else OnTrackerCleared.Broadcast();
}

FWzQuestView UWzNarrativeWidgetController::BuildQuestView(const FGameplayTag& Quest) const
{
	FWzQuestView View;
	const UWzQuest* Found = NarrativeSubsystem->FindQuest(Quest);
	if (!Found) return View;

	// Quest
	View.QuestName = Found->DisplayName;
	View.Type = Found->Type;

	// Current objective and the tasks the tracker shows
	const FWzObjective* Objective = NarrativeSubsystem->GetObjective(Quest);
	if (!Objective) return View;

	View.ObjectiveTitle = Objective->Title;
	for (int32 Index = 0; Index < Objective->Tasks.Num(); ++Index) if (IsTaskShown(*Objective, Index)) View.Tasks.Add(BuildTaskView(Quest, *Objective, Index));

	return View;
}

FWzTaskView UWzNarrativeWidgetController::BuildTaskView(const FGameplayTag& Quest, const FWzObjective& Objective, const int32 TaskIndex) const
{
	// Text from the asset's task
	FWzTaskView View;
	View.Description = Objective.Tasks[TaskIndex]->Description;
	View.bOptional = Objective.Tasks[TaskIndex]->bOptional;

	// Progress from the running copy, when it exists (the objective may not be listening yet)
	const auto& Running = NarrativeSubsystem->GetTasks(Quest);
	if (const UWzNarrativeCondition* Task = Running.IsValidIndex(TaskIndex) ? Running[TaskIndex].Get() : nullptr)
	{
		View.bDone = Task->IsCompleted();
		View.Current = Task->GetCurrent();
		View.Required = Task->GetRequired();
	}

	return View;
}
