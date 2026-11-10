#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WzWidget.generated.h"

class UWzWidgetController;

/** A widget driven by a widget controller; it never touches the game model directly. */
UCLASS()
class UISYSTEM_API UWzWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ====== Controller ====== //

	/** Stores the controller, passes it to every WzWidget child, then fires WidgetControllerSet. */
	UFUNCTION(BlueprintCallable, Category = "Controller")
	void SetWidgetController(UWzWidgetController* InWidgetController);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Controller")
	TObjectPtr<UWzWidgetController> WidgetController;

	/** The controller is ready: bind to its delegates here. */
	UFUNCTION(BlueprintNativeEvent, Category = "Controller")
	void WidgetControllerSet();
};
