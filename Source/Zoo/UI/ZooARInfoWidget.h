#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZooARInfoWidget.generated.h"

class AZooARExhibit;
class UButton;
class UTextBlock;
class UVerticalBox;

/** World-space, mouse-interactive animal information card. */
UCLASS(Blueprintable)
class ZOO_API UZooARInfoWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void BindExhibit(AZooARExhibit* InExhibit);
    void RefreshInformation();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UFUNCTION() void HandleInformationClick();
    TWeakObjectPtr<AZooARExhibit> Exhibit;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SpeciesText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ScientificText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> InformationText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ButtonText;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> InformationBody;
    UPROPERTY(Transient) TObjectPtr<UButton> InformationButton;
};
