// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/Routing/PathGuideFormat.h"

#include "PathGuide/Data/PathGuideUIConfig.h"
#include "Internationalization/Text.h"

namespace PathGuide::Format
{
	// ====== Helpers ====== //

	namespace
	{
		constexpr float CentimetersPerMeter = 100.f;

		FText WithNumber(const FText& Pattern, const FText& Number)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Number"), Number);

			return FText::Format(Pattern, Arguments);
		}
	}

	// ====== API ====== //

	FText Distance(const float Length, const UWzPathGuideUIConfig& Config)
	{
		// Under 10 m: whole metres. Under 1 km: steps of 5 m. Beyond that: kilometres with one decimal.
		const float Meters = FMath::Max(Length, 0.f) / CentimetersPerMeter;

		if (Meters < 10.f) return WithNumber(Config.DistanceMetersFormat, FText::AsNumber(FMath::RoundToInt(Meters)));
		if (Meters < 1000.f) return WithNumber(Config.DistanceMetersFormat, FText::AsNumber(FMath::RoundToInt(Meters / 5.f) * 5));

		FNumberFormattingOptions Options;
		Options.SetUseGrouping(false);
		Options.SetMinimumFractionalDigits(1);
		Options.SetMaximumFractionalDigits(1);

		return WithNumber(Config.DistanceKilometersFormat, FText::AsNumber(Meters / 1000.f, &Options));
	}

	FText Eta(const float Length, const UWzPathGuideUIConfig& Config)
	{
		// Under a minute shows text, otherwise whole minutes rounded up.
		const float Seconds = FMath::Max(Length, 0.f) / FMath::Max(Config.WalkingSpeed, 1.f);
		if (Seconds < 60.f) return Config.EtaUnderOneMinuteText;

		return WithNumber(Config.EtaMinutesFormat, FText::AsNumber(FMath::CeilToInt(Seconds / 60.f)));
	}

	FText DistanceAndEta(const float Length, const UWzPathGuideUIConfig& Config)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Distance"), Distance(Length, Config));
		Arguments.Add(TEXT("Eta"), Eta(Length, Config));

		return FText::Format(Config.DistanceAndEtaFormat, Arguments);
	}
}
