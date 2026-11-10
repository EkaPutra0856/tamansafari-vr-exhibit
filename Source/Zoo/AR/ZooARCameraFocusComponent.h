#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZooARCameraFocusComponent.generated.h"

class APawn;
class APlayerController;
class AZooARExhibit;
class UBoxComponent;

/** Owns one temporary desktop camera focus per player and balances only its own input locks. */
UCLASS(Transient)
class ZOO_API UZooARCameraFocusComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UZooARCameraFocusComponent();
    static UZooARCameraFocusComponent* GetOrCreate(APlayerController* Player);
    bool StartFocus(AZooARExhibit* Exhibit, float MaxDuration);
    bool StartDeckWalk(AZooARExhibit* Exhibit);
    void FinishAnimalTurn(const AZooARExhibit* Exhibit);
    void StopFocus(const AZooARExhibit* Exhibit);
    bool IsFocusingFor(const AZooARExhibit* Exhibit) const;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    friend struct FZooAnimalTestAccess;

    enum class EFocusPhase : uint8 { None, DeckWalk, AnimalFocus };

    bool AcquireFocus(AZooARExhibit* Exhibit);
    bool ResolveDeckDestination(const AZooARExhibit* Exhibit, const APlayerController* Player, FVector& Destination) const;
    void UpdateDeckWalk(float DeltaTime);
    void UpdateAnimalFocus(float DeltaTime);
    void ApplyAnimalFocus(float Alpha);
    void StopVisitorMovement();
    void AbortDeckSequence();
    void ReleaseFocus();
    FVector GetFocusPoint(const AZooARExhibit* Exhibit) const;

    TWeakObjectPtr<AZooARExhibit> FocusExhibit;
    TWeakObjectPtr<APawn> FocusPawn;
    TWeakObjectPtr<UBoxComponent> DeckBox;
    FVector DeckDestination = FVector::ZeroVector;
    FRotator InitialRotation = FRotator::ZeroRotator;
    float Elapsed = 0.f;
    float TimeLimit = 0.f;
    bool bOwnsInputLocks = false;
    bool bGuidedSequence = false;
    bool bAnimalTurnFinished = false;
    EFocusPhase Phase = EFocusPhase::None;
};
