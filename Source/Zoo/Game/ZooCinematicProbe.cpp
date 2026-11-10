// Copyright Epic Games, Inc. All Rights Reserved.

// Cinematic tooling (development builds only): "zoo.cine.Probe <in.json> <out.json>" measures the running level for the
// L_Zoo_Cine camera scripts (Tools/Cine): surfaces under ground points, and the clearance of camera paths (rays + sphere sweeps
// against complex collision, which the editor commandlet cannot trace). Nothing here runs unless the command is typed.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/TriggerBase.h"
#include "GameFramework/Volume.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ZooCineProbe
{
	const FName IgnoreTraceTag{ TEXT("ARIgnoreTrace") };

	/** Solid world only: no animals or visitor (object types), no triggers, volumes, invisible walls or sky dome. */
	FCollisionQueryParams MakeParams(UWorld* World)
	{
		FCollisionQueryParams Params{ SCENE_QUERY_STAT(ZooCineProbe), true };
		for (TActorIterator<AActor> It{ World }; It; ++It)
		{
			const bool bNotSolid = It->IsA<ATriggerBase>() || It->IsA<AVolume>() || It->ActorHasTag(IgnoreTraceTag);
			if (bNotSolid || It->GetActorNameOrLabel().StartsWith(TEXT("SKY_"))) Params.AddIgnoredActor(*It);
		}

		return Params;
	}

	FVector ReadVector(const TSharedPtr<FJsonValue>& Value)
	{
		const TArray<TSharedPtr<FJsonValue>>& A = Value->AsArray();

		return { A[0]->AsNumber(), A[1]->AsNumber(), A.Num() > 2 ? A[2]->AsNumber() : 0. };
	}

	FString Label(const FHitResult& Hit)
	{
		const AActor* Actor = Hit.GetActor();
		const UPrimitiveComponent* Component = Hit.GetComponent();
		FString Text = Actor ? Actor->GetActorNameOrLabel() : TEXT("?");
		if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Component))
		{
			if (Instanced->GetStaticMesh()) Text += TEXT("/") + Instanced->GetStaticMesh()->GetName();
		}

		return Text;
	}

	/** Every surface straight down from high above, top first. */
	TSharedPtr<FJsonValue> ProbeGround(UWorld* World, const FVector& Point, FCollisionQueryParams Params, const FCollisionObjectQueryParams& Objects)
	{
		TArray<TSharedPtr<FJsonValue>> Surfaces;
		FVector Start{ Point.X, Point.Y, 8000. };
		for (int32 Index = 0; Index < 8; ++Index)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(Hit, Start, FVector{ Point.X, Point.Y, -4000. }, Objects, Params)) break;
			TArray<TSharedPtr<FJsonValue>> Entry{ MakeShared<FJsonValueNumber>(FMath::RoundToInt(Hit.ImpactPoint.Z)), MakeShared<FJsonValueString>(Label(Hit)) };
			Surfaces.Add(MakeShared<FJsonValueArray>(Entry));
			if (Hit.GetComponent()) Params.AddIgnoredComponent(Hit.GetComponent());
			Start = Hit.ImpactPoint + FVector{ 0., 0., 1. };
		}

		return MakeShared<FJsonValueArray>(Surfaces);
	}

	/** Nearest surface around a camera point (26 rays) and what a sphere hits sweeping in from the previous point. */
	TSharedPtr<FJsonObject> ProbePoint(UWorld* World, const FVector& Point, const FVector* Previous, double MaxDistance, double Radius, const FCollisionQueryParams& Params, const FCollisionObjectQueryParams& Objects)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		double Nearest = MaxDistance;
		FString NearestLabel;
		for (int32 X = -1; X <= 1; ++X) for (int32 Y = -1; Y <= 1; ++Y) for (int32 Z = -1; Z <= 1; ++Z)
		{
			if (X == 0 && Y == 0 && Z == 0) continue;
			FHitResult Hit;
			const FVector Direction = FVector{ double(X), double(Y), double(Z) }.GetSafeNormal();
			if (!World->LineTraceSingleByObjectType(Hit, Point, Point + Direction * MaxDistance, Objects, Params) || Hit.Distance >= Nearest) continue;
			Nearest = Hit.Distance;
			NearestLabel = Label(Hit);
		}
		Out->SetNumberField(TEXT("d"), FMath::RoundToInt(Nearest));
		if (!NearestLabel.IsEmpty()) Out->SetStringField(TEXT("a"), NearestLabel);

		// Started inside geometry, or crossed it on the way in
		if (Previous)
		{
			FHitResult Hit;
			if (World->SweepSingleByObjectType(Hit, *Previous, Point, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Params))
			{
				Out->SetStringField(TEXT("sweep"), Label(Hit));
				Out->SetNumberField(TEXT("sweep_z"), FMath::RoundToInt(Hit.ImpactPoint.Z));
			}
		}

		return Out;
	}

	void Run(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2 || !World)
		{
			UE_LOG(LogTemp, Error, TEXT("[ZooCineProbe] usage: zoo.cine.Probe <in.json> <out.json>"));
			return;
		}
		FString Text;
		TSharedPtr<FJsonObject> In;
		if (!FFileHelper::LoadFileToString(Text, *Args[0]) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), In) || !In)
		{
			UE_LOG(LogTemp, Error, TEXT("[ZooCineProbe] cannot read %s"), *Args[0]);
			return;
		}

		// Static and dynamic world geometry, complex collision
		const FCollisionQueryParams Params = MakeParams(World);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		const double MaxDistance = In->HasField(TEXT("max")) ? In->GetNumberField(TEXT("max")) : 400.;
		const double Radius = In->HasField(TEXT("radius")) ? In->GetNumberField(TEXT("radius")) : 60.;
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();

		// Ground columns
		TArray<TSharedPtr<FJsonValue>> Ground;
		const TArray<TSharedPtr<FJsonValue>>* GroundIn = nullptr;
		if (In->TryGetArrayField(TEXT("ground"), GroundIn))
		{
			for (const auto& Value : *GroundIn) Ground.Add(ProbeGround(World, ReadVector(Value), Params, Objects));
		}
		Out->SetArrayField(TEXT("ground"), Ground);

		// Camera paths
		TSharedPtr<FJsonObject> PathsOut = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* PathsIn = nullptr;
		if (In->TryGetObjectField(TEXT("paths"), PathsIn))
		{
			for (const auto& Pair : (*PathsIn)->Values)
			{
				TArray<TSharedPtr<FJsonValue>> Results;
				FVector Previous;
				bool bHasPrevious = false;
				for (const auto& Value : Pair.Value->AsArray())
				{
					const FVector Point = ReadVector(Value);
					Results.Add(MakeShared<FJsonValueObject>(ProbePoint(World, Point, bHasPrevious ? &Previous : nullptr, MaxDistance, Radius, Params, Objects)));
					Previous = Point;
					bHasPrevious = true;
				}
				PathsOut->SetArrayField(Pair.Key, Results);
			}
		}
		Out->SetObjectField(TEXT("paths"), PathsOut);

		// Actor placement and bounds, by label prefix
		TSharedPtr<FJsonObject> ActorsOut = MakeShared<FJsonObject>();
		const TArray<TSharedPtr<FJsonValue>>* ActorsIn = nullptr;
		if (In->TryGetArrayField(TEXT("actors"), ActorsIn))
		{
			for (TActorIterator<AActor> It{ World }; It; ++It)
			{
				const FString Name = It->GetActorNameOrLabel();
				if (!ActorsIn->ContainsByPredicate([&Name](const TSharedPtr<FJsonValue>& Prefix) { return Name.StartsWith(Prefix->AsString()); })) continue;
				FVector Origin, Extent;
				It->GetActorBounds(false, Origin, Extent, true);
				const FVector Location = It->GetActorLocation();
				const FRotator Rotation = It->GetActorRotation();
				TArray<TSharedPtr<FJsonValue>> Values;
				for (const double Number : { Location.X, Location.Y, Location.Z, Rotation.Yaw, Origin.X, Origin.Y, Origin.Z, Extent.X, Extent.Y, Extent.Z }) Values.Add(MakeShared<FJsonValueNumber>(FMath::RoundToInt(Number)));
				ActorsOut->SetArrayField(Name, Values);
			}
		}
		Out->SetObjectField(TEXT("actors"), ActorsOut);

		// Write
		FString Json;
		FJsonSerializer::Serialize(Out.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
		FFileHelper::SaveStringToFile(Json, *Args[1]);
		UE_LOG(LogTemp, Log, TEXT("[ZooCineProbe] %d ground columns, %d paths -> %s"), Ground.Num(), PathsOut->Values.Num(), *Args[1]);
	}

	FAutoConsoleCommandWithWorldAndArgs ProbeCommand{
		TEXT("zoo.cine.Probe"),
		TEXT("Cinematic tooling: zoo.cine.Probe <in.json> <out.json> (ground surfaces and camera path clearance of the running level)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Run) };
}

#endif
