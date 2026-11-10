#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data.h"
#include "InteractionInterface.generated.h"

class AActor;
class AController;

UINTERFACE(MinimalAPI, Blueprintable)
class UWzInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by anything the player can look at and interact with (C++ or Blueprint). */
class INTERACTIONSYSTEM_API IWzInteractionInterface
{
	GENERATED_BODY()

public:
	// ====== Overlap ====== //

	/** Interactor started looking at this. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnBeginOverlap(AActor* CurrentActor);

	/** Interactor stopped looking at this. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnEndOverlap(AActor* CurrentActor);

public:
	// ====== Interact ====== //

	/** Interaction begins (or ticks, when continuous). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction", meta = (AutoCreateRefTerm = "Data"))
	void StartInteract(AController* Controller, const FInteractionData& Data);

	/** Interaction ends. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void EndInteract(AController* Controller);

	/** True if the interaction is held, so the highlight hides while it runs. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool IsContinuous() const;
};
