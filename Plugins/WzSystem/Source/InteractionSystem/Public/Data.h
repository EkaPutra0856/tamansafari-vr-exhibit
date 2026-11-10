#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Data.generated.h"

/** Payload passed to an interactable; the concrete struct inside is up to the target. */
USTRUCT(BlueprintType)
struct INTERACTIONSYSTEM_API FInteractionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FInstancedStruct Data;
};
