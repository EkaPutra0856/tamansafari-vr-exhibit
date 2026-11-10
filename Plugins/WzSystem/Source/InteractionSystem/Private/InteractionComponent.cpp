#include "InteractionComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

// ====== Lifecycle ====== //

UWzInteractionComponent::UWzInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UWzInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Detect();
}

// ====== Interact With ====== //

void UWzInteractionComponent::StartInteract(const FInteractionData& Data)
{
	UObject* Target = CurrentInteract.GetObject();
	Interacting = CurrentInteract;
	if (!IsValid(Target)) return;

	// Forward to the target
	IWzInteractionInterface::Execute_StartInteract(Target, GetController(), Data);

	// Continuous targets hide their highlight while held
	if (IWzInteractionInterface::Execute_IsContinuous(Target)) IWzInteractionInterface::Execute_OnEndOverlap(Target, GetOwner());
}

void UWzInteractionComponent::EndInteract()
{
	UObject* Target = Interacting.GetObject();
	Interacting = nullptr;
	if (!IsValid(Target)) return;

	// Forward to the target (the one StartInteract began on, even if the view moved off it)
	IWzInteractionInterface::Execute_EndInteract(Target, GetController());

	// Restore the highlight while it is still in sight
	if (Target == CurrentInteract.GetObject()) IWzInteractionInterface::Execute_OnBeginOverlap(Target, GetOwner());
}

// ====== Detection ====== //

void UWzInteractionComponent::Detect()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Trace start and end
	const FVector Start = Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(Offset);
	const AController* Controller = GetController();
	const FVector Direction = (bFollowControllerFacing && Controller) ? Controller->GetControlRotation().Vector() : Owner->GetActorForwardVector();
	const FVector End = Start + Direction * DetectionLineLength;

	// Line trace
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);

	// Resolve target (kept fresh even when the target is unchanged)
	AActor* HitActor = Hit.GetActor();
	CurrentHitDistance = HitActor ? FVector::Dist(Start, HitActor->GetActorLocation()) : 0.f;
	UObject* NewTarget = (HitActor && HitActor->Implements<UWzInteractionInterface>()) ? HitActor : nullptr;
	if (NewTarget == CurrentInteract.GetObject()) return;

	// Swap targets
	UObject* Last = CurrentInteract.GetObject();
	CurrentInteract = TScriptInterface<IWzInteractionInterface>(NewTarget);

	// Notify old (it may be destroyed by now) and new
	if (IsValid(Last)) IWzInteractionInterface::Execute_OnEndOverlap(Last, Owner);
	if (UObject* Current = CurrentInteract.GetObject()) IWzInteractionInterface::Execute_OnBeginOverlap(Current, Owner);
	OnInteractOverlap.Broadcast(NewTarget != nullptr);
}

// ====== Helpers ====== //

AController* UWzInteractionComponent::GetController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());

	return Pawn ? Pawn->GetController() : nullptr;
}

// ====== Highlight ====== //

void UWzInteractionComponent::Highlight(UPrimitiveComponent* Mesh, bool bHighlight)
{
	if (!Mesh) return;

	Mesh->SetCustomDepthStencilValue(HighlightStencil);
	Mesh->SetRenderCustomDepth(bHighlight);
}
