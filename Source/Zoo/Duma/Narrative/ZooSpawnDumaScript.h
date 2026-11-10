// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Quest/WzNarrativeScript.h"
#include "ZooSpawnDumaScript.generated.h"

class AZooDuma;

/** Spawns Duma at the visitor, with a first appear as slow as the quest wants, and optionally sends him away (the full dematerialize) when the quest or objective ends. A Duma that already exists (hidden by a despawn) is brought back instead of spawned again. */
UCLASS(meta = (DisplayName = "Spawn Duma"))
class UZooSpawnDumaScript : public UWzNarrativeScript
{
	GENERATED_BODY()

public:
	// ====== Events ====== //

	virtual void OnBegin_Implementation(const FWzNarrativeContext& Context) override;
	virtual void OnEnd_Implementation(const FWzNarrativeContext& Context) override;

protected:
	// ====== Spawn ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TSubclassOf<AZooDuma> DumaClass;

	/** From the pawn, in its local space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FVector Offset = FVector::ZeroVector;

	/** Seconds the first appear takes (the slow intro). 0 or less = the hologram profile's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0"))
	float AppearSeconds = 0.f;

	/** Dematerialize him when the quest or objective ends (he is only hidden, a later spawn brings him back). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	bool bDespawnOnEnd = false;

private:
	// ====== State ====== //

	TWeakObjectPtr<AZooDuma> Duma;
};