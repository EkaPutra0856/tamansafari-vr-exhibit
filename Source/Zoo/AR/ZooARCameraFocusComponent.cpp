#include "AR/ZooARCameraFocusComponent.h"

#include "AR/ZooAnimal.h"
#include "AR/ZooARExhibit.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"

UZooARCameraFocusComponent::UZooARCameraFocusComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

UZooARCameraFocusComponent* UZooARCameraFocusComponent::GetOrCreate(APlayerController* Player)
{
    if (!IsValid(Player))
    {
        return nullptr;
    }
    if (UZooARCameraFocusComponent* Existing = Player->FindComponentByClass<UZooARCameraFocusComponent>())
    {
        return Existing;
    }

    UZooARCameraFocusComponent* Component = NewObject<UZooARCameraFocusComponent>(Player, NAME_None, RF_Transient);
    Player->AddInstanceComponent(Component);
    Component->RegisterComponent();
    return Component;
}

bool UZooARCameraFocusComponent::StartFocus(AZooARExhibit* Exhibit, float MaxDuration)
{
    if (!FMath::IsFinite(MaxDuration) || MaxDuration <= 0.f)
    {
        return false;
    }
    if (FocusExhibit.IsValid())
    {
        return FocusExhibit.Get() == Exhibit;
    }
    if (!AcquireFocus(Exhibit))
    {
        return false;
    }
    Phase = EFocusPhase::AnimalFocus;
    TimeLimit = FMath::Min(MaxDuration, 2.f);
    return true;
}

bool UZooARCameraFocusComponent::AcquireFocus(AZooARExhibit* Exhibit)
{
    APlayerController* Player = Cast<APlayerController>(GetOwner());
    if (!IsValid(Exhibit) || !Exhibit->IsWaitingForAnimal() || !Player || !Player->IsLocalController()
        || !Player->GetPawn() || bOwnsInputLocks)
    {
        return false;
    }
    // A menu, dialogue, or another camera owner may already control the player's input.
    if (Player->IsMoveInputIgnored() || Player->IsLookInputIgnored())
    {
        return false;
    }

    FocusExhibit = Exhibit;
    FocusPawn = Player->GetPawn();
    InitialRotation = Player->GetControlRotation();
    Elapsed = 0.f;
    Player->SetIgnoreMoveInput(true);
    Player->SetIgnoreLookInput(true);
    bOwnsInputLocks = true;
    StopVisitorMovement();
    AddTickPrerequisiteActor(Exhibit);
    SetComponentTickEnabled(true);
    return true;
}

bool UZooARCameraFocusComponent::ResolveDeckDestination(const AZooARExhibit* Exhibit, const APlayerController* Player, FVector& Destination) const
{
    const ACharacter* Visitor = Player ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
    const UBoxComponent* Box = Exhibit && Exhibit->ViewingZone ? Exhibit->ViewingZone->FindComponentByClass<UBoxComponent>() : nullptr;
    if (!Visitor || !Box || Exhibit->DeckCenterOffset.ContainsNaN()
        || !Visitor->GetCharacterMovement()->IsMovingOnGround() || Visitor->GetCharacterMovement()->MaxWalkSpeed <= 0.f)
    {
        return false;
    }

    // Use box axes without multiplying centimeter offsets by its scale. Find floor for the capsule, not the box's elevated center.
    const FVector Point = Box->GetComponentLocation() + Box->GetComponentQuat().RotateVector(Exhibit->DeckCenterOffset);
    const UCapsuleComponent* Capsule = Visitor->GetCapsuleComponent();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const FBox Bounds = Box->Bounds.GetBox();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZooDeckDestination), false, Visitor);
    Params.AddIgnoredActor(Exhibit->ViewingZone);
    FHitResult Hit;
    const FVector Start(Point.X, Point.Y, Bounds.Max.Z + HalfHeight + 1.);
    const FVector End(Point.X, Point.Y, Bounds.Min.Z - HalfHeight);
    if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Capsule->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(Radius, HalfHeight), Params, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))
        || Hit.bStartPenetrating || !Visitor->GetCharacterMovement()->IsWalkable(Hit))
    {
        return false;
    }

    const FVector StandingCenter = Hit.Location + FVector(0., 0., 2.);
    const FVector LocalCenter = Box->GetComponentTransform().InverseTransformPosition(StandingCenter);
    const FVector Extent = Box->GetUnscaledBoxExtent();
    if (FMath::Abs(LocalCenter.X) > Extent.X || FMath::Abs(LocalCenter.Y) > Extent.Y || FMath::Abs(LocalCenter.Z) > Extent.Z)
    {
        return false;
    }
    Destination = StandingCenter - FVector(0., 0., HalfHeight);
    return !Destination.ContainsNaN();
}

bool UZooARCameraFocusComponent::StartDeckWalk(AZooARExhibit* Exhibit)
{
    FVector Destination;
    APlayerController* Player = Cast<APlayerController>(GetOwner());
    if (!IsValid(Exhibit) || !Exhibit->IsWalkingToDeckCenter() || FocusExhibit.IsValid()
        || !ResolveDeckDestination(Exhibit, Player, Destination) || !AcquireFocus(Exhibit))
    {
        return false;
    }
    DeckBox = Exhibit->ViewingZone->FindComponentByClass<UBoxComponent>();
    DeckDestination = Destination;
    bGuidedSequence = true;
    bAnimalTurnFinished = false;
    Phase = EFocusPhase::DeckWalk;
    return true;
}

void UZooARCameraFocusComponent::StopFocus(const AZooARExhibit* Exhibit)
{
    if (FocusExhibit.Get() == Exhibit)
    {
        ReleaseFocus();
    }
}

bool UZooARCameraFocusComponent::IsFocusingFor(const AZooARExhibit* Exhibit) const
{
    return bOwnsInputLocks && FocusExhibit.Get() == Exhibit;
}

FVector UZooARCameraFocusComponent::GetFocusPoint(const AZooARExhibit* Exhibit) const
{
    // Until the animal stands at the exhibit, look at where it will be (the hologram), not at the animal still walking over
    const AZooAnimal* Animal = Exhibit->GetSelectedAnimal();
    if (Animal && Animal->GetAnimalState() == EZooAnimalState::WaitingAtExhibit)
    {
        const USkeletalMeshComponent* Mesh = Animal->GetMesh();
        FVector Point = Animal->GetActorLocation();
        if (!Exhibit->CameraFocusSocket.IsNone() && Mesh->DoesSocketExist(Exhibit->CameraFocusSocket))
        {
            Point = Mesh->GetSocketLocation(Exhibit->CameraFocusSocket);
        }
        else if (Mesh->GetSkeletalMeshAsset())
        {
            Point = Mesh->Bounds.Origin;
        }
        return Point + Animal->GetActorQuat().RotateVector(Exhibit->CameraFocusOffset);
    }
    return Exhibit->GetAnimalTargetLocation() + Exhibit->GetActorQuat().RotateVector(Exhibit->CameraFocusOffset);
}

void UZooARCameraFocusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    APlayerController* Player = Cast<APlayerController>(GetOwner());
    const AZooARExhibit* Exhibit = FocusExhibit.Get();
    if (!bOwnsInputLocks || !IsValid(Exhibit) || !Exhibit->IsZoneActive() || !Exhibit->IsWaitingForAnimal()
        || !Player || !FocusPawn.IsValid() || Player->GetPawn() != FocusPawn.Get())
    {
        AbortDeckSequence();
        return;
    }

    const float Step = FMath::IsFinite(DeltaTime) ? FMath::Max(DeltaTime, 0.f) : 0.f;
    if (Phase == EFocusPhase::DeckWalk)
    {
        UpdateDeckWalk(Step);
    }
    else
    {
        UpdateAnimalFocus(Step);
    }
}

void UZooARCameraFocusComponent::UpdateDeckWalk(float DeltaTime)
{
    AZooARExhibit* Exhibit = FocusExhibit.Get();
    APlayerController* Player = Cast<APlayerController>(GetOwner());
    ACharacter* Visitor = Cast<ACharacter>(FocusPawn.Get());
    if (!DeckBox.IsValid() || !Visitor || !Visitor->GetCharacterMovement()->IsMovingOnGround()
        || Visitor->GetCharacterMovement()->MaxWalkSpeed <= 0.f)
    {
        AbortDeckSequence();
        return;
    }

    Elapsed += DeltaTime;
    const FVector Difference = DeckDestination - Visitor->GetNavAgentLocation();
    const float Acceptance = FMath::IsFinite(Exhibit->DeckArrivalAcceptanceRadius) ? FMath::Max(Exhibit->DeckArrivalAcceptanceRadius, 1.f) : 20.f;
    if (Difference.SizeSquared2D() <= FMath::Square(Acceptance)
        && FMath::Abs(Difference.Z) <= Visitor->GetCharacterMovement()->MaxStepHeight)
    {
        StopVisitorMovement();
        Phase = EFocusPhase::AnimalFocus;
        InitialRotation = Player->GetControlRotation();
        Elapsed = 0.f;
        const float Timeout = FMath::IsFinite(Exhibit->AnimalArrivalTimeout) ? FMath::Max(Exhibit->AnimalArrivalTimeout, 0.f) : 2.f;
        TimeLimit = FMath::Min(Timeout, 2.f);
        Exhibit->NotifyDeckCenterReached();
        return;
    }

    const float WalkTimeout = FMath::IsFinite(Exhibit->DeckWalkTimeout) ? FMath::Max(Exhibit->DeckWalkTimeout, .1f) : 8.f;
    if (Elapsed >= WalkTimeout)
    {
        AbortDeckSequence();
        return;
    }

    // Face the same horizontal direction used for movement, including corrections after collision.
    // Check arrival first so a zero-length direction cannot reset yaw at the stand point.
    const FVector WalkDirection = Difference.GetSafeNormal2D();
    if (!WalkDirection.IsNearlyZero())
    {
        const FRotator TargetFacing(0., WalkDirection.Rotation().Yaw, 0.);
        const float BlendTime = FMath::IsFinite(Exhibit->CameraFocusBlendTime) ? FMath::Max(Exhibit->CameraFocusBlendTime, .01f) : .8f;
        const float Alpha = FMath::Clamp(Elapsed / BlendTime, 0.f, 1.f);
        const float SmoothAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
        FRotator Rotation = FQuat::Slerp(InitialRotation.Quaternion(), TargetFacing.Quaternion(), SmoothAlpha).Rotator();
        Rotation.Roll = 0.;
        Player->SetControlRotation(Rotation);
    }
    const float InputScale = FMath::Clamp(Difference.Size2D() / FMath::Max(Visitor->GetCharacterMovement()->MaxWalkSpeed * .25f, 1.f), 0., 1.);
    // Only this automatic direction bypasses the manual input lock. CharacterMovement still handles collision, acceleration, and floor movement.
    Visitor->AddMovementInput(WalkDirection, InputScale, true);
}

void UZooARCameraFocusComponent::UpdateAnimalFocus(float DeltaTime)
{
    Elapsed += DeltaTime;
    if (Elapsed >= TimeLimit)
    {
        if (bGuidedSequence)
        {
            FinishAnimalTurn(FocusExhibit.Get());
        }
        ReleaseFocus();
        return;
    }

    const AZooARExhibit* Exhibit = FocusExhibit.Get();
    if (bGuidedSequence && !Exhibit->bEnableArrivalCameraFocus)
    {
        return;
    }
    const float ConfiguredBlend = FMath::IsFinite(Exhibit->CameraFocusBlendTime) ? FMath::Max(Exhibit->CameraFocusBlendTime, .01f) : .8f;
    const float BlendTime = bGuidedSequence ? FMath::Min(ConfiguredBlend, TimeLimit) : ConfiguredBlend;
    ApplyAnimalFocus(FMath::Clamp(Elapsed / BlendTime, 0.f, 1.f));
    if (bGuidedSequence && !bAnimalTurnFinished && Elapsed >= BlendTime)
    {
        FinishAnimalTurn(Exhibit);
    }
}

void UZooARCameraFocusComponent::ApplyAnimalFocus(float Alpha)
{
    const AZooARExhibit* Exhibit = FocusExhibit.Get();
    APlayerController* Player = Cast<APlayerController>(GetOwner());
    FVector CameraLocation;
    FRotator ViewRotation;
    Player->GetPlayerViewPoint(CameraLocation, ViewRotation);
    const FVector Direction = GetFocusPoint(Exhibit) - CameraLocation;
    if (Direction.ContainsNaN() || Direction.IsNearlyZero())
    {
        return;
    }

    FRotator TargetRotation = Direction.Rotation();
    const float MaxPitch = FMath::IsFinite(Exhibit->CameraFocusMaxPitch) ? FMath::Clamp(Exhibit->CameraFocusMaxPitch, 0.f, 89.f) : 60.f;
    TargetRotation.Pitch = FMath::ClampAngle(TargetRotation.Pitch, -MaxPitch, MaxPitch);
    TargetRotation.Roll = 0.;
    const float SmoothAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
    FRotator Rotation = FQuat::Slerp(InitialRotation.Quaternion(), TargetRotation.Quaternion(), SmoothAlpha).Rotator();
    Rotation.Roll = 0.;
    Player->SetControlRotation(Rotation);
}

void UZooARCameraFocusComponent::FinishAnimalTurn(const AZooARExhibit* Exhibit)
{
    if (!bGuidedSequence || bAnimalTurnFinished || Phase != EFocusPhase::AnimalFocus || FocusExhibit.Get() != Exhibit)
    {
        return;
    }
    bAnimalTurnFinished = true;
    if (Exhibit->bEnableArrivalCameraFocus)
    {
        ApplyAnimalFocus(1.f);
    }
    if (AZooARExhibit* OwnerExhibit = FocusExhibit.Get())
    {
        OwnerExhibit->NotifyAnimalFocusFinished();
    }
}

void UZooARCameraFocusComponent::StopVisitorMovement()
{
    if (APawn* Pawn = FocusPawn.Get())
    {
        Pawn->ConsumeMovementInputVector();
        if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
        {
            Movement->StopMovementImmediately();
        }
        if (ACharacter* Visitor = Cast<ACharacter>(Pawn))
        {
            Visitor->StopJumping();
        }
    }
}

void UZooARCameraFocusComponent::AbortDeckSequence()
{
    AZooARExhibit* Exhibit = FocusExhibit.Get();
    const bool bCancel = bGuidedSequence;
    ReleaseFocus();
    if (bCancel && IsValid(Exhibit))
    {
        Exhibit->CancelDeckSequence();
    }
}

void UZooARCameraFocusComponent::ReleaseFocus()
{
    if (bGuidedSequence && bOwnsInputLocks)
    {
        StopVisitorMovement();
    }
    if (bOwnsInputLocks)
    {
        if (APlayerController* Player = Cast<APlayerController>(GetOwner()))
        {
            Player->SetIgnoreMoveInput(false);
            Player->SetIgnoreLookInput(false);
        }
        bOwnsInputLocks = false;
    }
    if (AZooARExhibit* Exhibit = FocusExhibit.Get())
    {
        RemoveTickPrerequisiteActor(Exhibit);
    }
    FocusExhibit.Reset();
    FocusPawn.Reset();
    DeckBox.Reset();
    Phase = EFocusPhase::None;
    bGuidedSequence = false;
    bAnimalTurnFinished = false;
    SetComponentTickEnabled(false);
}

void UZooARCameraFocusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    AbortDeckSequence();
    Super::EndPlay(EndPlayReason);
}
