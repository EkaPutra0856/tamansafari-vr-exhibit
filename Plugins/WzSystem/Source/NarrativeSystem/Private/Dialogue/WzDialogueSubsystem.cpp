// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dialogue/WzDialogueSubsystem.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dialogue/Camera/WzDialogueCameraEffect.h"
#include "Dialogue/Data/WzDialogueAsset.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagAssetInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "WzNarrativeSettings.h"

// ====== Lifecycle ====== //

UWorld* UWzDialogueSubsystem::GetTickableGameObjectWorld() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();

	return LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
}

void UWzDialogueSubsystem::Deinitialize()
{
	// Drop what waits, then close the running one
	Queue.Reset();
	FinishRestore();
	EndDialogue();

	Super::Deinitialize();
}

void UWzDialogueSubsystem::Tick(const float DeltaTime)
{
	// Glide the view back, then release look (also while a camera-less dialogue runs)
	if (bRestoring)
	{
		if (APlayerController* PC = GetPlayerController())
		{
			const FRotator Now = FMath::RInterpTo(PC->GetControlRotation(), SavedControlRotation, DeltaTime, GetDefault<UWzNarrativeSettings>()->RestoreInterpSpeed);
			PC->SetControlRotation(Now);
			if (Now.Equals(SavedControlRotation, 0.5f)) FinishRestore();
		}
		else bRestoring = false;
	}
	if (!ActiveDialogue) return;

	// Camera effect
	const FWzDialogueLine* Line = GetCurrentLine();
	if (Line && Line->CameraEffect && !ActiveDialogue->bAmbient) Line->CameraEffect->TickEffect(GetPlayerController(), FindSpeakerActor(Line->Speaker), DeltaTime);

	// Typewriter: only tell listeners when it moved
	const int32 Before = Playback.GetRevealedChars();
	Playback.Tick(DeltaTime);
	if (Playback.GetRevealedChars() != Before) OnRevealChanged.Broadcast(Playback.GetRevealedChars());

	// Next line
	if (Playback.IsDone()) StartLine(LineIndex + 1);
}

// ====== API ====== //

bool UWzDialogueSubsystem::StartDialogue(UWzDialogueAsset* Dialogue)
{
	APlayerController* PC = GetPlayerController();
	if (!Dialogue || Dialogue->Lines.IsEmpty() || !PC) return false;

	// A bark never waits (dropped when busy); any other dialogue cuts a running bark
	if (Dialogue->bAmbient)
	{
		if (ActiveDialogue || bEnding || !Queue.IsEmpty()) return false;
	}
	else EndAmbientDialogue();

	// Wait behind the running one, anything already waiting, and an end still being announced
	if (ActiveDialogue || bEnding || !Queue.IsEmpty())
	{
		Queue.Add(Dialogue);

		return true;
	}

	// Free: play now
	Play(Dialogue, PC);

	return true;
}

void UWzDialogueSubsystem::Skip()
{
	if (!ActiveDialogue) return;

	// First press while typing: reveal in full and cut the voice
	const bool bWasTyping = Playback.IsTyping();
	Playback.Skip();
	if (bWasTyping) StopVoice();

	// Tell listeners, then next line when done
	OnRevealChanged.Broadcast(Playback.GetRevealedChars());
	if (Playback.IsDone()) StartLine(LineIndex + 1);
}

void UWzDialogueSubsystem::EndDialogue()
{
	if (!ActiveDialogue) return;

	// Close the line
	EndCurrentLine();
	StopLineAnimation();

	// Release the locks (the controller can be gone on shutdown)
	if (APlayerController* PC = GetPlayerController())
	{
		if (bMoveBlocked) PC->SetIgnoreMoveInput(false);

		// Glide back in Tick, which releases look when the view arrived
		if (bRestoreRotation) bRestoring = true;
		else if (bLookBlocked) PC->SetIgnoreLookInput(false);
	}

	// Reset
	UWzDialogueAsset* Ended = ActiveDialogue;
	ActiveDialogue = nullptr;
	LineIndex = INDEX_NONE;
	SpeakerActors.Reset();
	bMoveBlocked = false;
	bLookBlocked = false;
	bRestoreRotation = false;

	// Announce: dialogues requested meanwhile queue, so every listener sees the end before the next start
	bEnding = true;
	OnDialogueEnded.Broadcast(Ended);
	bEnding = false;

	// Next in the queue
	if (ActiveDialogue || Queue.IsEmpty()) return;

	UWzDialogueAsset* Next = Queue[0];
	Queue.RemoveAt(0);
	APlayerController* PC = GetPlayerController();
	if (PC) Play(Next, PC);
	else Queue.Reset();
}

void UWzDialogueSubsystem::EndAmbientDialogue()
{
	if (ActiveDialogue && ActiveDialogue->bAmbient && !bEnding) EndDialogue();
}

UWzDialogueSubsystem* UWzDialogueSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;

	return LocalPlayer ? LocalPlayer->GetSubsystem<UWzDialogueSubsystem>() : nullptr;
}

// ====== Getters ====== //

AActor* UWzDialogueSubsystem::FindSpeakerActor(const FGameplayTag Speaker) const
{
	const TWeakObjectPtr<AActor>* Found = SpeakerActors.Find(Speaker);

	return Found ? Found->Get() : nullptr;
}

// ====== Helpers ====== //

APlayerController* UWzDialogueSubsystem::GetPlayerController() const
{
	UWorld* World = GetTickableGameObjectWorld();

	return World ? GetLocalPlayer()->GetPlayerController(World) : nullptr;
}

const FWzDialogueLine* UWzDialogueSubsystem::GetCurrentLine() const
{
	return ActiveDialogue && ActiveDialogue->Lines.IsValidIndex(LineIndex) ? &ActiveDialogue->Lines[LineIndex] : nullptr;
}

void UWzDialogueSubsystem::Play(UWzDialogueAsset* Dialogue, APlayerController* PC)
{
	// A view glide may still run: a camera dialogue takes it over, a camera-less one lets it finish
	const bool bHasCamera = Dialogue->HasCameraEffects();
	const bool bTakeOverRestore = bRestoring && bHasCamera;
	if (bTakeOverRestore)
	{
		bRestoring = false;
		PC->SetIgnoreLookInput(false);
	}

	// Track it
	ActiveDialogue = Dialogue;
	Timing = Dialogue->GetEffectiveTiming();
	CacheSpeakers();

	// Player locks are reference counted: remember what was set to undo exactly that
	bMoveBlocked = !Dialogue->bAmbient && Dialogue->bBlockMovement;
	bLookBlocked = !Dialogue->bAmbient && (Dialogue->bBlockLook || bHasCamera);
	bRestoreRotation = bHasCamera;
	if (bMoveBlocked) PC->SetIgnoreMoveInput(true);
	if (bLookBlocked) PC->SetIgnoreLookInput(true);
	if (bRestoreRotation && !bTakeOverRestore) SavedControlRotation = PC->GetControlRotation();

	// Announce, then the first line
	OnDialogueStarted.Broadcast(Dialogue);
	StartLine(0);
}

void UWzDialogueSubsystem::StartLine(const int32 Index)
{
	// Past the last line: done (checked first, EndDialogue already ends the current line)
	if (!ActiveDialogue || !ActiveDialogue->Lines.IsValidIndex(Index))
	{
		EndDialogue();

		return;
	}
	EndCurrentLine();

	// Speaker and animation
	APlayerController* PC = GetPlayerController();
	LineIndex = Index;
	const FWzDialogueLine& Line = ActiveDialogue->Lines[Index];
	AActor* Speaker = FindSpeakerActor(Line.Speaker);
	if (Line.Animation.IsNull()) StopLineAnimation();
	else PlayLineAnimation(Line, Speaker);

	// Voice: on the speaker, or plain 2D for a narrator
	float VoiceDuration = 0.f;
	if (USoundBase* Sound = Line.Voice.LoadSynchronous()) // ponytail: sync load per line; async-preload the next line's voice if hitches show up
	{
		if (Speaker && Speaker->GetRootComponent()) VoiceComponent = UGameplayStatics::SpawnSoundAttached(Sound, Speaker->GetRootComponent());
		else if (PC) VoiceComponent = UGameplayStatics::SpawnSound2D(PC, Sound);

		// A looping sound has no end: do not wait for it
		VoiceDuration = Sound->IsLooping() ? 0.f : Sound->GetDuration();
	}

	// Typewriter and camera
	Playback.Start(Line.Text.ToString().Len(), VoiceDuration, Timing, ActiveDialogue->bAutoAdvance);
	if (Line.CameraEffect && !ActiveDialogue->bAmbient) Line.CameraEffect->BeginEffect(PC, Speaker);

	// Announce
	OnLineStarted.Broadcast(Index);
	OnRevealChanged.Broadcast(Playback.GetRevealedChars());
}

void UWzDialogueSubsystem::EndCurrentLine()
{
	const FWzDialogueLine* Line = GetCurrentLine();
	if (Line && Line->CameraEffect && !ActiveDialogue->bAmbient) Line->CameraEffect->EndEffect(GetPlayerController(), FindSpeakerActor(Line->Speaker));

	StopVoice();
}

void UWzDialogueSubsystem::StopVoice()
{
	// The component destroys itself when the sound ends, so it may be gone
	if (IsValid(VoiceComponent)) VoiceComponent->Stop();
	VoiceComponent = nullptr;
}

void UWzDialogueSubsystem::PlayLineAnimation(const FWzDialogueLine& Line, AActor* Speaker)
{
	// Resolve
	const UWzNarrativeSettings* Settings = GetDefault<UWzNarrativeSettings>();
	UAnimSequenceBase* Anim = Line.Animation.LoadSynchronous();
	USkeletalMeshComponent* Mesh = Speaker ? Speaker->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* Inst = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim || !Inst)
	{
		StopLineAnimation();

		return;
	}

	// Already playing on this mesh: keep it, only re-sync when it drifted (e.g. after Skip)
	if (Inst == SpeakerAnim.Get() && PlayingAnimation.Get() == Anim && SpeakerMontage && Inst->Montage_IsPlaying(SpeakerMontage))
	{
		if (FMath::Abs(Inst->Montage_GetPosition(SpeakerMontage) - Line.AnimationStartTime) > Settings->AnimationSyncTolerance) Inst->Montage_SetPosition(SpeakerMontage, Line.AnimationStartTime);

		return;
	}

	// Play
	StopLineAnimation();
	if (UAnimMontage* Montage = Cast<UAnimMontage>(Anim)) SpeakerMontage = Inst->Montage_Play(Montage, 1.f, EMontagePlayReturnType::MontageLength, Line.AnimationStartTime) > 0.f ? Montage : nullptr;
	else SpeakerMontage = Inst->PlaySlotAnimationAsDynamicMontage(Anim, Settings->AnimationSlot, Settings->AnimationBlendTime, Settings->AnimationBlendTime, 1.f, 1, -1.f, Line.AnimationStartTime);
	SpeakerAnim = Inst;
	PlayingAnimation = Anim;
}

void UWzDialogueSubsystem::StopLineAnimation()
{
	UAnimInstance* Inst = SpeakerAnim.Get();
	if (Inst && SpeakerMontage) Inst->Montage_Stop(GetDefault<UWzNarrativeSettings>()->AnimationBlendTime, SpeakerMontage);

	SpeakerMontage = nullptr;
	SpeakerAnim.Reset();
	PlayingAnimation.Reset();
}

void UWzDialogueSubsystem::FinishRestore()
{
	if (!bRestoring) return;

	// Snap back and release look
	bRestoring = false;
	if (APlayerController* PC = GetPlayerController())
	{
		PC->SetControlRotation(SavedControlRotation);
		PC->SetIgnoreLookInput(false);
	}
}

void UWzDialogueSubsystem::CacheSpeakers()
{
	SpeakerActors.Reset();

	UWorld* World = GetTickableGameObjectWorld();
	if (!World || !ActiveDialogue) return;

	// Speakers to find (a line without a speaker is the narrator)
	TSet<FGameplayTag> Wanted;
	for (const auto& Line : ActiveDialogue->Lines) Wanted.Add(Line.Speaker);
	Wanted.Remove(FGameplayTag::EmptyTag);

	// ponytail: full actor scan once per dialogue; add a registry if levels get huge
	for (TActorIterator<AActor> It(World); It && !Wanted.IsEmpty(); ++It)
	{
		// Real gameplay tags when the actor has them, else its plain Actor Tags by the tag's name
		FGameplayTagContainer Owned;
		if (const IGameplayTagAssetInterface* TagActor = Cast<IGameplayTagAssetInterface>(*It)) TagActor->GetOwnedGameplayTags(Owned);

		for (auto Tag = Wanted.CreateIterator(); Tag; ++Tag)
		{
			if (Owned.HasTagExact(*Tag) || It->ActorHasTag(Tag->GetTagName()))
			{
				SpeakerActors.Add(*Tag, *It);
				Tag.RemoveCurrent();
			}
		}
	}
}