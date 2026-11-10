#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/HitResult.h"
#include "AR/ZooARPresentationComponent.h"
#include "ZooARExhibit.generated.h"

class AZooARExhibit;
class APlayerController;
class UBoxComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UWidgetComponent;
class UWidgetInteractionComponent;
class UZooARInfoWidget;
class AZooAnimal;
class UZooARCameraFocusComponent;
class UZooHologramComponent;

UENUM(BlueprintType)
enum class EZooARActivationMode : uint8
{
    Immediate,
    WaitForAnimal
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZooARExhibitEventSignature, AZooARExhibit*, Exhibit);

/** An enclosure-anchored hologram, activated by its visitor zone. */
UCLASS(Blueprintable)
class ZOO_API AZooARExhibit : public AActor
{
    GENERATED_BODY()

public:
    AZooARExhibit();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="AR|Activation")
    TObjectPtr<AActor> ViewingZone;

    /** Immediate preserves the original entrance. WaitForAnimal delays all AR visuals until animal arrival or timeout. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Activation")
    EZooARActivationMode ActivationMode = EZooARActivationMode::Immediate;

    /** Walk to the ViewingZone box center before selecting an animal. Applies only to WaitForAnimal. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Deck", meta=(EditCondition="ActivationMode == EZooARActivationMode::WaitForAnimal"))
    bool bWalkToDeckCenterBeforeApproach = true;

    /** Stand-point offset in ViewingZone box axes, in world centimeters, independent of box scale. The destination is projected onto walkable floor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Deck", meta=(EditCondition="bWalkToDeckCenterBeforeApproach && ActivationMode == EZooARActivationMode::WaitForAnimal", Units="cm"))
    FVector DeckCenterOffset = FVector::ZeroVector;

    /** Retained for existing Blueprint references. Deck walking now faces the stand point automatically, and this offset is ignored. */
    UPROPERTY(BlueprintReadOnly, Category="AR|Deck", meta=(DeprecatedProperty, DeprecationMessage="Deck walking now faces the stand point automatically."))
    float DeckFacingYawOffset = 0.f;

    /** Horizontal distance from the projected stand point that completes the automatic walk. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Deck", meta=(EditCondition="bWalkToDeckCenterBeforeApproach && ActivationMode == EZooARActivationMode::WaitForAnimal", ClampMin="1", Units="cm"))
    float DeckArrivalAcceptanceRadius = 20.f;

    /** Cancel the deck sequence and restore input if the automatic walk takes longer than this. No AR presentation starts on cancellation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Deck", meta=(EditCondition="bWalkToDeckCenterBeforeApproach && ActivationMode == EZooARActivationMode::WaitForAnimal", ClampMin="0.1", Units="s"))
    float DeckWalkTimeout = 8.f;

    /** Maximum wait from deck-center arrival, including the animal-facing turn. Without the deck walk, count from zone entry. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Activation", meta=(EditCondition="ActivationMode == EZooARActivationMode::WaitForAnimal", ClampMin="0", Units="s"))
    float AnimalArrivalTimeout = 2.f;

    /** Smoothly focus on the animal before it approaches. The deck walk always locks manual movement and look input; this option controls the subsequent animal-facing turn. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Camera", meta=(EditCondition="ActivationMode == EZooARActivationMode::WaitForAnimal"))
    bool bEnableArrivalCameraFocus = true;

    /** Smooth turn duration for facing the walking destination and then the animal. The animal turn is capped by the remaining wait and camera time limit. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Camera", meta=(EditCondition="bEnableArrivalCameraFocus && ActivationMode == EZooARActivationMode::WaitForAnimal", ClampMin="0.01", Units="s"))
    float CameraFocusBlendTime = .8f;

    /** Optional skeletal socket on the approaching animal. When empty or missing, focus on the mesh bounds center. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Camera", meta=(EditCondition="bEnableArrivalCameraFocus && ActivationMode == EZooARActivationMode::WaitForAnimal"))
    FName CameraFocusSocket;

    /** Focus offset in animal axes, in centimeters. With no available animal, use the exhibit target and exhibit axes. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Camera", meta=(EditCondition="bEnableArrivalCameraFocus && ActivationMode == EZooARActivationMode::WaitForAnimal", Units="cm"))
    FVector CameraFocusOffset = FVector::ZeroVector;

    /** Maximum upward or downward pitch used by automatic camera focus. Roll remains zero. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Camera", meta=(EditCondition="bEnableArrivalCameraFocus && ActivationMode == EZooARActivationMode::WaitForAnimal", ClampMin="0", ClampMax="89", Units="deg"))
    float CameraFocusMaxPitch = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Habitat")
    FBox HabitatBounds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Habitat", meta=(ClampMin="0"))
    float HabitatCornerRadius = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Information")
    FText AnimalName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Information")
    FText ScientificName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Information", meta=(MultiLine=true))
    FText AnimalInformation;

    /** Duration of the visual reveal in seconds. Entrance movement uses EntranceMoveSpeed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Animation", meta=(ClampMin="0.1"))
    float RevealDuration = 2.2f;

    /** Duration of the visual fade-out in seconds. Return movement resets once fully hidden. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Animation", meta=(ClampMin="0.1"))
    float HideDuration = .45f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Animation", meta=(ClampMin="0"))
    float FloatAmplitude = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Animation", meta=(ClampMin="0.01"))
    float FloatFrequency = .22f;

    /** World-space offset from the exhibit anchor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Information")
    FVector PanelOffset = FVector(-330., 0., 410.);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Information", meta=(ClampMin="0.01"))
    float PanelWorldScale = .32f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Information")
    TSubclassOf<UZooARInfoWidget> InformationWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Interaction")
    int32 PointerIndex = 0;

    /** Maximum distance from an animal's feet to its exhibit destination when selecting a visitor. Zero disables selection. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Animals", meta=(ClampMin="0", Units="cm"))
    float MaxApproachDistance = 1000.f;

    /** Move the hologram between entrance locations. WaitForAnimal finishes IN before starting this movement. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Entrance")
    bool bAnimateEntranceMovement = true;

    /** Starting mesh location relative to HologramRoot. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Entrance", meta=(EditCondition="bAnimateEntranceMovement", MakeEditWidget=true))
    FVector EntranceStartLocation = FVector(-300., 0., 0.);

    /** Final mesh location relative to HologramRoot. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Entrance", meta=(EditCondition="bAnimateEntranceMovement || ActivationMode == EZooARActivationMode::WaitForAnimal", MakeEditWidget=true))
    FVector EntranceTargetLocation = FVector::ZeroVector;

    /** Constant travel speed along the entrance path in world centimeters per second, independent of reveal duration. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AR|Entrance", meta=(EditCondition="bAnimateEntranceMovement", ClampMin="0.1", Units="cm/s"))
    float EntranceMoveSpeed = 200.f;

    /** Retained for existing Blueprint references. Entrance movement now uses EntranceMoveSpeed. */
    UPROPERTY(BlueprintReadOnly, Category="AR|Entrance", meta=(DeprecatedProperty, DeprecationMessage="Use EntranceMoveSpeed in centimeters per second instead."))
    float EntranceMoveEndProgress = .75f;

    /** Broadcast when the hologram starts appearing, after the arrival gate in WaitForAnimal mode. */
    UPROPERTY(BlueprintAssignable, Category="AR|Events")
    FZooARExhibitEventSignature OnExhibitAppearStarted;

    /** Broadcast when the visual reveal is complete and the animal has reached its entrance target. */
    UPROPERTY(BlueprintAssignable, Category="AR|Events")
    FZooARExhibitEventSignature OnExhibitAppearCompleted;

    /** Broadcast after the hologram is fully hidden and RevealProgress has returned to zero. */
    UPROPERTY(BlueprintAssignable, Category="AR|Events")
    FZooARExhibitEventSignature OnExhibitHidden;

    /** Broadcast when the information panel becomes visible. WaitForAnimal waits until the hologram appearance is complete. */
    UPROPERTY(BlueprintAssignable, Category="AR|Events")
    FZooARExhibitEventSignature OnARPanelShow;

    /** Broadcast once to start the reserved animal's approach. A guided deck sequence defers this until the animal-facing turn finishes. */
    UPROPERTY(BlueprintAssignable, Category="AR|Events")
    FZooARExhibitEventSignature OnAnimalApproachRequested;

    /** Broadcast when the visitor leaves the viewing zone or the exhibit ends play. Looking away does not end the session. */
    UPROPERTY(BlueprintAssignable, Category="AR|Events")
    FZooARExhibitEventSignature OnARInteractionEnded;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AR|Components")
    TObjectPtr<USceneComponent> HologramRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AR|Components")
    TObjectPtr<USkeletalMeshComponent> AnimalMesh;

    /** The animal's hologram look (materialize, halo, rings): appears and disappears with the presentation. Its profile is set on the Blueprint. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AR|Components")
    TObjectPtr<UZooHologramComponent> Hologram;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AR|Components")
    TObjectPtr<UBoxComponent> AnimalClickBounds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AR|Components")
    TObjectPtr<UWidgetComponent> InformationPanel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AR|Components")
    TObjectPtr<UWidgetInteractionComponent> MouseInteraction;

    /** Phase timing and the walk/idle animations, used only in WaitForAnimal mode. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AR|Components")
    TObjectPtr<UZooARPresentationComponent> ReferencePresentation;

    UFUNCTION(BlueprintCallable, Category="AR|Information")
    void SetInformationOpen(bool bOpen);

    UFUNCTION(BlueprintPure, Category="AR|Information")
    bool IsInformationOpen() const { return bInformationOpen; }

    UFUNCTION(BlueprintPure, Category="AR|Activation")
    bool IsZoneActive() const { return bZoneActive; }

    UFUNCTION(BlueprintPure, Category="AR|Activation")
    bool IsLookingAtHabitat() const { return bLookingAtHabitat; }

    UFUNCTION(BlueprintPure, Category="AR|Activation")
    bool IsWaitingForAnimal() const { return bWaitingForAnimal; }

    UFUNCTION(BlueprintPure, Category="AR|Activation")
    bool IsWalkingToDeckCenter() const { return bWalkingToDeckCenter; }

    UFUNCTION(BlueprintPure, Category="AR|Activation")
    bool HasPresentationStarted() const { return bPresentationStarted; }

    UFUNCTION(BlueprintPure, Category="AR|Animation")
    float GetRevealProgress() const { return RevealProgress; }

    /** Stable world location of the animal's final entrance position, without floating motion. */
    UFUNCTION(BlueprintPure, Category="AR|Animals")
    FVector GetAnimalTargetLocation() const;

    UFUNCTION(BlueprintPure, Category="AR|Animals")
    AZooAnimal* GetSelectedAnimal() const;

    /** Called by the reserved animal on successful arrival. Late arrivals cannot restart the presentation. */
    void NotifyAnimalArrived(AZooAnimal* Animal);

    /** Internal callbacks from the local desktop sequence owner. */
    void NotifyDeckCenterReached();
    void NotifyAnimalFocusFinished();
    void CancelDeckSequence();

    UFUNCTION(BlueprintCallable, Category="AR|Interaction")
    void HandleMousePressed();

    UFUNCTION(BlueprintCallable, Category="AR|Interaction")
    void HandleMouseReleased();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    friend struct FZooAnimalTestAccess;
    friend class UZooARPresentationComponent;

    void ConfigureExhibit();
    void ConfigureInformationPanel();
    void CacheMeshBoundsAndPlacement();
    void InitializeInput();
    UZooARInfoWidget* GetInformationWidget() const;

    bool IsVisitorInZone() const;
    bool IsEntranceMovementComplete() const;
    bool CanInteract() const;
    bool CanInteractWithInformation() const;
    bool CanInteractWithAnimal() const;
    void UpdateActivation(float DeltaSeconds);
    void BeginAnimalArrivalWait();
    void UpdateAnimalArrivalWait(float DeltaSeconds);
    void StopArrivalCameraFocus();
    void StartAppearance();
    void UpdateInformationVisibility();
    void BeginARSession(bool bDeferApproach = false);
    void RequestAnimalApproach();
    void EndARSession();
    void AcquireCursor();
    void ReleaseCursor();

    void UpdateAnimation(float DeltaSeconds);
    void UpdateAppearanceEvents();
    void UpdateEntranceMovement(float DeltaSeconds);
    void UpdateInformationPanel(float BobOffset);
    void SetVisualsVisible(bool bVisible);

    bool GetMouseRay(FVector& Start, FVector& Direction) const;
    void UpdateMouseTrace();
    void HandleMouseScrolled(float Value);
    bool IsMouseOverAnimalBounds() const;

    TWeakObjectPtr<APlayerController> LocalController;
    TWeakObjectPtr<AZooAnimal> SelectedAnimal;
    TWeakObjectPtr<UZooARCameraFocusComponent> ArrivalCameraFocus;

    bool bZoneActive = false;
    bool bLookingAtHabitat = false;
    bool bInformationOpen = false;
    bool bPointerPressed = false;
    bool bSafePlacement = false;
    bool bOwnsMouseCursor = false;
    bool bAppearActive = false;
    bool bAppearCompleted = false;
    bool bARSessionStarted = false;
    bool bPanelWasVisible = false;
    bool bWaitingForAnimal = false;
    bool bWalkingToDeckCenter = false;
    bool bGuidedArrivalSession = false;
    bool bAnimalApproachRequested = false;
    bool bPresentationStarted = false;
    float AnimalWaitElapsed = 0.f;
    float RevealProgress = 0.f;
    float FloatTime = 0.f;
    float LookAwayTime = 0.f;

    FVector RestPanelLocation = FVector::ZeroVector;
    FVector RestHologramLocation = FVector::ZeroVector;
    FVector RestAnimalLocation = FVector::ZeroVector;
    FHitResult MouseHit;

    FBoxSphereBounds CachedMeshBounds = FBoxSphereBounds(ForceInit);
};
