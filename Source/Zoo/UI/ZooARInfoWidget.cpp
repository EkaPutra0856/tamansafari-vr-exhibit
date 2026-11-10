#include "UI/ZooARInfoWidget.h"
#include "AR/ZooARExhibit.h"

// Compatibility bridge only. Layout, data presentation and interaction live in Widget Blueprints.
TSharedRef<SWidget> UZooARInfoWidget::RebuildWidget()
{
    return Super::RebuildWidget();
}

void UZooARInfoWidget::BindExhibit(AZooARExhibit* InExhibit)
{
    Exhibit = InExhibit;
    if (UFunction* Initialize = FindFunction(TEXT("InitializeExhibit")))
    {
        struct { AZooARExhibit* OwnerExhibit; } Params{InExhibit};
        ProcessEvent(Initialize, &Params);
    }
}

void UZooARInfoWidget::RefreshInformation()
{
    if (UFunction* Sync = FindFunction(TEXT("SynchronizeExhibit"))) ProcessEvent(Sync, nullptr);
}

void UZooARInfoWidget::HandleInformationClick() {}
