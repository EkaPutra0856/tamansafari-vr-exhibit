// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "Dialogue/WzDialogueLinePlayback.h"
#include "WzDialogueSubsystem.generated.h"

class AActor;
class APlayerController;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UAudioComponent;
class UWzDialogueAsset;
struct FWzDialogueLine;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDialogueStarted, UWzDialogueAsset* /*Dialogue*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDialogueEnded, UWzDialogueAsset* /*Dialogue*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDialogueLineStarted, int32 /*LineIndex*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDialogueRevealChanged, int32 /*RevealedChars*/);

/** MODEL of the dialogue MVC: plays dialogue assets for one local player. No input, no UI. */
UCLASS()
class NARRATIVESYSTEM_API UWzDialogueSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWzDialogueSubsystem, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return ActiveDialogue != nullptr || bRestoring; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

public:
	// ====== Events ====== //

	FOnDialogueStarted OnDialogueStarted;
	FOnDialogueEnded OnDialogueEnded;
	FOnDialogueLineStarted OnLineStarted;

	/** Also fires once per line start and on Skip. */
	FOnDialogueRevealChanged OnRevealChanged;

public:
	// ====== API ====== //

	/** Plays it, or queues it behind the running one. False when null/empty, no player controller, or an ambient one while busy. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool StartDialogue(UWzDialogueAsset* Dialogue);

	/** 1st press completes the typing, the next goes to the next line. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Skip();

	/** Ends the running one; the next queued starts. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void EndDialogue();

	/** Ends the running one only if it is an ambient bark; anything else keeps playing. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void EndAmbientDialogue();

	/** The first local player's dialogue subsystem, or null. */
	UFUNCTION(BlueprintPure, Category = "Dialogue", meta = (WorldContext = "WorldContext"))
	static UWzDialogueSubsystem* Get(const UObject* WorldContext);

private:
	// ====== State ====== //

	UPROPERTY(Transient)
	TObjectPtr<UWzDialogueAsset> ActiveDialogue;

	/** Waiting for the running one to end, in request order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWzDialogueAsset>> Queue;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> SpeakerMontage;

	TWeakObjectPtr<UAnimInstance> SpeakerAnim;
	TWeakObjectPtr<const UAnimSequenceBase> PlayingAnimation;
	TMap<FGameplayTag, TWeakObjectPtr<AActor>> SpeakerActors;
	FWzDialogueLinePlayback Playback;
	FWzDialogueTiming Timing;
	FRotator SavedControlRotation = FRotator::ZeroRotator;
	int32 LineIndex = INDEX_NONE;
	bool bMoveBlocked = false;
	bool bLookBlocked = false;
	bool bRestoreRotation = false;

	/** The view is still gliding back to SavedControlRotation. */
	bool bRestoring = false;

	/** OnDialogueEnded is being broadcast: a dialogue requested now waits until every listener saw the end. */
	bool bEnding = false;

public:
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FORCEINLINE bool IsDialogueActive() const { return ActiveDialogue != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FORCEINLINE UWzDialogueAsset* GetActiveDialogue() const { return ActiveDialogue; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FORCEINLINE int32 GetLineIndex() const { return LineIndex; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FORCEINLINE int32 GetRevealedChars() const { return Playback.GetRevealedChars(); }

	/** The actor speaking for the tag, or null (narrator). */
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	AActor* FindSpeakerActor(FGameplayTag Speaker) const;

private:
	// ====== Helpers ====== //

	APlayerController* GetPlayerController() const;
	const FWzDialogueLine* GetCurrentLine() const;

	/** Starts it right now (nothing is playing). */
	void Play(UWzDialogueAsset* Dialogue, APlayerController* PC);

	void CacheSpeakers();
	void StartLine(int32 Index);

	/** Ends the line's camera effect and voice. */
	void EndCurrentLine();

	void StopVoice();
	void PlayLineAnimation(const FWzDialogueLine& Line, AActor* Speaker);
	void StopLineAnimation();

	/** Snaps the view back and releases look. */
	void FinishRestore();
};
