// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dialogue/WzBarkComponent.h"

#include "Dialogue/Data/WzDialogueAsset.h"
#include "Dialogue/WzDialogueSubsystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogWzBark, Log, All);

// ====== Lifecycle ====== //

void UWzBarkComponent::BeginPlay()
{
	Super::BeginPlay();

	// The silence is filled by listening to the dialogue: a start ends it, an end begins a new one
	UWzDialogueSubsystem* Subsystem = UWzDialogueSubsystem::Get(this);
	if (!Subsystem)
	{
		UE_LOG(LogWzBark, Warning, TEXT("%s: no dialogue subsystem, it will stay silent"), *GetOwner()->GetName());

		return;
	}

	// A regular dialogue would queue and block, so only ambient ones are accepted (said once, here)
	for (int32 Index = 0; Index < Barks.Num(); ++Index) if (!Barks[Index] || !Barks[Index]->bAmbient) UE_LOG(LogWzBark, Warning, TEXT("%s: bark %d is empty or not ambient, it will be skipped"), *GetOwner()->GetName(), Index);

	Dialogue = Subsystem;
	Subsystem->OnDialogueStarted.AddUObject(this, &UWzBarkComponent::HandleDialogueStarted);
	Subsystem->OnDialogueEnded.AddUObject(this, &UWzBarkComponent::HandleDialogueEnded);

	// Nothing to wait for: the first gap starts now
	bUnlocked = !bWaitForFirstDialogue;
	if (bUnlocked) Schedule(FMath::FRandRange(MinGap, FMath::Max(MinGap, MaxGap)));
}

void UWzBarkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWzDialogueSubsystem* Subsystem = Dialogue.Get())
	{
		Subsystem->OnDialogueStarted.RemoveAll(this);
		Subsystem->OnDialogueEnded.RemoveAll(this);
	}

	Dialogue.Reset();
	Cancel();

	Super::EndPlay(EndPlayReason);
}

// ====== Schedule ====== //

void UWzBarkComponent::Schedule(const float Seconds)
{
	Cancel();

	const UWorld* World = GetWorld();
	if (World && !Barks.IsEmpty()) World->GetTimerManager().SetTimer(Timer, this, &UWzBarkComponent::HandleTimer, Seconds);
}

void UWzBarkComponent::Cancel()
{
	if (const UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(Timer);
}

void UWzBarkComponent::HandleTimer()
{
	UWzDialogueSubsystem* Subsystem = Dialogue.Get();
	if (!Subsystem) return;

	// Nothing valid to say: silent for good
	UWzDialogueAsset* Bark = PeekBark();
	if (!Bark) return;

	// Not now (the owner says no) or the dialogue is taken: look again shortly
	if (!bAllowed || !Subsystem->StartDialogue(Bark))
	{
		Schedule(RetrySeconds);

		return;
	}

	// Said: the bag moves on. The end of the bark starts the next gap.
	LastBark = Bag.Pop();
}

// ====== Bag ====== //

UWzDialogueAsset* UWzBarkComponent::PeekBark()
{
	if (Bag.IsEmpty()) RefillBag();

	return Bag.IsEmpty() ? nullptr : Barks[Bag.Last()].Get();
}

void UWzBarkComponent::RefillBag()
{
	// Every ambient bark once, shuffled
	for (int32 Index = 0; Index < Barks.Num(); ++Index) if (Barks[Index] && Barks[Index]->bAmbient) Bag.Add(Index);
	for (int32 Index = Bag.Num() - 1; Index > 0; --Index) Bag.Swap(Index, FMath::RandRange(0, Index));

	// Never the one just said, even across a refill
	if (Bag.Num() > 1 && Bag.Last() == LastBark) Bag.Swap(0, Bag.Num() - 1);
}

// ====== Dialogue ====== //

void UWzBarkComponent::HandleDialogueStarted(UWzDialogueAsset* Started)
{
	Cancel();
}

void UWzBarkComponent::HandleDialogueEnded(UWzDialogueAsset* Ended)
{
	// A regular dialogue opens the door; after that every end, a bark's too, starts a new gap
	if (Ended && !Ended->bAmbient) bUnlocked = true;
	if (bUnlocked) Schedule(FMath::FRandRange(MinGap, FMath::Max(MinGap, MaxGap)));
}