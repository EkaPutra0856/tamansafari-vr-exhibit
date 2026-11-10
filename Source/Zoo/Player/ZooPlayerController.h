// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "PathGuide/PathGuideTypes.h"
#include "ZooPlayerController.generated.h"

class AZooARExhibit;
class AZooDumaController;
class UInputAction;
class UInputMappingContext;
class UWzPathGuideComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDestinationMenuChanged, bool /*bOpen*/);

/** Owns walking and Path Guide input (Enhanced Input, assets set on the Blueprint child), the Path Guide model, the destination menu state and the input mode. */
UCLASS(Blueprintable)
class AZooPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AZooPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

protected:
	// ====== Walking Input ====== //

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Input")
	TObjectPtr<UInputAction> SprintAction;

	/** Skips the dialogue line: the 1st press completes the text, the next goes to the next line. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Input")
	TObjectPtr<UInputAction> DialogueSkipAction;

protected:
	// ====== Path Guide ====== //

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PathGuide")
	TObjectPtr<UWzPathGuideComponent> PathGuide;

	/** Mapping context with the Toggle / Close / Show Cursor actions (N / Esc / Alt). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|PathGuideInput")
	TObjectPtr<UInputMappingContext> PathGuideMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|PathGuideInput")
	int32 PathGuideMappingPriority = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|PathGuideInput")
	TObjectPtr<UInputAction> ToggleMenuAction;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|PathGuideInput")
	TObjectPtr<UInputAction> CloseMenuAction;

	/** Held to reveal the mouse cursor. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|PathGuideInput")
	TObjectPtr<UInputAction> ShowCursorAction;

public:
	FORCEINLINE UWzPathGuideComponent* GetPathGuide() const { return PathGuide; }

public:
	// ====== Destination Menu ====== //

	/** The "Mau ke mana?" list opened or closed. */
	FOnDestinationMenuChanged OnDestinationMenuChanged;

private:
	bool bDestinationMenuOpen = false;

public:
	/** Opens or closes the list: applies the input mode, then tells the UI. */
	void SetDestinationMenuOpen(bool bOpen);

	FORCEINLINE bool IsDestinationMenuOpen() const { return bDestinationMenuOpen; }

private:
	// ====== Narrative ====== //

	/** Sends the narrative the places the visitor already stands in: its tasks only see places entered after they listen. */
	void HandleNarrativeListening();

	/** Tells the narrative the visitor chose a destination (the Path Guide started guiding). */
	void HandlePathGuideStateChanged(EPathGuideState NewState, const FGameplayTag& PlaceTag);

protected:
	// ====== AR ====== //

	/** Exhibit actor tag (Animal.Giraffe...) to the narrative event sent when the visitor leaves its AR view. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|AR")
	TMap<FName, FGameplayTag> ARVisitEvents;

private:
	/** Duma steps out while an AR animal shows. */
	UFUNCTION()
	void HandleARApproach(AZooARExhibit* Exhibit);

	UFUNCTION()
	void HandleARAppearStarted(AZooARExhibit* Exhibit);

	UFUNCTION()
	void HandleARAppearCompleted(AZooARExhibit* Exhibit);

	/** Left before the animal began showing: no Hidden follows, so Duma returns here. */
	UFUNCTION()
	void HandleARInteractionEnded(AZooARExhibit* Exhibit);

	/** The animal is gone: Duma returns, then the visit (if it completed) reaches the narrative. */
	UFUNCTION()
	void HandleARHidden(AZooARExhibit* Exhibit);

	/** Brings Duma back, only if the AR (not the narrative) sent him away. */
	void RestoreDuma();

	AZooDumaController* FindDumaBrain() const;

	/** An AR animal is showing: the cursor works its panels, and look follows the mouse only while the right button is held. */
	bool bARViewActive = false;

	/** Duma was despawned by an AR approach. */
	bool bDumaDismissedByAR = false;

	/** Exhibits whose animal began showing / finished showing in the current visit. */
	TSet<TWeakObjectPtr<AZooARExhibit>> StartedExhibits;
	TSet<TWeakObjectPtr<AZooARExhibit>> AppearedExhibits;

private:
	// ====== Input Mode ====== //

	/** Alt held: the cursor is revealed without the list. */
	bool bCursorRequested = false;

	/** Mirror the ignore states so the reference-counted SetIgnore*Input only runs on a change. */
	bool bLookIgnored = false;
	bool bMoveIgnored = false;

	/** Applies the cursor, look / move ignore and input mode that match the menu and Alt state. */
	void ApplyInputMode();

	void SetCursorRequested(bool bRequested);
	void SetCursorVisible(bool bVisible);
	void SetLookIgnored(bool bIgnored);
	void SetMoveIgnored(bool bIgnored);
};