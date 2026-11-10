#include "WzWidget.h"

#include "Blueprint/WidgetTree.h"
#include "WidgetController.h"

// ====== Controller ====== //

void UWzWidget::SetWidgetController(UWzWidgetController* InWidgetController)
{
	WidgetController = InWidgetController;

	// Hand it down; each child recurses into its own tree
	WidgetTree->ForEachWidget([InWidgetController](UWidget* Widget)
	{
		if (UWzWidget* Child = Cast<UWzWidget>(Widget)) Child->SetWidgetController(InWidgetController);
	});

	WidgetControllerSet();
}

void UWzWidget::WidgetControllerSet_Implementation() {}
