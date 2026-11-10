// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/Data/PathGuideUIConfig.h"

#define LOCTEXT_NAMESPACE "PathGuideConfig"

// ====== Lifecycle ====== //

UWzPathGuideUIConfig::UWzPathGuideUIConfig()
{
	DistanceMetersFormat = LOCTEXT("DistanceMeters", "{Number} m");
	DistanceKilometersFormat = LOCTEXT("DistanceKilometers", "{Number} km");
	EtaMinutesFormat = LOCTEXT("EtaMinutes", "{Number} min");
	EtaUnderOneMinuteText = LOCTEXT("EtaUnderOneMinute", "< 1 min");
	DistanceAndEtaFormat = LOCTEXT("DistanceAndEta", "{Distance} · {Eta}");
	ArrivalFormat = LOCTEXT("Arrival", "You have arrived at {Place}");
	RouteFailedText = LOCTEXT("RouteFailed", "Sorry, no walkable route to that place was found.");
}

#undef LOCTEXT_NAMESPACE