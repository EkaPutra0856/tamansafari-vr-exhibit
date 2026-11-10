// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

namespace PathGuide
{
	/** Ground line trace in the shape Ribbon::FGroundTraceFn expects. Pass it straight to the ribbon builder. */
	struct FGroundTracer
	{
		/** IgnoredActor (may be null) never blocks the traces, e.g. the visitor's own pawn. */
		FGroundTracer(const UWorld* InWorld, ECollisionChannel InChannel, const AActor* IgnoredActor)
			: World(InWorld)
			, Channel(InChannel)
			, QueryParams(SCENE_QUERY_STAT(PathGuideGround), false, IgnoredActor)
		{
		}

		bool operator()(const FVector& Start, const FVector& End, FVector& OutPosition, FVector& OutNormal) const
		{
			FHitResult Hit;
			if (!World || !World->LineTraceSingleByChannel(Hit, Start, End, Channel, QueryParams)) return false;
			OutPosition = Hit.ImpactPoint;
			OutNormal = Hit.ImpactNormal;

			return true;
		}

		const UWorld* World;
		ECollisionChannel Channel;
		FCollisionQueryParams QueryParams;
	};
}
