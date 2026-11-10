// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Quest/WzNarrativeContext.h"
#include "UObject/Object.h"
#include "WzNarrativeScript.generated.h"

class AActor;

/** Prepares the world for a quest or an objective (spawn actors, ...). Subclass in C++ or Blueprint. */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class NARRATIVESYSTEM_API UWzNarrativeScript : public UObject
{
	GENERATED_BODY()

public:
	// ====== Events ====== //

	/** The quest or objective began. */
	UFUNCTION(BlueprintNativeEvent, Category = "Script")
	void OnBegin(const FWzNarrativeContext& Context);

	/** The quest or objective ended. */
	UFUNCTION(BlueprintNativeEvent, Category = "Script")
	void OnEnd(const FWzNarrativeContext& Context);
};

/** Spawns actors at the player's pawn. */
UCLASS(meta = (DisplayName = "Spawn Actors"))
class NARRATIVESYSTEM_API UWzSpawnActorsScript : public UWzNarrativeScript
{
	GENERATED_BODY()

public:
	// ====== Spawn ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<TSubclassOf<AActor>> Classes;

	/** From the pawn, in its local space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FVector Offset = FVector::ZeroVector;

	/** Destroy what was spawned when the quest or objective ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	bool bDestroyOnEnd = false;

public:
	// ====== Events ====== //

	virtual void OnBegin_Implementation(const FWzNarrativeContext& Context) override;
	virtual void OnEnd_Implementation(const FWzNarrativeContext& Context) override;

private:
	// ====== State ====== //

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Spawned;
};