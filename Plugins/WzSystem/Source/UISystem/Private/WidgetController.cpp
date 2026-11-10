#include "WidgetController.h"

// ====== Setup ====== //

void UWzWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& Params)
{
	PlayerController = Params.PlayerController;
	PlayerState = Params.PlayerState;
}

void UWzWidgetController::BroadcastInitialValues() {}

void UWzWidgetController::BindCallbacksToDependencies() {}
