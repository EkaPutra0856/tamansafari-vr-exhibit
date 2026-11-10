#include "AR/ZooARPresentationComponent.h"

#include "AR/ZooARExhibit.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

namespace
{
    float SafeDuration(float Value, float Fallback)
    {
        return FMath::IsFinite(Value) ? FMath::Max(Value, .01f) : Fallback;
    }
}

UZooARPresentationComponent::UZooARPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UZooARPresentationComponent::Initialize(AZooARExhibit* OwnerExhibit)
{
    if (bInitialized || !IsValid(OwnerExhibit)) return;
    Exhibit = OwnerExhibit;
    bInitialized = true;
    const USkeletalMesh* MeshAsset = OwnerExhibit->AnimalMesh->GetSkeletalMeshAsset();
    const FString MeshName = GetNameSafe(MeshAsset);
    const FString Species = MeshName.Contains(TEXT("Giraffe")) ? TEXT("Giraffe")
        : MeshName.Contains(TEXT("Wombat")) ? TEXT("Wombat") : FString();
    if (Species.IsEmpty()) return;

    const FString SpeciesRoot = AssetRoot / Species;
    WalkAnimation = LoadObject<UAnimSequence>(nullptr, *(SpeciesRoot / (TEXT("A_") + Species + TEXT("_ReferenceWalk"))));
    IdleAnimation = LoadObject<UAnimSequence>(nullptr, *(SpeciesRoot / (TEXT("A_") + Species + TEXT("_ReferenceIdle"))));
}

float UZooARPresentationComponent::GetInDuration() const { return SafeDuration(InDuration, 3.9f); }
float UZooARPresentationComponent::GetOutDuration() const { return SafeDuration(OutDuration, 100.f / 30.f); }

float UZooARPresentationComponent::GetProgress() const
{
    switch (Phase)
    {
    case EZooARVisualPhase::In: return FMath::Clamp(PhaseElapsed / GetInDuration(), 0.f, 1.f);
    case EZooARVisualPhase::Out: return FMath::Clamp(1.f - PhaseElapsed / GetOutDuration(), 0.f, 1.f);
    case EZooARVisualPhase::Moving:
    case EZooARVisualPhase::Idle: return 1.f;
    default: return 0.f;
    }
}

void UZooARPresentationComponent::ResetPresentation()
{
    Phase = EZooARVisualPhase::Hidden;
    PhaseElapsed = 0.f;
    if (AZooARExhibit* OwnerExhibit = Exhibit.Get())
    {
        OwnerExhibit->SetVisualsVisible(false);
        OwnerExhibit->AnimalMesh->bPauseAnims = true;
    }
}

void UZooARPresentationComponent::PlayBodyAnimation(UAnimSequence* Animation)
{
    AZooARExhibit* OwnerExhibit = Exhibit.Get();
    if (!OwnerExhibit || !Animation || !OwnerExhibit->AnimalMesh->GetSkeletalMeshAsset()) return;
    if (OwnerExhibit->AnimalMesh->GetSkeletalMeshAsset()->GetSkeleton() != Animation->GetSkeleton())
    {
        UE_LOG(LogTemp, Warning, TEXT("AR %s skipped incompatible presentation animation %s. Its skeleton must match the exhibit mesh."), *OwnerExhibit->GetName(), *Animation->GetName());
        return;
    }
    OwnerExhibit->AnimalMesh->PlayAnimation(Animation, true);
    OwnerExhibit->AnimalMesh->bPauseAnims = false;
}

void UZooARPresentationComponent::SetPhase(EZooARVisualPhase NewPhase)
{
    Phase = NewPhase;
    PhaseElapsed = 0.f;
    if (Phase == EZooARVisualPhase::Moving) PlayBodyAnimation(WalkAnimation);
    else if (Phase == EZooARVisualPhase::In || Phase == EZooARVisualPhase::Idle || Phase == EZooARVisualPhase::Out)
        PlayBodyAnimation(IdleAnimation);
}

void UZooARPresentationComponent::BeginIn()
{
    AZooARExhibit* OwnerExhibit = Cast<AZooARExhibit>(GetOwner());
    Initialize(OwnerExhibit);
    if (!OwnerExhibit) return;
    ResetPresentation();
    OwnerExhibit->HologramRoot->SetRelativeLocation(OwnerExhibit->RestHologramLocation);
    OwnerExhibit->AnimalMesh->SetRelativeLocation(OwnerExhibit->bAnimateEntranceMovement
        ? OwnerExhibit->EntranceStartLocation : OwnerExhibit->RestAnimalLocation);
    OwnerExhibit->SetVisualsVisible(true);
    SetPhase(EZooARVisualPhase::In);
}

void UZooARPresentationComponent::BeginOut()
{
    if (Phase == EZooARVisualPhase::Hidden || Phase == EZooARVisualPhase::Out) return;
    // Nothing showed yet: there is nothing to hide
    if (Phase == EZooARVisualPhase::In && PhaseElapsed <= 0.f)
    {
        ResetPresentation();
        return;
    }
    SetPhase(EZooARVisualPhase::Out);
}

void UZooARPresentationComponent::EnterAfterIn()
{
    const AZooARExhibit* OwnerExhibit = Exhibit.Get();
    SetPhase(OwnerExhibit && OwnerExhibit->bAnimateEntranceMovement && !OwnerExhibit->IsEntranceMovementComplete()
        ? EZooARVisualPhase::Moving : EZooARVisualPhase::Idle);
}

void UZooARPresentationComponent::Advance(float DeltaSeconds)
{
    AZooARExhibit* OwnerExhibit = Exhibit.Get();
    if (!OwnerExhibit) return;
    float Remaining = FMath::IsFinite(DeltaSeconds) ? FMath::Max(DeltaSeconds, 0.f) : 0.f;
    if (Phase == EZooARVisualPhase::In)
    {
        const float Step = FMath::Min(Remaining, FMath::Max(GetInDuration() - PhaseElapsed, 0.f));
        PhaseElapsed += Step;
        Remaining -= Step;
        if (PhaseElapsed >= GetInDuration()) EnterAfterIn();
    }
    if (Phase == EZooARVisualPhase::Moving)
    {
        OwnerExhibit->UpdateEntranceMovement(Remaining);
        if (OwnerExhibit->IsEntranceMovementComplete()) SetPhase(EZooARVisualPhase::Idle);
    }
    else if (Phase == EZooARVisualPhase::Out)
    {
        PhaseElapsed += Remaining;
        if (PhaseElapsed >= GetOutDuration()) ResetPresentation();
    }
    OwnerExhibit->HologramRoot->SetRelativeLocation(OwnerExhibit->RestHologramLocation);
}

void UZooARPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetPresentation();
    Super::EndPlay(EndPlayReason);
}
