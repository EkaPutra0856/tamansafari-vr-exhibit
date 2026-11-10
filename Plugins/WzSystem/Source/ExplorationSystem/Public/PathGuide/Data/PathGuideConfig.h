// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "PathGuideConfig.generated.h"

class AWzPathGuideRouteActor;
class UMaterialInterface;
class UStaticMesh;
class UNavigationQueryFilter;

/** World-side tunables of the Path Guide (routing, ribbon, beacon, materials). Texts live in UWzPathGuideUIConfig. cm, seconds. */
UCLASS(BlueprintType)
class EXPLORATIONSYSTEM_API UWzPathGuideConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UWzPathGuideConfig();

public:
	// ====== Routing ====== //

	/** Destinations closer than this are reached with a direct NavMesh route. 0 = always use walkways. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routing", meta = (ClampMin = "0"))
	float DirectRouteMax = 800.f;

	/** Re-route when the visitor is farther than this from the route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routing", meta = (ClampMin = "50"))
	float OffRouteRecalc = 300.f;

	/** Vertical distance under which the visitor may count as arrived (deck / bridge heights). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routing", meta = (ClampMin = "10"))
	float ArrivalMaxHeightDelta = 200.f;

	/** Optional NavMesh query filter for the access/exit legs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routing")
	TSubclassOf<UNavigationQueryFilter> NavFilterClass;

public:
	// ====== Ribbon ====== //

	/** Width of the route line on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon", meta = (ClampMin = "5"))
	float RibbonWidth = 70.f;

	/** Lift above the ground to avoid z-fighting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon", meta = (ClampMin = "0"))
	float GroundOffset = 2.f;

	/** Collision channel used to find the ground (project channel "PathGuideGround" = GameTraceChannel1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_GameTraceChannel1;

	/** Ground traces start this far above the route height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon", meta = (ClampMin = "0"))
	float TraceUp = 150.f;

	/** Ground traces reach this far below the route height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon", meta = (ClampMin = "10"))
	float TraceDown = 1000.f;

	/** Ground heights further than this from their neighbours are dropped as spikes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon", meta = (ClampMin = "1"))
	float SpikeThreshold = 40.f;

	/** How far behind the visitor the line is still drawn before it is consumed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon", meta = (ClampMin = "0"))
	float ConsumeBehind = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon", meta = (ClampMin = "0"))
	float FadeStart = 4000.f;

	/** Beyond this distance ahead of the visitor the line is not rendered at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon", meta = (ClampMin = "1000"))
	float MaxVisible = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ribbon", meta = (ClampMin = "0", ClampMax = "1"))
	float FarOpacity = 0.3f;

public:
	// ====== Arrows ====== //

	/** Length of route ahead of the visitor that carries chevrons (drawn inside the ribbon material). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrows", meta = (ClampMin = "100"))
	float ArrowSection = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrows", meta = (ClampMin = "20"))
	float ChevronSpacing = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arrows")
	float ChevronSpeed = 120.f;

public:
	// ====== Colours ====== //

	/** Cream #F3ECDA (zoo signage palette) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor LineColor = FLinearColor::FromSRGBColor(FColor(0xF3, 0xEC, 0xDA));

	/** Ochre #CD923C */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor ChevronColor = FLinearColor::FromSRGBColor(FColor(0xCD, 0x92, 0x3C));

	/** Ochre #CD923C */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor BeaconColor = FLinearColor::FromSRGBColor(FColor(0xCD, 0x92, 0x3C));

	/** Scales all emissive output so the guide reads in bright daylight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours", meta = (ClampMin = "0"))
	float EmissiveIntensity = 2.f;

public:
	// ====== Beacon ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beacon", meta = (ClampMin = "20"))
	float RingRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beacon", meta = (ClampMin = "2"))
	float RingWidth = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beacon", meta = (ClampMin = "50"))
	float ColumnHeight = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beacon", meta = (ClampMin = "5"))
	float ColumnRadius = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beacon")
	TSoftObjectPtr<UStaticMesh> ColumnMesh;

public:
	// ====== Timing ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0"))
	float FadeInDuration = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0"))
	float FadeOutDuration = 0.6f;

public:
	// ====== Assets ====== //

	/** The actor that draws the route in the world. Subclass it to change the look. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets|Visuals")
	TSubclassOf<AWzPathGuideRouteActor> RouteActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets|Visuals")
	TSoftObjectPtr<UMaterialInterface> RouteMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets|Visuals")
	TSoftObjectPtr<UMaterialInterface> RingMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets|Visuals")
	TSoftObjectPtr<UMaterialInterface> ColumnMaterial;
};