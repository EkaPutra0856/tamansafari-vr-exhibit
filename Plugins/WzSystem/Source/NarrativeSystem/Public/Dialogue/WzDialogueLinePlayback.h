// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dialogue/Data/WzDialogueAsset.h"

/** Timing of ONE line: typewriter reveal, voice length, hold, and the two-press Skip. Pure logic, no world. */
struct NARRATIVESYSTEM_API FWzDialogueLinePlayback
{
	enum class EPhase : uint8
	{
		/** Text is typing out, or the voice is still playing. */
		Typing,
		/** Finished; the line stays up for the hold / reading buffer. */
		Holding,
		/** Manual dialogue: waiting for the next Skip. */
		Waiting,
		Done
	};

	void Start(int32 InNumChars, float InVoiceDuration, const FWzDialogueTiming& InTiming, bool bInAutoAdvance);
	void Tick(float DeltaTime);

	/** 1st press while typing: reveal everything + a reading buffer. Later presses: Done. */
	void Skip();

	int32 GetRevealedChars() const { return Revealed; }
	EPhase GetPhase() const { return Phase; }
	bool IsTyping() const { return Phase == EPhase::Typing; }
	bool IsDone() const { return Phase == EPhase::Done; }

private:
	FWzDialogueTiming Timing;
	int32 NumChars = 0;
	int32 Revealed = 0;
	float VoiceDuration = 0.f;
	float Elapsed = 0.f;
	float HoldRemaining = 0.f;
	bool bAutoAdvance = true;
	bool bSkipped = false;
	EPhase Phase = EPhase::Done;
};