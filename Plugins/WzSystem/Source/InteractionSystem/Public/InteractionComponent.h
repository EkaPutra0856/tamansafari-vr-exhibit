#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Data.h"
#include "InteractionInterface.h"
#include "InteractionComponent.generated.h"

class AController;
class UPrimitiveComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInteractOverlap, bool /*bIsOnOverlapping*/);

/** Traces from the owner and forwards overlap/interact calls to the IWzInteractionInterface actor in sight. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class INTERACTIONSYSTEM_API UWzInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UWzInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// ====== Interact With ====== //

	/** Starts interacting with CurrentInteract; a continuous target hides its highlight meanwhile. */
	UFUNCTION(BlueprintCallable, Category = "Interact With", meta = (AutoCreateRefTerm = "Data"))
	void StartInteract(const FInteractionData& Data);

	/** Ends interacting with CurrentInteract and restores its highlight. */
	UFUNCTION(BlueprintCallable, Category = "Interact With")
	void EndInteract();

protected:
	UPROPERTY()
	TScriptInterface<IWzInteractionInterface> CurrentInteract;

	/** The target StartInteract began on, so EndInteract ends it even after the view moved away. */
	UPROPERTY()
	TScriptInterface<IWzInteractionInterface> Interacting;

private:
	float CurrentHitDistance = 0.f;

public:
	/** Distance from the trace start to the target actor, 0 when nothing is in sight. */
	UFUNCTION(BlueprintPure, Category = "Interact With")
	FORCEINLINE float GetCurrentHitDistance() const { return CurrentHitDistance; }

protected:
	// ====== Detection ====== //

	/** Trace length in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	float DetectionLineLength = 80.f;

	/** Trace start, relative to the owner's location and rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	FVector Offset = FVector::ZeroVector;

	/** Trace along the controller's facing instead of the owner's forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	bool bFollowControllerFacing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

public:
	/** Fires once per target change: true when an interactable is in sight, false when none. */
	FOnInteractOverlap OnInteractOverlap;

private:
	/** Traces, resolves the interactable in sight and fires Begin/EndOverlap on change. */
	void Detect();

private:
	// ====== Helpers ====== //

	/** The owner pawn's controller, looked up live (possession can follow BeginPlay). */
	AController* GetController() const;

public:
	// ====== Highlight ====== //

	/** Toggles the custom-depth stencil highlight on a mesh. */
	UFUNCTION(BlueprintCallable, Category = "Highlight")
	static void Highlight(UPrimitiveComponent* Mesh, bool bHighlight);

private:
	static constexpr int32 HighlightStencil = 1;
};
