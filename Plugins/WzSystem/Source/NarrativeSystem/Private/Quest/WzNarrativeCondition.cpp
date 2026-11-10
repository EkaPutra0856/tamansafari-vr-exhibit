// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quest/WzNarrativeCondition.h"

// ====== Events ====== //

void UWzNarrativeCondition::OnBegin_Implementation(const FWzNarrativeContext& Context) {}

void UWzNarrativeCondition::OnEnd_Implementation() {}

// ====== API ====== //

void UWzNarrativeCondition::Begin(const FWzNarrativeContext& Context)
{
	if (Phase != EPhase::Idle) return;

	// Listen
	Phase = EPhase::Running;
	OnBegin(Context);
}

void UWzNarrativeCondition::End()
{
	// One that never ran is closed too, so a late Begin is ignored
	const bool bWasRunning = Phase == EPhase::Running;
	Phase = EPhase::Ended;
	if (bWasRunning) OnEnd();
}

void UWzNarrativeCondition::Complete()
{
	if (Phase != EPhase::Running || bCompleted) return;

	// Done
	bCompleted = true;
	Current = Required;
	OnChanged.Broadcast(this);
}

void UWzNarrativeCondition::SetProgress(const int32 InCurrent, const int32 InRequired)
{
	if (Phase != EPhase::Running || bCompleted) return;

	// Nothing new: no announcement
	const int32 NewRequired = FMath::Max(1, InRequired);
	const int32 NewCurrent = FMath::Clamp(InCurrent, 0, NewRequired);
	if (NewCurrent == Current && NewRequired == Required) return;

	// Count, and complete when reached
	Required = NewRequired;
	Current = NewCurrent;
	if (Current >= Required) Complete();
	else OnChanged.Broadcast(this);
}