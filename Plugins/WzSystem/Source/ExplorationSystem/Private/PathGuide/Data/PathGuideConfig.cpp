// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/Data/PathGuideConfig.h"

#include "PathGuide/World/PathGuideRouteActor.h"

// ====== Lifecycle ====== //

UWzPathGuideConfig::UWzPathGuideConfig()
{
	RouteActorClass = AWzPathGuideRouteActor::StaticClass();
	ColumnMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
}