// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dialogue/WzDialogueLinePlayback.h"

namespace
{
	constexpr EAutomationTestFlags DialogueTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	using EPhase = FWzDialogueLinePlayback::EPhase;

	FWzDialogueTiming MakeTiming(const float Type, const float Read, const float Hold)
	{
		FWzDialogueTiming Timing;
		Timing.TypewriterCharsPerSecond = Type;
		Timing.ReadingCharsPerSecond = Read;
		Timing.MinHold = Hold;
		return Timing;
	}

	/** Checks the revealed character count and the phase in one go. */
	void Expect(FAutomationTestBase& Test, const TCHAR* What, const FWzDialogueLinePlayback& Playback, const int32 Revealed, const EPhase Phase)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s: revealed"), What), Playback.GetRevealedChars(), Revealed);
		Test.TestEqual(*FString::Printf(TEXT("%s: phase"), What), static_cast<int32>(Playback.GetPhase()), static_cast<int32>(Phase));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzDialogueTypewriterTest, "NarrativeSystem.Dialogue.Playback.Typewriter", DialogueTestFlags)
bool FWzDialogueTypewriterTest::RunTest(const FString&)
{
	FWzDialogueLinePlayback P;
	P.Start(10, 0.f, MakeTiming(10, 10, 1), true);

	P.Tick(0.5f);
	Expect(*this, TEXT("half typed"), P, 5, EPhase::Typing);
	P.Tick(0.5f);
	Expect(*this, TEXT("fully typed"), P, 10, EPhase::Holding);
	P.Tick(1.f);
	Expect(*this, TEXT("hold over"), P, 10, EPhase::Done);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzDialogueWaitsForVoiceTest, "NarrativeSystem.Dialogue.Playback.WaitsForVoice", DialogueTestFlags)
bool FWzDialogueWaitsForVoiceTest::RunTest(const FString&)
{
	FWzDialogueLinePlayback P;
	P.Start(10, 3.f, MakeTiming(10, 10, 1), true);

	P.Tick(1.f);
	Expect(*this, TEXT("text done, voice playing"), P, 10, EPhase::Typing);
	P.Tick(2.f);
	Expect(*this, TEXT("voice done"), P, 10, EPhase::Holding);
	P.Tick(1.f);
	Expect(*this, TEXT("hold over"), P, 10, EPhase::Done);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzDialogueSkipTwiceTest, "NarrativeSystem.Dialogue.Playback.SkipTwice", DialogueTestFlags)
bool FWzDialogueSkipTwiceTest::RunTest(const FString&)
{
	// 20 unread chars at 10 chars/s = a 2 s reading buffer.
	FWzDialogueLinePlayback P;
	P.Start(30, 0.f, MakeTiming(10, 10, 0.5f), false);

	P.Tick(1.f);
	Expect(*this, TEXT("typing"), P, 10, EPhase::Typing);
	P.Skip();
	Expect(*this, TEXT("1st skip reveals all"), P, 30, EPhase::Holding);
	P.Tick(1.f);
	Expect(*this, TEXT("buffer running"), P, 30, EPhase::Holding);
	P.Skip();
	Expect(*this, TEXT("2nd skip"), P, 30, EPhase::Done);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzDialogueSkipBufferAdvancesManualTest, "NarrativeSystem.Dialogue.Playback.SkipBufferAdvancesManual", DialogueTestFlags)
bool FWzDialogueSkipBufferAdvancesManualTest::RunTest(const FString&)
{
	// After a Skip the line moves on by itself once the buffer ends, even in a manual dialogue.
	FWzDialogueLinePlayback P;
	P.Start(30, 0.f, MakeTiming(10, 10, 0.5f), false);

	P.Tick(1.f);
	P.Skip();
	P.Tick(1.f);
	Expect(*this, TEXT("buffer running"), P, 30, EPhase::Holding);
	P.Tick(1.f);
	Expect(*this, TEXT("buffer over"), P, 30, EPhase::Done);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzDialogueSkipBufferMinimumTest, "NarrativeSystem.Dialogue.Playback.SkipBufferMinimum", DialogueTestFlags)
bool FWzDialogueSkipBufferMinimumTest::RunTest(const FString&)
{
	// 3 unread chars need 0.3 s, but the buffer never drops below MinHold (1.5 s).
	FWzDialogueLinePlayback P;
	P.Start(10, 0.f, MakeTiming(10, 10, 1.5f), true);

	P.Tick(0.5f);
	P.Tick(0.25f);
	Expect(*this, TEXT("typing"), P, 7, EPhase::Typing);
	P.Skip();
	Expect(*this, TEXT("skipped"), P, 10, EPhase::Holding);
	P.Tick(1.f);
	Expect(*this, TEXT("still buffering"), P, 10, EPhase::Holding);
	P.Tick(0.5f);
	Expect(*this, TEXT("buffer over"), P, 10, EPhase::Done);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzDialogueManualWaitsTest, "NarrativeSystem.Dialogue.Playback.ManualWaits", DialogueTestFlags)
bool FWzDialogueManualWaitsTest::RunTest(const FString&)
{
	FWzDialogueLinePlayback P;
	P.Start(10, 0.f, MakeTiming(10, 10, 1), false);

	P.Tick(1.f);
	Expect(*this, TEXT("typed"), P, 10, EPhase::Holding);
	P.Tick(1.f);
	Expect(*this, TEXT("waiting"), P, 10, EPhase::Waiting);
	P.Tick(5.f);
	Expect(*this, TEXT("keeps waiting"), P, 10, EPhase::Waiting);
	P.Skip();
	Expect(*this, TEXT("skip"), P, 10, EPhase::Done);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzDialogueInstantTextTest, "NarrativeSystem.Dialogue.Playback.InstantText", DialogueTestFlags)
bool FWzDialogueInstantTextTest::RunTest(const FString&)
{
	// Typewriter speed 0 shows the whole line at once.
	FWzDialogueLinePlayback P;
	P.Start(12, 0.f, MakeTiming(0, 10, 1), true);

	Expect(*this, TEXT("start"), P, 12, EPhase::Typing);
	P.Tick(0.25f);
	Expect(*this, TEXT("first tick"), P, 12, EPhase::Holding);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS