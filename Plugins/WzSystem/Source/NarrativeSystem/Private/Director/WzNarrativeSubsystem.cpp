// Copyright Epic Games, Inc. All Rights Reserved.

#include "Director/WzNarrativeSubsystem.h"

#include "Dialogue/Data/WzDialogueAsset.h"
#include "Dialogue/WzDialogueSubsystem.h"
#include "Director/WzNarrativeDirector.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NarrativeSystem.h"
#include "Quest/WzNarrativeCondition.h"
#include "Quest/WzNarrativeConditions.h"
#include "Quest/WzNarrativeScript.h"
#include "Quest/WzQuestLogic.h"
#include "TimerManager.h"
#include "WzNarrativeSettings.h"

namespace
{
	/** Runtime copies of the asset's templates, so progress never writes into the asset. Empty slots are skipped. */
	template <typename T>
	TArray<TObjectPtr<T>> MakeCopies(const TArray<TObjectPtr<T>>& Templates, UObject* Outer)
	{
		TArray<TObjectPtr<T>> Copies;
		for (const auto& Template : Templates) if (Template) Copies.Add(DuplicateObject(Template.Get(), Outer));

		return Copies;
	}

	/** Stops listening on every task. */
	void EndAll(const TArray<TObjectPtr<UWzNarrativeCondition>>& Tasks)
	{
		for (const auto& Task : Tasks) if (Task) Task->End();
	}
}

// ====== Lifecycle ====== //

void UWzNarrativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// A dialogue ending finishes the objectives waiting on their Complete Dialogue
	Collection.InitializeDependency<UWzDialogueSubsystem>();
	if (UWzDialogueSubsystem* Dialogue = GetDialogue()) DialogueEndedHandle = Dialogue->OnDialogueEnded.AddUObject(this, &UWzNarrativeSubsystem::HandleDialogueEnded);
}

void UWzNarrativeSubsystem::Deinitialize()
{
	if (APlayerController* PC = BoundController.Get()) PC->OnPossessedPawnChanged.RemoveDynamic(this, &UWzNarrativeSubsystem::HandlePawnChanged);
	if (UWzDialogueSubsystem* Dialogue = GetDialogue()) Dialogue->OnDialogueEnded.Remove(DialogueEndedHandle);
	TearDown();

	Super::Deinitialize();
}

void UWzNarrativeSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	// A new controller means a new level (or a first start): run again, the progress stays
	if (APlayerController* Old = BoundController.Get()) Old->OnPossessedPawnChanged.RemoveDynamic(this, &UWzNarrativeSubsystem::HandlePawnChanged);
	TearDown();
	BoundController = NewPlayerController;
	bStarted = false;
	if (!NewPlayerController) return;

	// Starts once the player has a pawn (spawned actors appear at it)
	if (NewPlayerController->GetPawn()) BeginNarrative();
	else NewPlayerController->OnPossessedPawnChanged.AddDynamic(this, &UWzNarrativeSubsystem::HandlePawnChanged);
}

// ====== API ====== //

void UWzNarrativeSubsystem::SendEvent(const FGameplayTag Event)
{
	OnEvent.Broadcast(Event);
}

// ====== Getters ====== //

EWzQuestState UWzNarrativeSubsystem::GetQuestState(const FGameplayTag& Quest) const
{
	const FWzQuestProgress* Progress = State.Quests.Find(Quest);

	return Progress ? Progress->State : EWzQuestState::Locked;
}

bool UWzNarrativeSubsystem::IsQuestCompleted(const FGameplayTag& Quest) const
{
	return GetQuestState(Quest) == EWzQuestState::Completed;
}

FGameplayTag UWzNarrativeSubsystem::GetTrackedQuest() const
{
	if (!Director) return {};

	// The active quest wins, else the first released one
	FGameplayTag FirstReleased;
	for (const auto& Quest : Director->Quests)
	{
		if (!Quest) continue;

		const EWzQuestState QuestState = GetQuestState(Quest->QuestTag);
		if (QuestState == EWzQuestState::Active) return Quest->QuestTag;
		if (QuestState == EWzQuestState::Released && !FirstReleased.IsValid()) FirstReleased = Quest->QuestTag;
	}

	return FirstReleased;
}

UWzQuest* UWzNarrativeSubsystem::FindQuest(const FGameplayTag& Quest) const
{
	return Director ? Director->FindQuest(Quest) : nullptr;
}

int32 UWzNarrativeSubsystem::GetObjectiveIndex(const FGameplayTag& Quest) const
{
	const FWzQuestProgress* Progress = State.Quests.Find(Quest);

	return Progress ? Progress->ObjectiveIndex : INDEX_NONE;
}

const FWzObjective* UWzNarrativeSubsystem::GetObjective(const FGameplayTag& Quest) const
{
	const UWzQuest* Found = FindQuest(Quest);
	const int32 Index = GetObjectiveIndex(Quest);

	return Found && Found->Objectives.IsValidIndex(Index) ? &Found->Objectives[Index] : nullptr;
}

const TArray<TObjectPtr<UWzNarrativeCondition>>& UWzNarrativeSubsystem::GetTasks(const FGameplayTag& Quest) const
{
	static const TArray<TObjectPtr<UWzNarrativeCondition>> None;
	const FWzQuestRuntime* Runtime = Runtimes.Find(Quest);

	return Runtime ? Runtime->Tasks : None;
}

// ====== Helpers ====== //

UWzDialogueSubsystem* UWzNarrativeSubsystem::GetDialogue() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();

	return LocalPlayer ? LocalPlayer->GetSubsystem<UWzDialogueSubsystem>() : nullptr;
}

UWorld* UWzNarrativeSubsystem::GetPlayerWorld() const
{
	return BoundController.IsValid() ? BoundController->GetWorld() : nullptr;
}

FWzNarrativeContext UWzNarrativeSubsystem::MakeContext(UWzDialogueAsset* ObjectiveDialogue)
{
	FWzNarrativeContext Context;
	Context.Subsystem = this;
	Context.PlayerController = BoundController.Get();
	Context.ObjectiveDialogue = ObjectiveDialogue;

	return Context;
}

bool UWzNarrativeSubsystem::HasActiveQuest() const
{
	for (const auto& Pair : State.Quests) if (Pair.Value.State == EWzQuestState::Active) return true;

	return false;
}

bool UWzNarrativeSubsystem::IsCurrentObjectiveDone(const FGameplayTag& Quest) const
{
	const FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	if (!Runtime) return false;

	// Required tasks only
	TArray<FWzTaskStatus> Statuses;
	for (const auto& Task : Runtime->Tasks) Statuses.Add({ Task->bOptional, Task->IsCompleted() });

	return !Statuses.IsEmpty() && WzQuestLogic::IsObjectiveDone(Statuses);
}

void UWzNarrativeSubsystem::SetQuestState(const FGameplayTag& Quest, const EWzQuestState NewState)
{
	FWzQuestProgress* Progress = State.Quests.Find(Quest);
	if (!Progress || Progress->State == NewState) return;

	// Change and announce
	Progress->State = NewState;
	UE_LOG(LogWzNarrative, Log, TEXT("Quest %s -> %s"), *Quest.ToString(), *UEnum::GetValueAsString(NewState));
	OnQuestStateChanged.Broadcast(Quest, NewState);
}

// ====== Session ====== //

void UWzNarrativeSubsystem::BeginNarrative()
{
	APlayerController* PC = BoundController.Get();
	if (bStarted || !PC || !PC->GetPawn()) return;

	// The pawn is here: stop waiting for it
	bStarted = true;
	PC->OnPossessedPawnChanged.RemoveDynamic(this, &UWzNarrativeSubsystem::HandlePawnChanged);

	Director = GetDefault<UWzNarrativeSettings>()->Director.LoadSynchronous();
	if (!Director) return;

	// Every quest gets its progress and runtime now, so later lookups never add (references stay valid)
	Runtimes.Reset();
	for (const auto& Quest : Director->Quests)
	{
		if (!Quest || !Quest->QuestTag.IsValid()) continue;

		State.Quests.FindOrAdd(Quest->QuestTag);
		Runtimes.Add(Quest->QuestTag);
	}

	// Pick up what a level change interrupted: scripts again, the objective from its start
	for (const auto& Quest : Director->Quests)
	{
		if (!Quest) continue;

		const FGameplayTag Tag = Quest->QuestTag;
		const EWzQuestState QuestState = GetQuestState(Tag);
		if (QuestState != EWzQuestState::Active && QuestState != EWzQuestState::Released) continue;

		StartQuestScripts(Tag);
		BeginObjective(Tag, GetObjectiveIndex(Tag));
	}

	// Decide who starts next frame: the pawn is possessed before the HUD and the game glue exist
	GetPlayerWorld()->GetTimerManager().SetTimerForNextTick(this, &UWzNarrativeSubsystem::Refresh);
}

void UWzNarrativeSubsystem::TearDown()
{
	UWorld* World = GetPlayerWorld();
	for (auto& Pair : Runtimes)
	{
		// Timers and listeners go; the scripts are dropped without ending (their quest did not end) but stop hearing events
		FWzQuestRuntime& Runtime = Pair.Value;
		for (const auto& Script : Runtime.QuestScripts) OnEvent.RemoveAll(Script);
		for (const auto& Script : Runtime.ObjectiveScripts) OnEvent.RemoveAll(Script);
		if (World) World->GetTimerManager().ClearTimer(Runtime.IntroTimer);
		if (Runtime.Trigger) Runtime.Trigger->End();
		EndAll(Runtime.Tasks);
		Runtime = FWzQuestRuntime{};
	}
}

void UWzNarrativeSubsystem::Refresh()
{
	if (bRefreshing)
	{
		bRefreshAgain = true;

		return;
	}

	// Run again while something changed during a pass
	TGuardValue<bool> Guard(bRefreshing, true);
	do
	{
		bRefreshAgain = false;
		RefreshOnce();
	}
	while (bRefreshAgain);
}

void UWzNarrativeSubsystem::RefreshOnce()
{
	if (!Director) return;

	// Unlock the quests whose prerequisites are all completed
	TSet<FGameplayTag> Completed;
	for (const auto& Pair : State.Quests) if (Pair.Value.State == EWzQuestState::Completed) Completed.Add(Pair.Key);

	for (const auto& Quest : Director->Quests)
	{
		if (!Quest || GetQuestState(Quest->QuestTag) != EWzQuestState::Locked) continue;

		if (WzQuestLogic::IsUnlocked(Quest->Prerequisites, Completed)) SetQuestState(Quest->QuestTag, EWzQuestState::Ready);
	}

	// While no quest is active, Ready quests wait for their start condition and Released ones for their tasks
	for (const auto& Quest : Director->Quests)
	{
		if (!Quest) continue;

		// Read again each time: an earlier quest may have taken the slot
		const FGameplayTag Tag = Quest->QuestTag;
		const bool bSlotFree = !HasActiveQuest();
		const EWzQuestState QuestState = GetQuestState(Tag);
		if (QuestState == EWzQuestState::Ready)
		{
			if (bSlotFree && !Quest->StartCondition) ActivateQuest(Tag);
			else SetTriggerListening(Tag, bSlotFree);
		}
		else if (QuestState == EWzQuestState::Released) SetTasksListening(Tag, bSlotFree);
	}
}

// ====== Listening ====== //

void UWzNarrativeSubsystem::SetTriggerListening(const FGameplayTag& Quest, const bool bListen)
{
	FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	const UWzQuest* Found = FindQuest(Quest);
	if (!Runtime || !Found) return;

	// Stop (also when the quest has no start condition)
	if (!bListen || !Found->StartCondition)
	{
		if (Runtime->Trigger) Runtime->Trigger->End();
		Runtime->Trigger = nullptr;

		return;
	}
	if (Runtime->Trigger) return;

	// Listen with a copy: the asset's condition is only a template
	UWzNarrativeCondition* Trigger = DuplicateObject(Found->StartCondition.Get(), this);
	Runtime->Trigger = Trigger;
	Trigger->OnChanged.AddUObject(this, &UWzNarrativeSubsystem::HandleTriggerChanged, Quest);
	Trigger->Begin(MakeContext(nullptr));
	OnListening.Broadcast();
}

void UWzNarrativeSubsystem::SetTasksListening(const FGameplayTag& Quest, const bool bListen)
{
	FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	if (!Runtime) return;

	// Stop: the copies are dropped, listening again starts fresh
	if (!bListen)
	{
		EndAll(Runtime->Tasks);
		Runtime->Tasks.Reset();

		return;
	}

	const FWzObjective* Objective = GetObjective(Quest);
	if (!Objective || !Runtime->bIntroDone || !Runtime->Tasks.IsEmpty()) return;

	// Copies of the asset's tasks
	Runtime->Tasks = MakeCopies(Objective->Tasks, this);

	// No required task: the Start Dialogue ending finishes the objective
	bool bHasRequired = false;
	for (const auto& Task : Runtime->Tasks) bHasRequired |= !Task->bOptional;
	if (!bHasRequired)
	{
		UWzDialogueEndedCondition* Implicit = NewObject<UWzDialogueEndedCondition>(this);
		Implicit->Dialogue = Runtime->IntroDialogue;
		Runtime->Tasks.Add(Implicit);
	}

	// Bind all, then begin: a task can finish the objective while beginning
	const TArray<TObjectPtr<UWzNarrativeCondition>> Listening = Runtime->Tasks;
	for (const auto& Task : Listening) Task->OnChanged.AddUObject(this, &UWzNarrativeSubsystem::HandleTaskChanged, Quest);

	const FWzNarrativeContext Context = MakeContext(Runtime->IntroDialogue);
	for (const auto& Task : Listening) Task->Begin(Context);
	OnListening.Broadcast();
}

// ====== Scripts ====== //

void UWzNarrativeSubsystem::StartQuestScripts(const FGameplayTag& Quest)
{
	FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	const UWzQuest* Found = FindQuest(Quest);
	if (!Runtime || !Found) return;

	BeginScripts(Found->Scripts, Runtime->QuestScripts);
}

void UWzNarrativeSubsystem::BeginScripts(const TArray<TObjectPtr<UWzNarrativeScript>>& Templates, TArray<TObjectPtr<UWzNarrativeScript>>& Running)
{
	// Copies of the asset's scripts, then begin (iterating a snapshot: a script may start others)
	Running = MakeCopies(Templates, this);
	const FWzNarrativeContext Context = MakeContext(nullptr);
	const TArray<TObjectPtr<UWzNarrativeScript>> Scripts = Running;
	for (const auto& Script : Scripts) Script->OnBegin(Context);
}

void UWzNarrativeSubsystem::EndScripts(TArray<TObjectPtr<UWzNarrativeScript>>& Scripts)
{
	const FWzNarrativeContext Context = MakeContext(nullptr);
	for (const auto& Script : Scripts) if (Script) Script->OnEnd(Context);
	Scripts.Reset();
}

// ====== Quest Flow ====== //

void UWzNarrativeSubsystem::ActivateQuest(const FGameplayTag& Quest)
{
	// The start condition did its job
	SetTriggerListening(Quest, false);
	SetQuestState(Quest, EWzQuestState::Active);

	// Quest scripts, then the first objective
	StartQuestScripts(Quest);
	BeginObjective(Quest, 0);

	// The slot is taken: the others stop listening
	Refresh();
}

void UWzNarrativeSubsystem::BeginObjective(const FGameplayTag& Quest, const int32 Index)
{
	const UWzQuest* Found = FindQuest(Quest);
	FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	FWzQuestProgress* Progress = State.Quests.Find(Quest);
	if (!Found || !Runtime || !Progress) return;

	// No such objective: the quest is done
	if (!Found->Objectives.IsValidIndex(Index))
	{
		CompleteQuest(Quest);

		return;
	}

	// Start fresh (the previous objective's done tasks were kept for the tracker until now)
	Progress->ObjectiveIndex = Index;
	const FWzObjective& Objective = Found->Objectives[Index];
	Runtime->Tasks.Reset();
	Runtime->bIntroDone = false;
	Runtime->IntroDialogue = nullptr;
	Runtime->PendingDialogue = nullptr;

	BeginScripts(Objective.Scripts, Runtime->ObjectiveScripts);

	// Announce, and free the slot when the objective releases the quest
	OnObjectiveChanged.Broadcast(Quest, Index);
	if (Objective.bReleasesQuest)
	{
		SetQuestState(Quest, EWzQuestState::Released);
		Refresh();
	}

	// The intro starts after the delay
	UWorld* World = GetPlayerWorld();
	if (Objective.StartDelay <= 0.f || !World)
	{
		FinishIntro(Quest);

		return;
	}

	const FTimerDelegate Intro = FTimerDelegate::CreateUObject(this, &UWzNarrativeSubsystem::FinishIntro, Quest);
	World->GetTimerManager().SetTimer(Runtime->IntroTimer, Intro, Objective.StartDelay, false);
}

void UWzNarrativeSubsystem::FinishIntro(const FGameplayTag Quest)
{
	FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	const FWzObjective* Objective = GetObjective(Quest);
	if (!Runtime || !Objective) return;

	// The Start Dialogue (a releasing objective is not waited on: the quest is away)
	UWzDialogueSubsystem* Dialogue = GetDialogue();
	const bool bDialogueStarted = Objective->StartDialogue && Dialogue && Dialogue->StartDialogue(Objective->StartDialogue);
	Runtime->IntroDialogue = bDialogueStarted && !Objective->bReleasesQuest ? Objective->StartDialogue.Get() : nullptr;
	Runtime->bIntroDone = true;

	// An active quest's tasks listen right away; a released one's wait for the free slot
	if (GetQuestState(Quest) == EWzQuestState::Active) SetTasksListening(Quest, true);
	Refresh();
}

void UWzNarrativeSubsystem::CompleteObjective(const FGameplayTag& Quest)
{
	FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	const FWzObjective* Objective = GetObjective(Quest);
	if (!Runtime || !Objective || Runtime->PendingDialogue) return;

	// Tasks done: stop listening but keep them for the tracker, and a released quest is back
	EndAll(Runtime->Tasks);
	if (GetQuestState(Quest) == EWzQuestState::Released) SetQuestState(Quest, EWzQuestState::Active);

	// The Complete Dialogue, then finish (nothing to wait for when it does not start)
	UWzDialogueSubsystem* Dialogue = GetDialogue();
	if (Objective->CompleteDialogue && Dialogue && Dialogue->StartDialogue(Objective->CompleteDialogue)) Runtime->PendingDialogue = Objective->CompleteDialogue;
	else FinishObjective(Quest);

	// Active again: the others stop listening
	Refresh();
}

void UWzNarrativeSubsystem::FinishObjective(const FGameplayTag& Quest)
{
	FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	if (!Runtime) return;

	// End its scripts, then the next objective
	Runtime->PendingDialogue = nullptr;
	EndScripts(Runtime->ObjectiveScripts);
	BeginObjective(Quest, GetObjectiveIndex(Quest) + 1);
}

void UWzNarrativeSubsystem::CompleteQuest(const FGameplayTag& Quest)
{
	FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	if (!Runtime) return;

	// Stop everything of the quest
	SetTasksListening(Quest, false);
	SetTriggerListening(Quest, false);
	if (UWorld* World = GetPlayerWorld()) World->GetTimerManager().ClearTimer(Runtime->IntroTimer);
	EndScripts(Runtime->ObjectiveScripts);
	EndScripts(Runtime->QuestScripts);

	// Completed: what points here unlocks, the slot is free
	SetQuestState(Quest, EWzQuestState::Completed);
	Refresh();

	// The story is done when every mandatory quest is
	if (bNarrativeCompleted || !Director) return;

	for (const auto& Other : Director->Quests) if (Other && Other->Type == EWzQuestType::Mandatory && !IsQuestCompleted(Other->QuestTag)) return;

	bNarrativeCompleted = true;
	OnNarrativeCompleted.Broadcast();
}

// ====== Handlers ====== //

void UWzNarrativeSubsystem::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (NewPawn) BeginNarrative();
}

void UWzNarrativeSubsystem::HandleTriggerChanged(UWzNarrativeCondition* Condition, const FGameplayTag Quest)
{
	if (!Condition->IsCompleted() || HasActiveQuest() || GetQuestState(Quest) != EWzQuestState::Ready) return;

	ActivateQuest(Quest);
}

void UWzNarrativeSubsystem::HandleTaskChanged(UWzNarrativeCondition* Condition, const FGameplayTag Quest)
{
	const FWzQuestRuntime* Runtime = Runtimes.Find(Quest);
	const int32 Index = Runtime ? Runtime->Tasks.IndexOfByKey(Condition) : INDEX_NONE;
	if (Index == INDEX_NONE) return;

	// A released quest only acts while no other quest is active
	if (GetQuestState(Quest) == EWzQuestState::Released && HasActiveQuest()) return;

	// Announce, then finish when every required task is done
	OnTaskChanged.Broadcast(Quest, Index);
	if (IsCurrentObjectiveDone(Quest)) CompleteObjective(Quest);
}

void UWzNarrativeSubsystem::HandleDialogueEnded(UWzDialogueAsset* Ended)
{
	// Objectives waiting on this Complete Dialogue are done (collected first: finishing changes the runtimes)
	TArray<FGameplayTag> Waiting;
	for (const auto& Pair : Runtimes) if (Pair.Value.PendingDialogue == Ended) Waiting.Add(Pair.Key);

	for (const auto& Quest : Waiting) FinishObjective(Quest);
}