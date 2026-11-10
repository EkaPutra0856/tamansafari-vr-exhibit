// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dialogue/WzDialogueLinePlayback.h"

void FWzDialogueLinePlayback::Start(const int32 InNumChars, const float InVoiceDuration, const FWzDialogueTiming& InTiming, const bool bInAutoAdvance)
{
	Timing = InTiming;
	NumChars = InNumChars;
	VoiceDuration = InVoiceDuration;
	bAutoAdvance = bInAutoAdvance;
	Elapsed = 0.f;
	HoldRemaining = 0.f;
	bSkipped = false;
	Phase = EPhase::Typing;
	Revealed = Timing.TypewriterCharsPerSecond > 0.f ? 0 : NumChars;
}

void FWzDialogueLinePlayback::Tick(const float DeltaTime)
{
	switch (Phase)
	{
	case EPhase::Typing:
		Elapsed += DeltaTime;
		if (Timing.TypewriterCharsPerSecond > 0.f) Revealed = FMath::Min(NumChars, FMath::FloorToInt(Elapsed * Timing.TypewriterCharsPerSecond));

		// The line is over when the text is fully shown AND the voice has finished.
		if (Revealed >= NumChars && Elapsed >= VoiceDuration)
		{
			HoldRemaining = Timing.MinHold;
			Phase = EPhase::Holding;
		}
		break;

	case EPhase::Holding:
		HoldRemaining -= DeltaTime;
		if (HoldRemaining <= 0.f) Phase = (bAutoAdvance || bSkipped) ? EPhase::Done : EPhase::Waiting;
		break;

	default:
		break;
	}
}

void FWzDialogueLinePlayback::Skip()
{
	switch (Phase)
	{
	case EPhase::Typing:
	{
		// The buffer scales with the text the reader has not seen yet.
		const int32 Unread = NumChars - Revealed;
		Revealed = NumChars;
		bSkipped = true;
		HoldRemaining = FMath::Max(Timing.MinHold, Unread / FMath::Max(Timing.ReadingCharsPerSecond, 1.f));
		Phase = EPhase::Holding;
		break;
	}

	case EPhase::Holding:
	case EPhase::Waiting:
		Phase = EPhase::Done;
		break;

	default:
		break;
	}
}