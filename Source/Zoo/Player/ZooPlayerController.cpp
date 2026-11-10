// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/ZooPlayerController.h"

#include "AR/ZooARExhibit.h"
#include "Duma/ZooDuma.h"
#include "Duma/ZooDumaController.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "PathGuide/Components/PathGuideComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/ZooCharacter.h"
#include "Dialogue/WzDialogueSubsystem.h"
#include "Director/WzNarrativeSubsystem.h"

// ====== Lifecycle ====== //

AZooPlayerController::AZooPlayerController()
{
	PathGuide = CreateDefaultSubobject<UWzPathGuideComponent>(TEXT("PathGuide"));
}

void AZooPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Places the visitor walks into (guided or not) reach the narrative as events tagged with the place.
	PathGuide->OnPlaceReached.AddWeakLambda(this, [this](const FGameplayTag& Place)
	{
		GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>()->SendEvent(Place);
	});

	// A task that starts listening while the visitor already stands in its place still counts it.
	GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>()->OnListening.AddUObject(this, &AZooPlayerController::HandleNarrativeListening);

	// Choosing a destination in the list starts the guidance.
	PathGuide->OnStateChanged.AddUObject(this, &AZooPlayerController::HandlePathGuideStateChanged);

	// Arriving at whichever destination the visitor chose in the list.
	PathGuide->OnArrived.AddWeakLambda(this, [this](const FGameplayTag&)
	{
		GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>()->SendEvent(FGameplayTag::RequestGameplayTag(TEXT("Narrative.Event.Zoo.NavigationArrived")));
	});

	// Every placed AR exhibit: Duma steps out while its animal shows, and the visit reaches the narrative.
	for (TActorIterator<AZooARExhibit> It(GetWorld()); It; ++It)
	{
		It->OnAnimalApproachRequested.AddDynamic(this, &AZooPlayerController::HandleARApproach);
		It->OnExhibitAppearStarted.AddDynamic(this, &AZooPlayerController::HandleARAppearStarted);
		It->OnExhibitAppearCompleted.AddDynamic(this, &AZooPlayerController::HandleARAppearCompleted);
		It->OnARInteractionEnded.AddDynamic(this, &AZooPlayerController::HandleARInteractionEnded);
		It->OnExhibitHidden.AddDynamic(this, &AZooPlayerController::HandleARHidden);
	}

	// Mapping contexts: walking, then Path Guide.
	checkf(DefaultMappingContext && PathGuideMappingContext, TEXT("The mapping contexts are not set on the player controller"));
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	Subsystem->AddMappingContext(DefaultMappingContext, MappingPriority);
	Subsystem->AddMappingContext(PathGuideMappingContext, PathGuideMappingPriority);

	// Start in the game input mode.
	ApplyInputMode();
}

void AZooPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Leave the input as the game expects it: list closed, cursor hidden, look free.
	SetCursorRequested(false);
	SetDestinationMenuOpen(false);

	Super::EndPlay(EndPlayReason);
}

void AZooPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	checkf(MoveAction && LookAction && JumpAction && SprintAction && DialogueSkipAction && ToggleMenuAction && CloseMenuAction && ShowCursorAction, TEXT("The input actions are not set on the player controller"));
	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(InputComponent);

	// Move and look.
	Input->BindActionValueLambda(MoveAction, ETriggerEvent::Triggered, [this](const FInputActionValue& Value)
	{
		if (AZooCharacter* Visitor = GetPawn<AZooCharacter>()) Visitor->Move(Value.Get<FVector2D>());
	});
	Input->BindActionValueLambda(LookAction, ETriggerEvent::Triggered, [this](const FInputActionValue& Value)
	{
		if (AZooCharacter* Visitor = GetPawn<AZooCharacter>()) Visitor->Look(Value.Get<FVector2D>());
	});

	// Jump: a locked player (dialogue) does not jump; Space is also the dialogue Skip key.
	Input->BindActionInstanceLambda(JumpAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		if (AZooCharacter* Visitor = GetPawn<AZooCharacter>(); Visitor && !IsMoveInputIgnored()) Visitor->Jump();
	});
	Input->BindActionInstanceLambda(JumpAction, ETriggerEvent::Completed, [this](const FInputActionInstance&)
	{
		if (AZooCharacter* Visitor = GetPawn<AZooCharacter>()) Visitor->StopJumping();
	});

	// Sprint.
	Input->BindActionInstanceLambda(SprintAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		if (AZooCharacter* Visitor = GetPawn<AZooCharacter>()) Visitor->SetSprinting(true);
	});
	Input->BindActionInstanceLambda(SprintAction, ETriggerEvent::Completed, [this](const FInputActionInstance&)
	{
		if (AZooCharacter* Visitor = GetPawn<AZooCharacter>()) Visitor->SetSprinting(false);
	});

	// Dialogue skip.
	Input->BindActionInstanceLambda(DialogueSkipAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		GetLocalPlayer()->GetSubsystem<UWzDialogueSubsystem>()->Skip();
	});

	// Path Guide menu.
	Input->BindActionInstanceLambda(ToggleMenuAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		SetDestinationMenuOpen(!bDestinationMenuOpen);
	});
	Input->BindActionInstanceLambda(CloseMenuAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		SetDestinationMenuOpen(false);
	});

	// Path Guide cursor: held to reveal it.
	Input->BindActionInstanceLambda(ShowCursorAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		SetCursorRequested(true);
	});
	Input->BindActionInstanceLambda(ShowCursorAction, ETriggerEvent::Completed, [this](const FInputActionInstance&)
	{
		SetCursorRequested(false);
	});
	Input->BindActionInstanceLambda(ShowCursorAction, ETriggerEvent::Canceled, [this](const FInputActionInstance&)
	{
		SetCursorRequested(false);
	});
}

void AZooPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// AR view: the cursor is free to click the panels, holding the right button looks around
	if (bARViewActive && !bDestinationMenuOpen && !bCursorRequested) SetLookIgnored(!IsInputKeyDown(EKeys::RightMouseButton));
}

// ====== Narrative ====== //

void AZooPlayerController::HandleNarrativeListening()
{
	UWzNarrativeSubsystem* Narrative = GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>();
	for (const auto& Place : PathGuide->GetPlacesInside()) Narrative->SendEvent(Place);
}

void AZooPlayerController::HandlePathGuideStateChanged(const EPathGuideState NewState, const FGameplayTag&)
{
	if (NewState != EPathGuideState::Guiding) return;

	GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>()->SendEvent(FGameplayTag::RequestGameplayTag(TEXT("Narrative.Event.Zoo.NavigationStarted")));
}

// ====== AR ====== //

void AZooPlayerController::HandleARApproach(AZooARExhibit* Exhibit)
{
	// Only a Duma who is present: one the narrative despawned stays gone
	AZooDumaController* Brain = FindDumaBrain();
	if (bDumaDismissedByAR || !Brain || Brain->IsDespawned()) return;

	Brain->Despawn();
	bDumaDismissedByAR = true;
}

void AZooPlayerController::HandleARAppearStarted(AZooARExhibit* Exhibit)
{
	StartedExhibits.Add(Exhibit);

	// The panels need the cursor
	bARViewActive = true;
	ApplyInputMode();
}

void AZooPlayerController::HandleARAppearCompleted(AZooARExhibit* Exhibit)
{
	AppearedExhibits.Add(Exhibit);
}

void AZooPlayerController::HandleARInteractionEnded(AZooARExhibit* Exhibit)
{
	// Back to the game input mode (the exhibit already dropped its own cursor)
	bARViewActive = false;
	ApplyInputMode();

	if (!StartedExhibits.Contains(Exhibit)) RestoreDuma();
}

void AZooPlayerController::HandleARHidden(AZooARExhibit* Exhibit)
{
	// A session without an AR session start never sent InteractionEnded
	bARViewActive = false;
	ApplyInputMode();

	// Duma first, so a quest that completes on this visit starts its line with him present
	RestoreDuma();

	// Only a visit that finished showing counts
	if (AppearedExhibits.Contains(Exhibit))
	{
		UWzNarrativeSubsystem* Narrative = GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>();
		for (const auto& [ActorTag, Event] : ARVisitEvents)
		{
			if (Exhibit->ActorHasTag(ActorTag)) Narrative->SendEvent(Event);
		}
	}

	StartedExhibits.Remove(Exhibit);
	AppearedExhibits.Remove(Exhibit);
}

void AZooPlayerController::RestoreDuma()
{
	if (!bDumaDismissedByAR) return;

	bDumaDismissedByAR = false;
	if (AZooDumaController* Brain = FindDumaBrain()) Brain->Spawn();
}

AZooDumaController* AZooPlayerController::FindDumaBrain() const
{
	const AZooDuma* Duma = Cast<AZooDuma>(UGameplayStatics::GetActorOfClass(this, AZooDuma::StaticClass()));

	return Duma ? Duma->GetController<AZooDumaController>() : nullptr;
}

// ====== Destination Menu ====== //

void AZooPlayerController::SetDestinationMenuOpen(const bool bOpen)
{
	if (bDestinationMenuOpen == bOpen) return;

	bDestinationMenuOpen = bOpen;
	ApplyInputMode();
	OnDestinationMenuChanged.Broadcast(bOpen);
}

// ====== Input Mode ====== //

void AZooPlayerController::ApplyInputMode()
{
	// The list or Alt reveals the cursor. The game keeps the keys, so N / Esc still reach this controller.
	if (bDestinationMenuOpen || bCursorRequested)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		SetCursorVisible(true);
		SetLookIgnored(true);
	}
	else if (bARViewActive)
	{
		// The cursor stays visible; PlayerTick frees look while the right button is held (the cursor hides during that capture)
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(true);
		SetInputMode(Mode);
		SetCursorVisible(true);
		SetLookIgnored(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetCursorVisible(false);
		SetLookIgnored(false);
	}

	// The open list also stops walking.
	SetMoveIgnored(bDestinationMenuOpen);
}

void AZooPlayerController::SetCursorRequested(const bool bRequested)
{
	if (bCursorRequested == bRequested) return;

	bCursorRequested = bRequested;
	ApplyInputMode();
}

void AZooPlayerController::SetCursorVisible(const bool bVisible)
{
	bShowMouseCursor = bVisible;
	bEnableClickEvents = bVisible;
	bEnableMouseOverEvents = bVisible;
}

void AZooPlayerController::SetLookIgnored(const bool bIgnored)
{
	if (bIgnored == bLookIgnored) return;

	SetIgnoreLookInput(bIgnored);
	bLookIgnored = bIgnored;
}

void AZooPlayerController::SetMoveIgnored(const bool bIgnored)
{
	if (bIgnored == bMoveIgnored) return;

	SetIgnoreMoveInput(bIgnored);
	bMoveIgnored = bIgnored;
}
