#include "AR/ZooAnimal.h"

#include "AIController.h"
#include "AR/ZooARExhibit.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogZooAnimal, Log, All);

namespace
{
    constexpr float MinimumDelay = .1f;
    constexpr int32 WanderAttempts = 4;

    float NonNegativeOr(float Value, float Fallback)
    {
        return FMath::IsFinite(Value) ? FMath::Max(Value, 0.f) : Fallback;
    }

    FString DescribeActorTags(const AActor* Actor)
    {
        return FString::JoinBy(Actor->Tags, TEXT(", "), [](FName Tag) { return Tag.ToString(); });
    }
}

AZooAnimal::AZooAnimal()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    bUseControllerRotationYaw = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->MaxWalkSpeed = WalkSpeed;
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0., 180., 0.);

    // Physical animals must not intercept the exhibit's mouse traces.
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AZooAnimal::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConfigureAppearance();
}

void AZooAnimal::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (State == EZooAnimalState::WaitingAtExhibit)
    {
        MatchExhibitFacing();
    }
}

void AZooAnimal::ConfigureAppearance()
{
    const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    GetMesh()->SetRelativeLocation(FVector(0., 0., -HalfHeight) + MeshVisualOffset);
    GetCharacterMovement()->MaxWalkSpeed = FMath::Max(NonNegativeOr(WalkSpeed, 200.f), .1f);
}

void AZooAnimal::BeginPlay()
{
    Super::BeginPlay();
    ConfigureAppearance();
    HomeLocation = GetNavAgentLocation();
    bReady = true;
    BindController();
    UE_LOG(LogZooAnimal, Log, TEXT("Animal %s initialized: controller=%s, actor tags=[%s], MatchingTag=%s, feet=(%s)."),
        *GetName(), *GetNameSafe(AnimalController.Get()), *DescribeActorTags(this), *MatchingTag.ToString(), *HomeLocation.ToString());
    ScheduleWander();
}

void AZooAnimal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    GetWorldTimerManager().ClearTimer(WanderTimer);
    ReleaseExhibit();
    StopActiveMove();
    UnbindController();
    Super::EndPlay(EndPlayReason);
}

void AZooAnimal::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (bReady)
    {
        BindController();
        ScheduleWander();
    }
}

void AZooAnimal::UnPossessed()
{
    StopActiveMove();
    UnbindController();
    GetWorldTimerManager().ClearTimer(WanderTimer);
    ReleaseExhibit();
    Super::UnPossessed();
}

void AZooAnimal::BindController()
{
    AAIController* AnimalAI = Cast<AAIController>(GetController());
    if (AnimalController.Get() == AnimalAI && MoveCompletedHandle.IsValid())
    {
        return;
    }

    UnbindController();
    AnimalController = AnimalAI;
    if (AnimalAI && AnimalAI->GetPathFollowingComponent())
    {
        MoveCompletedHandle = AnimalAI->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &AZooAnimal::HandleMoveCompleted);
    }
    else if (HasAuthority())
    {
        UE_LOG(LogZooAnimal, Warning, TEXT("Animal %s needs an AIController before it can wander or approach an exhibit."), *GetName());
    }
}

void AZooAnimal::UnbindController()
{
    if (AAIController* AnimalAI = AnimalController.Get())
    {
        if (UPathFollowingComponent* Following = AnimalAI->GetPathFollowingComponent())
        {
            Following->OnRequestFinished.Remove(MoveCompletedHandle);
        }
    }

    MoveCompletedHandle.Reset();
    AnimalController.Reset();
}

void AZooAnimal::StopActiveMove()
{
    // Invalidate first: StopMovement can synchronously report the aborted request.
    ActiveMoveRequest = FAIRequestID::InvalidRequest;
    if (AAIController* AnimalAI = AnimalController.Get())
    {
        AnimalAI->StopMovement();
    }
}

void AZooAnimal::ScheduleWander()
{
    State = EZooAnimalState::Idle;
    SetActorTickEnabled(false);
    if (!bReady || bEndingPlay || !HasAuthority() || !AnimalController.IsValid()
        || AssignedExhibit.IsValid() || NonNegativeOr(WanderRadius, 0.f) <= 0.f)
    {
        return;
    }

    const float MinWait = FMath::Max(NonNegativeOr(WanderWaitMin, 2.f), MinimumDelay);
    const float MaxWait = FMath::Max(NonNegativeOr(WanderWaitMax, MinWait), MinWait);
    GetWorldTimerManager().SetTimer(WanderTimer, this, &AZooAnimal::ChooseWanderDestination, FMath::FRandRange(MinWait, MaxWait), false);
}

void AZooAnimal::ChooseWanderDestination()
{
    if (bEndingPlay || AssignedExhibit.IsValid() || !AnimalController.IsValid())
    {
        return;
    }

    const float Radius = NonNegativeOr(WanderRadius, 0.f);
    for (int32 Attempt = 0; Radius > 0.f && Attempt < WanderAttempts; ++Attempt)
    {
        const float Angle = FMath::FRand() * 2.f * PI;
        const float Distance = FMath::Sqrt(FMath::FRand()) * Radius;
        const FVector Destination = HomeLocation + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.);

        // The controller projects the point onto NavMesh and rejects incomplete paths.
        const EPathFollowingRequestResult::Type Result = RequestMove(Destination, EZooAnimalState::Wandering);
        if (Result == EPathFollowingRequestResult::RequestSuccessful)
        {
            return;
        }
        if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
        {
            break;
        }
    }

    ScheduleWander();
}

EPathFollowingRequestResult::Type AZooAnimal::RequestMove(const FVector& Destination, EZooAnimalState NewState)
{
    AAIController* AnimalAI = AnimalController.Get();
    if (!AnimalAI || Destination.ContainsNaN())
    {
        return EPathFollowingRequestResult::Failed;
    }

    StopActiveMove();
    State = NewState;
    GetCharacterMovement()->MaxWalkSpeed = FMath::Max(NonNegativeOr(WalkSpeed, 200.f), .1f);

    FAIMoveRequest Request(Destination);
    Request.SetUsePathfinding(true);
    Request.SetProjectGoalLocation(true);
    Request.SetAllowPartialPath(false);
    Request.SetAcceptanceRadius(NonNegativeOr(AcceptanceRadius, 30.f));
    Request.SetReachTestIncludesAgentRadius(false);

    const FPathFollowingRequestResult Result = AnimalAI->MoveTo(Request);
    if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
    {
        ActiveMoveRequest = Result.MoveId;
    }
    return Result.Code;
}

void AZooAnimal::HandleMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    if (!ActiveMoveRequest.IsValid() || RequestID.GetID() != ActiveMoveRequest.GetID())
    {
        return;
    }

    ActiveMoveRequest = FAIRequestID::InvalidRequest;
    if (State == EZooAnimalState::MovingToExhibit)
    {
        if (Result.IsSuccess() && AssignedExhibit.IsValid())
        {
            EnterWaitingAtExhibit();
            UE_LOG(LogZooAnimal, Log, TEXT("Animal %s reached exhibit %s and is waiting for the viewing-zone session to end."),
                *GetName(), *GetNameSafe(AssignedExhibit.Get()));
        }
        else
        {
            UE_LOG(LogZooAnimal, Warning, TEXT("Animal %s could not reach exhibit %s: %s; resuming wander."),
                *GetName(), *GetNameSafe(AssignedExhibit.Get()), *Result.ToString());
            ReleaseExhibit();
        }
    }
    else if (State == EZooAnimalState::Wandering)
    {
        ScheduleWander();
    }
}

bool AZooAnimal::MatchesExhibit(const AZooARExhibit* Exhibit) const
{
    if (!IsValid(Exhibit))
    {
        return false;
    }
    if (!MatchingTag.IsNone())
    {
        return Exhibit->ActorHasTag(MatchingTag);
    }

    for (const FName Tag : Tags)
    {
        if (Tag.ToString().StartsWith(TEXT("Animal."), ESearchCase::IgnoreCase) && Exhibit->ActorHasTag(Tag))
        {
            return true;
        }
    }
    return false;
}

bool AZooAnimal::IsAvailableForExhibit(const AZooARExhibit* Exhibit) const
{
    return bReady && !bEndingPlay && HasAuthority() && AnimalController.IsValid()
        && !AssignedExhibit.IsValid() && MatchesExhibit(Exhibit);
}

FVector AZooAnimal::GetExhibitDestination(const AZooARExhibit* Exhibit) const
{
    return Exhibit->GetAnimalTargetLocation() + Exhibit->GetActorQuat().RotateVector(ExhibitTargetOffset);
}

AZooAnimal* AZooAnimal::FindNearestForExhibit(AZooARExhibit* Exhibit, float MaxDistance)
{
    if (!IsValid(Exhibit) || !Exhibit->GetWorld() || !FMath::IsFinite(MaxDistance) || MaxDistance <= 0.f)
    {
        return nullptr;
    }

    AZooAnimal* Nearest = nullptr;
    double BestDistanceSquared = FMath::Square(static_cast<double>(MaxDistance));
    double ClosestAvailableDistance = -1.;
    int32 TotalAnimals = 0;
    int32 MatchingAnimals = 0;
    int32 AvailableAnimals = 0;
    int32 MissingControllers = 0;
    UE_LOG(LogZooAnimal, Log, TEXT("AR interaction session started for %s: actor tags=[%s], target=(%s), selection radius=%.1f cm."),
        *Exhibit->GetName(), *DescribeActorTags(Exhibit), *Exhibit->GetAnimalTargetLocation().ToString(), MaxDistance);
    for (TActorIterator<AZooAnimal> It(Exhibit->GetWorld()); It; ++It)
    {
        AZooAnimal* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        ++TotalAnimals;
        if (!Candidate->MatchesExhibit(Exhibit))
        {
            continue;
        }
        ++MatchingAnimals;
        if (!Candidate->AnimalController.IsValid())
        {
            ++MissingControllers;
        }
        if (!Candidate->IsAvailableForExhibit(Exhibit))
        {
            UE_LOG(LogZooAnimal, Verbose, TEXT("Animal %s matches %s but is unavailable: ready=%d, ending play=%d, authority=%d, controller=%s, assigned exhibit=%s."),
                *Candidate->GetName(), *Exhibit->GetName(), Candidate->bReady, Candidate->bEndingPlay, Candidate->HasAuthority(),
                *GetNameSafe(Candidate->AnimalController.Get()), *GetNameSafe(Candidate->AssignedExhibit.Get()));
            continue;
        }
        ++AvailableAnimals;

        const double DistanceSquared = FVector::DistSquared(Candidate->GetNavAgentLocation(), Candidate->GetExhibitDestination(Exhibit));
        const double Distance = FMath::Sqrt(DistanceSquared);
        if (ClosestAvailableDistance < 0. || Distance < ClosestAvailableDistance)
        {
            ClosestAvailableDistance = Distance;
        }
        if (DistanceSquared <= BestDistanceSquared && (!Nearest || DistanceSquared < BestDistanceSquared
            || Candidate->GetName().Compare(Nearest->GetName()) < 0))
        {
            Nearest = Candidate;
            BestDistanceSquared = DistanceSquared;
        }
    }
    if (Nearest)
    {
        UE_LOG(LogZooAnimal, Log, TEXT("Exhibit %s selected animal %s at %.1f cm from destination (%s)."),
            *Exhibit->GetName(), *Nearest->GetName(), FMath::Sqrt(BestDistanceSquared), *Nearest->GetExhibitDestination(Exhibit).ToString());
    }
    else
    {
        UE_LOG(LogZooAnimal, Log, TEXT("Exhibit %s found no eligible animal within %.1f cm: total animals=%d, matching tags=%d, available matches=%d, matches without AIController=%d, nearest available distance=%.1f cm (-1 means none). Set the animal's MatchingTag to an exhibit Actor Tag, or use shared Animal.* Actor Tags."),
            *Exhibit->GetName(), MaxDistance, TotalAnimals, MatchingAnimals, AvailableAnimals, MissingControllers, ClosestAvailableDistance);
    }
    return Nearest;
}

bool AZooAnimal::BindToExhibit(AZooARExhibit* Exhibit, bool bDeferApproach)
{
    if (!IsAvailableForExhibit(Exhibit))
    {
        return false;
    }

    AssignedExhibit = Exhibit;
    AddTickPrerequisiteActor(Exhibit);
    GetWorldTimerManager().ClearTimer(WanderTimer);
    StopActiveMove();
    State = bDeferApproach ? EZooAnimalState::ReservedForExhibit : EZooAnimalState::MovingToExhibit;
    Exhibit->OnAnimalApproachRequested.AddUniqueDynamic(this, &AZooAnimal::HandleAnimalApproachRequested);
    Exhibit->OnARInteractionEnded.AddUniqueDynamic(this, &AZooAnimal::HandleARInteractionEnded);
    return true;
}

void AZooAnimal::HandleAnimalApproachRequested(AZooARExhibit* Exhibit)
{
    if (AssignedExhibit.Get() != Exhibit || !IsValid(Exhibit)
        || State == EZooAnimalState::WaitingAtExhibit || ActiveMoveRequest.IsValid())
    {
        return;
    }

    const FVector Destination = GetExhibitDestination(Exhibit);
    const EPathFollowingRequestResult::Type Result = RequestMove(Destination, EZooAnimalState::MovingToExhibit);
    UE_LOG(LogZooAnimal, Log, TEXT("Animal %s received OnAnimalApproachRequested from %s: destination=(%s), move result=%s."),
        *GetName(), *Exhibit->GetName(), *Destination.ToString(),
        Result == EPathFollowingRequestResult::RequestSuccessful ? TEXT("RequestSuccessful")
            : Result == EPathFollowingRequestResult::AlreadyAtGoal ? TEXT("AlreadyAtGoal") : TEXT("Failed"));
    if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        EnterWaitingAtExhibit();
    }
    else if (Result == EPathFollowingRequestResult::Failed)
    {
        UE_LOG(LogZooAnimal, Warning, TEXT("Animal %s has no complete navigation path to exhibit %s; resuming wander."), *GetName(), *Exhibit->GetName());
        ReleaseExhibit();
    }
}

void AZooAnimal::HandleARInteractionEnded(AZooARExhibit* Exhibit)
{
    if (AssignedExhibit.Get() == Exhibit)
    {
        ReleaseExhibit();
    }
}

void AZooAnimal::EnterWaitingAtExhibit()
{
    State = EZooAnimalState::WaitingAtExhibit;
    GetCharacterMovement()->StopMovementImmediately();
    MatchExhibitFacing();
    SetActorTickEnabled(true);
    if (AZooARExhibit* Exhibit = AssignedExhibit.Get())
    {
        Exhibit->NotifyAnimalArrived(this);
    }
}

void AZooAnimal::MatchExhibitFacing()
{
    const AZooARExhibit* Exhibit = AssignedExhibit.Get();
    if (!IsValid(Exhibit))
    {
        return;
    }

    // Match the visible hologram even when the two meshes have different local rotations.
    const FQuat DesiredRotation = Exhibit->AnimalMesh->GetComponentQuat() * GetMesh()->GetRelativeRotation().Quaternion().Inverse();
    const FRotator Facing(0., DesiredRotation.Rotator().Yaw, 0.);
    SetActorRotation(Facing);
    if (AAIController* AnimalAI = AnimalController.Get())
    {
        AnimalAI->SetControlRotation(Facing);
    }
}

void AZooAnimal::ReleaseExhibit()
{
    if (AZooARExhibit* Exhibit = AssignedExhibit.Get())
    {
        RemoveTickPrerequisiteActor(Exhibit);
        Exhibit->OnAnimalApproachRequested.RemoveDynamic(this, &AZooAnimal::HandleAnimalApproachRequested);
        Exhibit->OnARInteractionEnded.RemoveDynamic(this, &AZooAnimal::HandleARInteractionEnded);
    }

    AssignedExhibit.Reset();
    StopActiveMove();
    ScheduleWander();
}
