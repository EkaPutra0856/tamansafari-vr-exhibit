// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PathGuideUIConfig.generated.h"

/** Screen-side tunables of the Path Guide: texts, ETA pace and HUD numbers. */
UCLASS(BlueprintType)
class EXPLORATIONSYSTEM_API UWzPathGuideUIConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UWzPathGuideUIConfig();

public:
	// ====== Texts ====== //

	/** All texts are localizable. Named argument: {Number} */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText DistanceMetersFormat;

	/** Named argument: {Number} */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText DistanceKilometersFormat;

	/** Named argument: {Number} */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText EtaMinutesFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText EtaUnderOneMinuteText;

	/** Named arguments: {Distance} {Eta} */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText DistanceAndEtaFormat;

	/** Named argument: {Place} */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText ArrivalFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texts")
	FText RouteFailedText;

public:
	// ====== Layout ====== //

	/** Visitor pace used for the "N min" estimate (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "10"))
	float WalkingSpeed = 130.f;

	/** Pixels kept free at the screen edge for the off-screen waypoint pin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float ScreenEdgePadding = 48.f;

	/** Height above the ground where the screen-space waypoint pin is anchored (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float WaypointHeightOffset = 250.f;

	/** Seconds the arrival toast stays on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float ArrivalToastDuration = 3.f;
};