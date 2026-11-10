#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WidgetController.h"
#include "WzWidget.h"
#include "WzUILibrary.generated.h"

/** The widget CreateWzWidget made and the controller wired to it. */
template <typename TWidget, typename TController>
struct TWzWidgetPair
{
	TWidget* Widget = nullptr;
	TController* Controller = nullptr;
};

UCLASS()
class UISYSTEM_API UWzUILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ====== Create ====== //

	/** Creates the widget and its controller, wires them together and pushes the initial values. The caller adds it to the viewport. */
	template <typename TWidget, typename TController>
	static TWzWidgetPair<TWidget, TController> CreateWzWidget(APlayerController* PlayerController, TSubclassOf<TWidget> WidgetClass, TSubclassOf<TController> ControllerClass, const FWidgetControllerParams& Params)
	{
		static_assert(TIsDerivedFrom<TWidget, UWzWidget>::Value, "TWidget must derive from UWzWidget");
		static_assert(TIsDerivedFrom<TController, UWzWidgetController>::Value, "TController must derive from UWzWidgetController");
		checkf(WidgetClass, TEXT("CreateWzWidget: WidgetClass is not set."));
		checkf(ControllerClass, TEXT("CreateWzWidget: ControllerClass is not set."));

		// Widget, and a controller that lives and dies with it
		TWidget* Widget = CreateWidget<TWidget>(PlayerController, WidgetClass);
		TController* Controller = NewObject<TController>(Widget, ControllerClass);

		// Wire: model -> controller -> widget
		Controller->SetWidgetControllerParams(Params);
		Controller->BindCallbacksToDependencies();
		Widget->SetWidgetController(Controller);

		// Widget is bound, push the starting values
		Controller->BroadcastInitialValues();

		return { Widget, Controller };
	}
};