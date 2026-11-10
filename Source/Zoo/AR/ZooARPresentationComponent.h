#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZooARPresentationComponent.generated.h"

class AZooARExhibit;
class UAnimSequence;

UENUM(BlueprintType)
enum class EZooARVisualPhase : uint8
{
    Hidden,
    In,
    Moving,
    Idle,
    Out
};

/** Drives the separate IN, optional walk, IDLE, and OUT phases (timing and animations) without changing NPC behavior. The look is the exhibit's hologram. */
UCLASS()
class ZOO_API UZooARPresentationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UZooARPresentationComponent();

    /** Full IN effect duration, including the final ring fade. Movement starts afterwards. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Presentation", meta=(ClampMin="0.01", Units="s"))
    float InDuration = 3.9f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Presentation", meta=(ClampMin="0.01", Units="s"))
    float OutDuration = 100.f / 30.f;

    /** Folder of the walk and idle animations, one sub-folder per species; source animal assets are only referenced. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Presentation")
    FString AssetRoot = TEXT("/Game/Zoo/ARPresentation_20261022");
    void Initialize(AZooARExhibit* OwnerExhibit);
    void ResetPresentation();
    void BeginIn();
    void BeginOut();
    void Advance(float DeltaSeconds);

    UFUNCTION(BlueprintPure, Category="AR|Presentation")
    EZooARVisualPhase GetVisualPhase() const { return Phase; }

    bool IsComplete() const { return Phase == EZooARVisualPhase::Idle; }
    float GetProgress() const;
    float GetInDuration() const;
    float GetOutDuration() const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void SetPhase(EZooARVisualPhase NewPhase);
    void PlayBodyAnimation(UAnimSequence* Animation);
    void EnterAfterIn();

    TWeakObjectPtr<AZooARExhibit> Exhibit;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> IdleAnimation;
    EZooARVisualPhase Phase = EZooARVisualPhase::Hidden;
    float PhaseElapsed = 0.f;
    bool bInitialized = false;
};
