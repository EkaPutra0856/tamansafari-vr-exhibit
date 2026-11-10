// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PathGuide/PathGuideTypes.h"
#include "PathGuideRouteActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UWzPathGuideComponent;
class UWzPathGuideConfig;

namespace PathGuide::Ribbon
{
	struct FParams;
}

/** World VIEW of the Path Guide: draws the route, ring and column from the Model. */
UCLASS(HideCategories = (Replication, Collision, Input, HLOD, Physics, Networking, Cooking))
class EXPLORATIONSYSTEM_API AWzPathGuideRouteActor : public AActor
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AWzPathGuideRouteActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// ====== Types ====== //

	enum class EFade : uint8
	{
		None,
		In,
		Out
	};

	struct FChunk
	{
		float StartDistance = 0.f;
		float EndDistance = 0.f;
	};

private:
	// ====== Route ====== //

	/** Rebuilds the visuals from the Model's route. */
	void ShowRoute();

	/** Updates the materials and hides consumed / far chunks. */
	void SetProgress(float DistanceAlong);

	void FadeOut();
	void Hide();

private:
	// ====== Visuals ====== //

	void CreateMaterials();
	void PushStaticParameters() const;
	void BuildRibbonSections();
	void BuildRingSection() const;
	void BuildColumn() const;
	void UpdateChunkVisibility(bool bForce);
	void SetGlobalOpacity(float NewOpacity) const;
	PathGuide::Ribbon::FParams MakeTraceParams() const;

	/** Adds a section from rows of LanesPerSample vertices. */
	void CreateStripSection(const TArray<FVector>& Vertices, const TArray<FVector>& Normals, const TArray<FVector2D>& UV, UMaterialInterface* Material, int32 LanesPerSample) const;

private:
	// ====== References ====== //

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Column;

	UPROPERTY(Transient)
	TObjectPtr<UWzPathGuideComponent> PathGuide;

	/** Owned by the subsystem. */
	UPROPERTY(Transient)
	TObjectPtr<const UWzPathGuideConfig> Config;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RouteMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ColumnMaterial;

private:
	// ====== State ====== //

	FPathGuideRibbon Ribbon;
	FVector BeaconLocation = FVector::ZeroVector;
	TArray<FChunk> Chunks;
	int32 FirstVisibleChunk = INDEX_NONE;
	int32 LastVisibleChunk = INDEX_NONE;
	float Progress = 0.f;
	float LastPushedProgress = -1.f;

	EFade Fade = EFade::None;
	float FadeElapsed = 0.f;

	/** True from the first route until the fade-out ends. */
	bool bVisible = false;
};