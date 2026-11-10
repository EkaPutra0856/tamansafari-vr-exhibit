#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
#include "ZooAnimal.generated.h"

class AAIController;
class AZooARExhibit;
struct FPathFollowingResult;

UENUM(BlueprintType)
enum class EZooAnimalState : uint8
{
    Idle,
    Wandering,
    MovingToExhibit,
    WaitingAtExhibit,
    ReservedForExhibit
};

/** A ground animal that wanders near its home and approaches a matching AR exhibit when selected. */
UCLASS(Blueprintable)
class ZOO_API AZooAnimal : public ACharacter
{
    GENERATED_BODY()

public:
    AZooAnimal();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;

    /** Movement speed for wandering and exhibit approaches, in centimeters per second. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animal|Movement", meta=(ClampMin="0.1", Units="cm/s"))
    float WalkSpeed = 200.f;

    /** Radius around the initial feet location used for wander destinations. Zero disables wandering. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animal|Wander", meta=(ClampMin="0", Units="cm"))
    float WanderRadius = 500.f;

    /** Minimum idle time between wander moves, in seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animal|Wander", meta=(ClampMin="0.1", Units="s"))
    float WanderWaitMin = 2.f;

    /** Maximum idle time between wander moves, in seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animal|Wander", meta=(ClampMin="0.1", Units="s"))
    float WanderWaitMax = 5.f;

    /** Distance from the destination at which a move is considered complete. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animal|Movement", meta=(ClampMin="0", Units="cm"))
    float AcceptanceRadius = 30.f;

    /** Optional animal identity matched against an exhibit Actor Tag. When empty, use shared Animal.* Actor Tags on both actors. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animal|AR")
    FName MatchingTag;

    /** Destination offset in exhibit axes: X forward, Y right, Z up, in centimeters. Navigation projects it onto walkable ground. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animal|AR", meta=(Units="cm"))
    FVector ExhibitTargetOffset = FVector::ZeroVector;

    /** Additional mesh offset from the capsule feet. Use this for visual height adjustments without changing navigation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animal|Appearance", meta=(Units="cm"))
    FVector MeshVisualOffset = FVector::ZeroVector;

    UFUNCTION(BlueprintPure, Category="Animal|AI")
    EZooAnimalState GetAnimalState() const { return State; }

    UFUNCTION(BlueprintPure, Category="Animal|AR")
    AZooARExhibit* GetAssignedExhibit() const { return AssignedExhibit.Get(); }

    /** Select once per exhibit session. Occupied animals and animals without an AI controller are excluded. */
    static AZooAnimal* FindNearestForExhibit(AZooARExhibit* Exhibit, float MaxDistance);
    bool BindToExhibit(AZooARExhibit* Exhibit, bool bDeferApproach = false);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    friend struct FZooAnimalTestAccess;

    void ConfigureAppearance();
    void BindController();
    void UnbindController();
    void StopActiveMove();
    void ScheduleWander();
    void ChooseWanderDestination();
    EPathFollowingRequestResult::Type RequestMove(const FVector& Destination, EZooAnimalState NewState);
    void HandleMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);
    bool MatchesExhibit(const AZooARExhibit* Exhibit) const;
    bool IsAvailableForExhibit(const AZooARExhibit* Exhibit) const;
    FVector GetExhibitDestination(const AZooARExhibit* Exhibit) const;
    void EnterWaitingAtExhibit();
    void MatchExhibitFacing();
    void ReleaseExhibit();

    UFUNCTION()
    void HandleAnimalApproachRequested(AZooARExhibit* Exhibit);

    UFUNCTION()
    void HandleARInteractionEnded(AZooARExhibit* Exhibit);

    UPROPERTY(Transient)
    EZooAnimalState State = EZooAnimalState::Idle;

    TWeakObjectPtr<AAIController> AnimalController;
    TWeakObjectPtr<AZooARExhibit> AssignedExhibit;
    FDelegateHandle MoveCompletedHandle;
    FTimerHandle WanderTimer;
    FAIRequestID ActiveMoveRequest;
    FVector HomeLocation = FVector::ZeroVector;
    bool bReady = false;
    bool bEndingPlay = false;
};
