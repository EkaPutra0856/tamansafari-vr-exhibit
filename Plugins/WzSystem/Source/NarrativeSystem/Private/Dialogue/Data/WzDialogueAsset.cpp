// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dialogue/Data/WzDialogueAsset.h"

#include "WzNarrativeSettings.h"

FWzDialogueTiming UWzDialogueAsset::GetEffectiveTiming() const
{
	return bOverrideTiming ? Timing : GetDefault<UWzNarrativeSettings>()->DefaultTiming;
}

bool UWzDialogueAsset::HasCameraEffects() const
{
	return !bAmbient && Lines.ContainsByPredicate([](const FWzDialogueLine& Line) { return Line.CameraEffect != nullptr; });
}