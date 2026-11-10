// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/World/PathGuideRouteActor.h"

#include "Components/StaticMeshComponent.h"
#include "PathGuide/Data/PathGuideConfig.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ExplorationSystem.h"
#include "ProceduralMeshComponent.h"
#include "PathGuide/Components/PathGuideComponent.h"
#include "PathGuide/Routing/PathGuideGroundTrace.h"
#include "PathGuide/Routing/PathGuideRibbonBuilder.h"
#include "PathGuide/Subsystem/PathGuideSubsystem.h"

namespace
{
	/** Vertices around the ring. */
	constexpr int32 RingSegments = 64;

	/** Ribbon chunk length, so far chunks can be hidden. */
	constexpr float ChunkLength = 1000.f;

	// Material parameters set every step
	const FName PlayerDistParam(TEXT("PlayerDist"));
	const FName GlobalOpacityParam(TEXT("GlobalOpacity"));

	void ConfigureVisual(UPrimitiveComponent* Component)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(false);
		Component->bAffectDistanceFieldLighting = false;
		Component->bAffectDynamicIndirectLighting = false;
		Component->bVisibleInRayTracing = false;
		Component->bVisibleInReflectionCaptures = false;
		Component->bVisibleInRealTimeSkyCaptures = false;
	}
}

// ====== Lifecycle ====== //

AWzPathGuideRouteActor::AWzPathGuideRouteActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	// One mesh for ribbon and ring, a static mesh for the column
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->bUseAsyncCooking = false;
	ConfigureVisual(Mesh);
	SetRootComponent(Mesh);

	Column = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Column"));
	Column->SetupAttachment(Mesh);
	Column->SetMobility(EComponentMobility::Movable);
	Column->SetVisibility(false);
	ConfigureVisual(Column);
}

void AWzPathGuideRouteActor::BeginPlay()
{
	Super::BeginPlay();

	// Model on the owner, config in the subsystem
	PathGuide = GetOwner() ? GetOwner()->FindComponentByClass<UWzPathGuideComponent>() : nullptr;
	const UWzPathGuideSubsystem* Subsystem = GetWorld()->GetSubsystem<UWzPathGuideSubsystem>();
	Config = Subsystem ? Subsystem->GetConfig() : nullptr;
	if (!PathGuide || !Config)
	{
		UE_LOG(LogPathGuide, Warning, TEXT("Route actor needs a PathGuide component on its owner and a config asset."));

		return;
	}

	// Route changed
	PathGuide->OnRouteChanged.AddWeakLambda(this, [this]()
	{
		ShowRoute();
	});

	// Progress
	PathGuide->OnProgress.AddWeakLambda(this, [this](float)
	{
		SetProgress(PathGuide->GetProgress());
	});

	// State: not guiding fades out
	PathGuide->OnStateChanged.AddWeakLambda(this, [this](EPathGuideState NewState, const FGameplayTag&)
	{
		if (NewState != EPathGuideState::Guiding) FadeOut();
	});
}

void AWzPathGuideRouteActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Fade == EFade::None || !Config)
	{
		SetActorTickEnabled(false);

		return;
	}

	// Fade in: opacity 0 -> 1. Fade out: 1 -> 0, then hide
	FadeElapsed += DeltaSeconds;
	if (Fade == EFade::In)
	{
		const float Alpha = FMath::Clamp(FadeElapsed / FMath::Max(Config->FadeInDuration, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
		SetGlobalOpacity(Alpha);
		if (Alpha >= 1.f) Fade = EFade::None;
	}
	else
	{
		const float Alpha = FMath::Clamp(1.f - FadeElapsed / FMath::Max(Config->FadeOutDuration, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
		SetGlobalOpacity(Alpha);
		if (Alpha <= 0.f) Hide();
	}
}

// ====== Route ====== //

void AWzPathGuideRouteActor::ShowRoute()
{
	FVector NewBeacon;
	if (!PathGuide->GetBeaconLocation(NewBeacon)) return;

	// Drape on the ground (the pawn must not block the traces)
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const PathGuide::FGroundTracer Trace(GetWorld(), Config->GroundTraceChannel, PC ? PC->GetPawn() : nullptr);
	PathGuide::Ribbon::FParams Params = MakeTraceParams();
	Params.Width = Config->RibbonWidth;

	FPathGuideRibbon NewRibbon;
	PathGuide::Ribbon::Build(PathGuide->GetRoute(), Params, Trace, NewRibbon);
	if (!NewRibbon.IsValid()) return;

	Ribbon = MoveTemp(NewRibbon);
	BeaconLocation = NewBeacon;

	// Materials, ribbon, ring, column
	CreateMaterials();
	PushStaticParameters();
	BuildRibbonSections();
	BuildRingSection();
	BuildColumn();

	// The route starts unconsumed
	FirstVisibleChunk = INDEX_NONE;
	LastVisibleChunk = INDEX_NONE;
	LastPushedProgress = -1.f;
	Progress = 0.f;
	if (RouteMaterial) RouteMaterial->SetScalarParameterValue(PlayerDistParam, Progress);
	UpdateChunkVisibility(true);

	// A re-route stays on screen; otherwise fade in
	if (bVisible && Fade != EFade::Out) return;

	bVisible = true;
	if (Config->FadeInDuration > 0.f)
	{
		Fade = EFade::In;
		FadeElapsed = 0.f;
		SetGlobalOpacity(0.f);
		SetActorTickEnabled(true);
	}
	else
	{
		Fade = EFade::None;
		SetGlobalOpacity(1.f);
	}
}

void AWzPathGuideRouteActor::SetProgress(const float DistanceAlong)
{
	// Push only when it moved enough
	Progress = DistanceAlong;
	if (RouteMaterial && FMath::Abs(Progress - LastPushedProgress) > 0.5f)
	{
		RouteMaterial->SetScalarParameterValue(PlayerDistParam, Progress);
		LastPushedProgress = Progress;
	}
	UpdateChunkVisibility(false);
}

void AWzPathGuideRouteActor::FadeOut()
{
	if (!bVisible || Fade == EFade::Out) return;

	if (Config->FadeOutDuration <= 0.f)
	{
		Hide();

		return;
	}

	Fade = EFade::Out;
	FadeElapsed = 0.f;
	SetActorTickEnabled(true);
}

void AWzPathGuideRouteActor::Hide()
{
	Mesh->ClearAllMeshSections();
	Column->SetVisibility(false);
	Chunks.Reset();
	Ribbon = FPathGuideRibbon();
	Fade = EFade::None;
	bVisible = false;
	SetActorTickEnabled(false);
}

// ====== Visuals ====== //

PathGuide::Ribbon::FParams AWzPathGuideRouteActor::MakeTraceParams() const
{
	PathGuide::Ribbon::FParams Params;
	Params.GroundOffset = Config->GroundOffset;
	Params.TraceUp = Config->TraceUp;
	Params.TraceDown = Config->TraceDown;
	Params.SpikeThreshold = Config->SpikeThreshold;

	return Params;
}

void AWzPathGuideRouteActor::CreateMaterials()
{
	// One dynamic instance per material
	auto MakeInstance = [this](const TSoftObjectPtr<UMaterialInterface>& Source, TObjectPtr<UMaterialInstanceDynamic>& InOutInstance)
	{
		if (!InOutInstance)
		{
			if (UMaterialInterface* Parent = Source.LoadSynchronous()) InOutInstance = UMaterialInstanceDynamic::Create(Parent, this);
			else UE_LOG(LogPathGuide, Warning, TEXT("A route material is not set in the Path Guide config; the default material is used."));
		}
	};

	MakeInstance(Config->RouteMaterial, RouteMaterial);
	MakeInstance(Config->RingMaterial, RingMaterial);
	MakeInstance(Config->ColumnMaterial, ColumnMaterial);
}

void AWzPathGuideRouteActor::PushStaticParameters() const
{
	const float Emissive = Config->EmissiveIntensity;

	if (RouteMaterial)
	{
		RouteMaterial->SetScalarParameterValue(TEXT("Behind"), Config->ConsumeBehind);
		RouteMaterial->SetScalarParameterValue(TEXT("ArrowLen"), Config->ArrowSection);
		RouteMaterial->SetScalarParameterValue(TEXT("Speed"), Config->ChevronSpeed);
		RouteMaterial->SetScalarParameterValue(TEXT("Spacing"), Config->ChevronSpacing);
		RouteMaterial->SetScalarParameterValue(TEXT("FadeStart"), Config->FadeStart);
		RouteMaterial->SetScalarParameterValue(TEXT("MaxVis"), Config->MaxVisible);
		RouteMaterial->SetScalarParameterValue(TEXT("FarOpacity"), Config->FarOpacity);
		RouteMaterial->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		RouteMaterial->SetVectorParameterValue(TEXT("LineColor"), Config->LineColor);
		RouteMaterial->SetVectorParameterValue(TEXT("ChevColor"), Config->ChevronColor);
	}
	if (RingMaterial)
	{
		RingMaterial->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		RingMaterial->SetVectorParameterValue(TEXT("BeaconColor"), Config->BeaconColor);
	}
	if (ColumnMaterial)
	{
		ColumnMaterial->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		ColumnMaterial->SetScalarParameterValue(TEXT("ColumnHeight"), Config->ColumnHeight);
		ColumnMaterial->SetVectorParameterValue(TEXT("BeaconColor"), Config->BeaconColor);
	}
}

void AWzPathGuideRouteActor::SetGlobalOpacity(const float NewOpacity) const
{
	for (const auto Material : { RouteMaterial.Get(), RingMaterial.Get(), ColumnMaterial.Get() })
	{
		if (Material) Material->SetScalarParameterValue(GlobalOpacityParam, NewOpacity);
	}
}

void AWzPathGuideRouteActor::CreateStripSection(const TArray<FVector>& Vertices, const TArray<FVector>& Normals, const TArray<FVector2D>& UV, UMaterialInterface* Material, const int32 LanesPerSample) const
{
	// Two triangles per quad
	const int32 Rows = Vertices.Num() / LanesPerSample;
	TArray<int32> Triangles;
	Triangles.Reserve(Vertices.Num() * 6);
	for (int32 Row = 0; Row + 1 < Rows; ++Row)
	{
		for (int32 Lane = 0; Lane + 1 < LanesPerSample; ++Lane)
		{
			const int32 A = Row * LanesPerSample + Lane;
			const int32 B = A + LanesPerSample;
			Triangles.Append({ A, B, A + 1, A + 1, B, B + 1 });
		}
	}

	const int32 Section = Mesh->GetNumSections();
	Mesh->CreateMeshSection(Section, Vertices, Triangles, Normals, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	if (Material) Mesh->SetMaterial(Section, Material);
}

void AWzPathGuideRouteActor::BuildRibbonSections()
{
	Mesh->ClearAllMeshSections();
	Chunks.Reset();

	// One mesh section per chunk
	const int32 Count = Ribbon.Num();
	int32 Start = 0;
	for (int32 Index = 1; Index < Count; ++Index)
	{
		if (Index != Count - 1 && Ribbon.Distances[Index] - Ribbon.Distances[Start] < ChunkLength) continue;

		// Left/centre/right row per sample; UV.x = distance along, UV.y = 0..1 across
		const int32 Samples = Index - Start + 1;
		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UV;
		Vertices.Reserve(Samples * 3);
		Normals.Reserve(Samples * 3);
		UV.Reserve(Samples * 3);

		for (int32 Sample = Start; Sample <= Index; ++Sample)
		{
			Vertices.Add(Ribbon.Lefts[Sample]);
			Vertices.Add(Ribbon.Centers[Sample]);
			Vertices.Add(Ribbon.Rights[Sample]);
			Normals.Add(Ribbon.Normals[Sample]);
			Normals.Add(Ribbon.Normals[Sample]);
			Normals.Add(Ribbon.Normals[Sample]);
			UV.Add({ Ribbon.Distances[Sample], 0.0 });
			UV.Add({ Ribbon.Distances[Sample], 0.5 });
			UV.Add({ Ribbon.Distances[Sample], 1.0 });
		}

		// Section and the stretch it covers
		CreateStripSection(Vertices, Normals, UV, RouteMaterial, 3);

		FChunk Chunk;
		Chunk.StartDistance = Ribbon.Distances[Start];
		Chunk.EndDistance = Ribbon.Distances[Index];
		Chunks.Add(Chunk);
		Start = Index;
	}
}

void AWzPathGuideRouteActor::BuildRingSection() const
{
	const float Inner = FMath::Max(Config->RingRadius - Config->RingWidth * 0.5f, 1.f);
	const float Outer = Config->RingRadius + Config->RingWidth * 0.5f;

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UV;

	// One ground tracer for the whole ring
	const PathGuide::FGroundTracer Trace(GetWorld(), Config->GroundTraceChannel, this);
	const PathGuide::Ribbon::FParams TraceParams = MakeTraceParams();

	// Inner and outer vertex at every angle, draped on the ground
	for (int32 Segment = 0; Segment <= RingSegments; ++Segment)
	{
		const double Fraction = static_cast<double>(Segment) / RingSegments;
		const double Angle = Fraction * UE_DOUBLE_TWO_PI;
		const FVector2D Direction{ FMath::Cos(Angle), FMath::Sin(Angle) };

		for (int32 Side = 0; Side < 2; ++Side)
		{
			const double Radius = Side == 0 ? Inner : Outer;
			const double X = BeaconLocation.X + Direction.X * Radius;
			const double Y = BeaconLocation.Y + Direction.Y * Radius;

			// A miss keeps the beacon height
			FVector Position{ X, Y, BeaconLocation.Z };
			FVector Normal = FVector::UpVector;
			PathGuide::Ribbon::TraceGround(Trace, X, Y, BeaconLocation.Z, TraceParams, Position, Normal);
			Vertices.Add(Position + Normal * Config->GroundOffset);
			Normals.Add(Normal);
			UV.Add({ Fraction, static_cast<double>(Side) });
		}
	}

	CreateStripSection(Vertices, Normals, UV, RingMaterial, 2);
}

void AWzPathGuideRouteActor::BuildColumn() const
{
	UStaticMesh* ColumnAsset = Config->ColumnMesh.LoadSynchronous();
	if (!ColumnAsset)
	{
		Column->SetVisibility(false);

		return;
	}

	Column->SetStaticMesh(ColumnAsset);
	if (ColumnMaterial) Column->SetMaterial(0, ColumnMaterial);

	// Size from the mesh bounds
	const FBoxSphereBounds Bounds = ColumnAsset->GetBounds();
	const FVector Extent = Bounds.BoxExtent.ComponentMax(FVector{ UE_KINDA_SMALL_NUMBER });
	const FVector Scale{ Config->ColumnRadius / Extent.X, Config->ColumnRadius / Extent.Y, Config->ColumnHeight * 0.5 / Extent.Z };

	Column->SetWorldScale3D(Scale);
	Column->SetWorldLocation(BeaconLocation + FVector{ 0.0, 0.0, Config->ColumnHeight * 0.5 } - Bounds.Origin * Scale);
	Column->SetVisibility(true);
}

void AWzPathGuideRouteActor::UpdateChunkVisibility(const bool bForce)
{
	if (Chunks.IsEmpty()) return;

	// Chunks overlapping the visible stretch
	int32 First = INDEX_NONE;
	int32 Last = INDEX_NONE;
	const float MinDistance = Progress - Config->ConsumeBehind;
	const float MaxDistance = Progress + Config->MaxVisible;
	for (int32 Index = 0; Index < Chunks.Num(); ++Index)
	{
		if (Chunks[Index].EndDistance >= MinDistance && Chunks[Index].StartDistance <= MaxDistance)
		{
			if (First == INDEX_NONE) First = Index;
			Last = Index;
		}
	}

	// Only touch the sections when the range changed
	if (!bForce && First == FirstVisibleChunk && Last == LastVisibleChunk) return;

	FirstVisibleChunk = First;
	LastVisibleChunk = Last;
	for (int32 Index = 0; Index < Chunks.Num(); ++Index)
	{
		Mesh->SetMeshSectionVisible(Index, First != INDEX_NONE && Index >= First && Index <= Last);
	}
}
