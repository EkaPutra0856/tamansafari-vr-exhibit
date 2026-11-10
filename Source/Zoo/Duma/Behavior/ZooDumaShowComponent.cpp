// Copyright Epic Games, Inc. All Rights Reserved.

#include "Duma/Behavior/ZooDumaShowComponent.h"

// ====== Lifecycle ====== //

UZooDumaShowComponent::UZooDumaShowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Every showpiece on: weight, seconds, size in cm
	Showpieces.Add(EZooDumaShowpiece::Loop, FZooDumaShowpieceSettings{ true, 1.f, 2.4f, 30.f });
	Showpieces.Add(EZooDumaShowpiece::BarrelRoll, FZooDumaShowpieceSettings{ true, 1.f, 1.6f, 40.f });
	Showpieces.Add(EZooDumaShowpiece::Swoop, FZooDumaShowpieceSettings{ true, 1.f, 3.f, 150.f });
	Showpieces.Add(EZooDumaShowpiece::Spiral, FZooDumaShowpieceSettings{ true, 1.f, 3.2f, 25.f });
	Showpieces.Add(EZooDumaShowpiece::PeekABoo, FZooDumaShowpieceSettings{ true, 0.8f, 1.6f, 150.f });
	Showpieces.Add(EZooDumaShowpiece::Swing, FZooDumaShowpieceSettings{ true, 1.f, 3.6f, 90.f });
}

void UZooDumaShowComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Fade with the permission
	Clock += DeltaTime;
	const bool bShow = bAllowed && !bAborting;
	Weight = FMath::FInterpConstantTo(Weight, bShow ? 1.f : 0.f, DeltaTime, 1.f / BlendSeconds);

	// Glancing: it ends when it is over, or when the fade is complete (the showpiece timer waits)
	if (bGlancing)
	{
		GlanceElapsed += DeltaTime;
		if (GlanceElapsed >= ZooDuma::GlanceDuration(Glance) || (!bShow && Weight <= 0.f)) EndGlance();

		return;
	}

	// Running: it ends when it is over, or when the fade is complete
	if (bPerforming)
	{
		TickShowpiece(DeltaTime);
		if (bPerforming && !bShow && Weight <= 0.f) EndShowpiece();

		return;
	}

	// Idle: count down, but only while it may run
	if (!bAllowed) return;

	NextShowIn -= DeltaTime;
	NextGlanceIn -= DeltaTime;

	// A showpiece wins when both are due
	if (NextShowIn <= 0.f) StartShowpiece();
	else if (NextGlanceIn <= 0.f && Glance.bEnabled) StartGlance();
}

// ====== API ====== //

void UZooDumaShowComponent::SetAllowed(const bool bInAllowed)
{
	if (bAllowed == bInAllowed) return;

	// Allowed again: no stale glance, and a fresh gap before the next showpiece
	bAllowed = bInAllowed;
	if (!bAllowed) return;

	bGlancing = false;
	if (!bPerforming) bAborting = false;
	ScheduleNext();
}

FZooDumaPose UZooDumaShowComponent::GetPose(const FVector& FollowOffset) const
{
	FZooDumaPose Pose = ZooDuma::Drift(Clock, Drift);
	if (bPerforming) Pose += GetShowpiecePose(FollowOffset);
	if (bGlancing) Pose += ZooDuma::Glance(GlanceElapsed, Glance, GlanceMirror, FollowOffset);

	// Faded by the permission
	Pose.Offset *= Weight;
	Pose.Rotation *= Weight;

	return Pose;
}

void UZooDumaShowComponent::Abort()
{
	if (bPerforming || bGlancing) bAborting = true;
}

void UZooDumaShowComponent::NotifyBlinkHidden()
{
	if (!bPerforming || Current != EZooDumaShowpiece::PeekABoo || !bWaitingBlink) return;

	// Hidden at home: he reappears at the side. Hidden at the side: he reappears at home and it is over.
	bWaitingBlink = false;
	if (Stage == EPeekStage::Out)
	{
		Stage = EPeekStage::Side;
		Elapsed = 0.f;
	}
	else if (Stage == EPeekStage::Home) EndShowpiece();
}

// ====== Schedule ====== //

void UZooDumaShowComponent::ScheduleNext()
{
	NextShowIn = FMath::FRandRange(MinGap, FMath::Max(MinGap, MaxGap));
	ScheduleNextGlance();
}

void UZooDumaShowComponent::ScheduleNextGlance()
{
	NextGlanceIn = FMath::FRandRange(Glance.MinGap, FMath::Max(Glance.MinGap, Glance.MaxGap));
}

void UZooDumaShowComponent::StartShowpiece()
{
	// The pool: enabled and weighted
	TArray<TPair<EZooDumaShowpiece, float>> Pool;
	float Total = 0.f;
	for (const auto& Pair : Showpieces)
	{
		if (!Pair.Value.bEnabled || Pair.Value.Weight <= 0.f) continue;

		Pool.Emplace(Pair.Key, Pair.Value.Weight);
		Total += Pair.Value.Weight;
	}

	// The last one rests, unless it is the only one
	if (bHasLast && Pool.Num() > 1)
	{
		const int32 Index = Pool.IndexOfByPredicate([this](const TPair<EZooDumaShowpiece, float>& Entry)
		{
			return Entry.Key == Last;
		});
		if (Index != INDEX_NONE)
		{
			Total -= Pool[Index].Value;
			Pool.RemoveAt(Index);
		}
	}
	if (Pool.IsEmpty())
	{
		ScheduleNext();

		return;
	}

	// Weighted pick
	float Roll = FMath::FRandRange(0.f, Total);
	EZooDumaShowpiece Pick = Pool.Last().Key;
	for (const auto& Entry : Pool)
	{
		Roll -= Entry.Value;
		if (Roll > 0.f) continue;

		Pick = Entry.Key;
		break;
	}

	// Start it, a little different every time
	Current = Pick;
	Last = Pick;
	bHasLast = true;
	bPerforming = true;
	bAborting = false;
	bBurstDone = false;
	bWaitingBlink = false;
	Stage = EPeekStage::Out;
	Elapsed = 0.f;
	BlinkWait = 0.f;
	Variation.Mirror = FMath::RandBool() ? 1.f : -1.f;
	Variation.Scale = 1.f + FMath::FRandRange(-Jitter, Jitter);
	Variation.Speed = 1.f + FMath::FRandRange(-Jitter, Jitter);
}

void UZooDumaShowComponent::EndShowpiece()
{
	bPerforming = false;
	bAborting = false;
	bWaitingBlink = false;
	ScheduleNext();
}

// ====== Glance ====== //

void UZooDumaShowComponent::StartGlance()
{
	// Turns left or right at random
	bGlancing = true;
	GlanceElapsed = 0.f;
	GlanceMirror = FMath::RandBool() ? 1.f : -1.f;
}

void UZooDumaShowComponent::EndGlance()
{
	bGlancing = false;
	bAborting = false;
	ScheduleNextGlance();
}

// ====== Showpiece ====== //

void UZooDumaShowComponent::TickShowpiece(const float DeltaTime)
{
	const FZooDumaShowpieceSettings* Settings = Showpieces.Find(Current);
	if (!Settings)
	{
		EndShowpiece();

		return;
	}

	// A peek has its own flow
	if (Current == EZooDumaShowpiece::PeekABoo)
	{
		TickPeek(DeltaTime, *Settings);

		return;
	}

	// The spiral bursts into sparkles at its top
	Elapsed += DeltaTime;
	const float Duration = GetDuration(*Settings);
	if (Current == EZooDumaShowpiece::Spiral && !bBurstDone && Elapsed >= 0.5f * Duration)
	{
		bBurstDone = true;
		OnBurst.Broadcast();
	}
	if (Elapsed >= Duration) EndShowpiece();
}

void UZooDumaShowComponent::TickPeek(const float DeltaTime, const FZooDumaShowpieceSettings& Settings)
{
	// At the side: wiggle, then ask for the way home
	if (Stage == EPeekStage::Side)
	{
		Elapsed += DeltaTime;
		if (Elapsed >= GetDuration(Settings))
		{
			Stage = EPeekStage::Home;
			bWaitingBlink = false;
		}

		return;
	}

	// Out or home: ask for the blink once (the answer may come at once), then wait for it, giving up after a while
	if (!bWaitingBlink)
	{
		bWaitingBlink = true;
		BlinkWait = 0.f;
		OnPeekBlink.Broadcast();

		return;
	}

	BlinkWait += DeltaTime;
	if (BlinkWait >= PeekBlinkTimeout) EndShowpiece();
}

float UZooDumaShowComponent::GetDuration(const FZooDumaShowpieceSettings& Settings) const
{
	return Settings.Duration / FMath::Max(Variation.Speed, 0.1f);
}

FZooDumaPose UZooDumaShowComponent::GetShowpiecePose(const FVector& FollowOffset) const
{
	const FZooDumaShowpieceSettings* Settings = Showpieces.Find(Current);
	if (!Settings) return FZooDumaPose{};

	// A peek is only seen at the side: nothing at home, held while waiting to leave
	float Alpha = Elapsed / GetDuration(*Settings);
	if (Current == EZooDumaShowpiece::PeekABoo)
	{
		if (Stage == EPeekStage::Out) return FZooDumaPose{};
		if (Stage == EPeekStage::Home) Alpha = 1.f;
	}

	return ZooDuma::Showpiece(Current, Alpha, Variation, *Settings, FollowOffset);
}