#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WidgetController.generated.h"

class APlayerController;
class APlayerState;

/** What a widget controller needs to reach the game model. */
USTRUCT(BlueprintType)
struct UISYSTEM_API FWidgetControllerParams
{
	GENERATED_BODY()

	FWidgetControllerParams() {}

	FWidgetControllerParams(APlayerController* InPlayerController, APlayerState* InPlayerState)
		: PlayerController(InPlayerController), PlayerState(InPlayerState) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reference")
	TObjectPtr<APlayerController> PlayerController = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reference")
	TObjectPtr<APlayerState> PlayerState = nullptr;
};

/** Sits between the game model and a widget: listens to the model, broadcasts delegates the widget binds to. */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API UWzWidgetController : public UObject
{
	GENERATED_BODY()

public:
	// ====== Setup ====== //

	UFUNCTION(BlueprintCallable, Category = "Setup")
	void SetWidgetControllerParams(const FWidgetControllerParams& Params);

	/** Pushes the model's current values through the delegates, once the widget is bound. */
	UFUNCTION(BlueprintCallable, Category = "Setup")
	virtual void BroadcastInitialValues();

	/** Subscribes to the model's delegates. */
	virtual void BindCallbacksToDependencies();

protected:
	// ====== Reference ====== //

	UPROPERTY(BlueprintReadOnly, Category = "Reference")
	TObjectPtr<APlayerController> PlayerController;

	UPROPERTY(BlueprintReadOnly, Category = "Reference")
	TObjectPtr<APlayerState> PlayerState;
};
