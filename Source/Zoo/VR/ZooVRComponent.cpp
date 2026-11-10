// Copyright Epic Games, Inc. All Rights Reserved.

#include "VR/ZooVRComponent.h"

#include "AR/ZooARExhibit.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Layout/WidgetPath.h"
#include "Materials/MaterialInterface.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "MotionControllerComponent.h"
#include "Player/ZooPlayerController.h"
#include "TimerManager.h"
#include "UI/ZooViewClutter.h"
#include "VR/Demo/ZooVRDemoDirector.h"
#include "VR/ZooVRPanelHost.h"
#include "XRDeviceVisualizationComponent.h"

// ====== Lifecycle ====== //

UZooVRComponent::UZooVRComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	RelayWidgetClasses.Add(UUserWidget::StaticClass());
}

void UZooVRComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only with a headset (or the dev autopilot demo): desktop play stays exactly the Zoo's
	APlayerController* Controller = GetController();
	bDemo = UZooVRDemoDirector::IsRequested();
	bVRActive = Controller && Controller->IsLocalController() && (bDemo || UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled());
	SetComponentTickEnabled(bVRActive);
	if (!bVRActive) return;

	// Demo: the director poses the rig before this component aims the laser
	if (bDemo)
	{
		Demo = NewObject<UZooVRDemoDirector>(Controller, TEXT("ZooVRDemo"));
		Demo->RegisterComponent();
		PrimaryComponentTick.AddPrerequisite(Demo, Demo->PrimaryComponentTick);
	}

	// The HUD lives on the panels here: Duma keeps clear of them through this source
	FZooViewClutter::Source.BindUObject(this, &UZooVRComponent::QueryClutter);

	// Performance, input, rig (the pawn may come later), panels
	ApplyConsoleVariables();
	BindInput();
	Controller->GetOnNewPawnNotifier().AddUObject(this, &UZooVRComponent::HandleNewPawn);
	HandleNewPawn(Controller->GetPawn());
	GetWorld()->GetTimerManager().SetTimer(RelayTimer, this, &UZooVRComponent::RelayScreenWidgets, RelayInterval, true);
}

void UZooVRComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bVRActive)
	{
		// Undo what the session changed outside this component
		RestoreConsoleVariables();
		ClearHover();
		FZooViewClutter::Source.Unbind();
		GetWorld()->GetTimerManager().ClearTimer(RelayTimer);
		if (APlayerController* Controller = GetController())
		{
			Controller->GetOnNewPawnNotifier().RemoveAll(this);
			if (VRInput) Controller->PopInputComponent(VRInput);
		}
		if (PanelActor) PanelActor->Destroy();
	}

	Super::EndPlay(EndPlayReason);
}

void UZooVRComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePanels(DeltaTime);
	UpdatePointerTarget();
	UpdateHover(DeltaTime);
	UpdateHoldScroll(DeltaTime);

	// Controller buttons in the UI hints
	PromptTimer += DeltaTime;
	if (PromptTimer >= PromptInterval)
	{
		PromptTimer = 0.f;
		ReplacePrompts();
	}
}

// ====== Input ====== //

void UZooVRComponent::BindInput()
{
	APlayerController* Controller = GetController();

	// Mapping context
	const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Subsystem && VRMappingContext) Subsystem->AddMappingContext(VRMappingContext, VRMappingPriority);

	// Own input component on the controller's stack (the Zoo's bindings stay untouched)
	VRInput = NewObject<UEnhancedInputComponent>(Controller, TEXT("ZooVRInput"));
	VRInput->RegisterComponent();
	VRInput->bBlockInput = false;
	Controller->PushInputComponent(VRInput);

	// Walk
	if (MoveAction) VRInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UZooVRComponent::OnMove);

	// Snap turn
	if (SnapTurnAction)
	{
		VRInput->BindAction(SnapTurnAction, ETriggerEvent::Triggered, this, &UZooVRComponent::OnSnapTurn);
		VRInput->BindActionValueLambda(SnapTurnAction, ETriggerEvent::Completed, [this](const FInputActionValue&)
		{
			bSnapTurnArmed = true;
		});
	}

	// Select (trigger)
	if (SelectAction)
	{
		VRInput->BindAction(SelectAction, ETriggerEvent::Started, this, &UZooVRComponent::OnSelectStarted);
		VRInput->BindAction(SelectAction, ETriggerEvent::Completed, this, &UZooVRComponent::OnSelectCompleted);
	}

	// Menu (A)
	if (MenuAction)
	{
		VRInput->BindAction(MenuAction, ETriggerEvent::Started, this, &UZooVRComponent::OnMenuStarted);
		VRInput->BindAction(MenuAction, ETriggerEvent::Completed, this, &UZooVRComponent::OnMenuCompleted);
	}

	// Scroll
	if (ScrollAction) VRInput->BindAction(ScrollAction, ETriggerEvent::Triggered, this, &UZooVRComponent::OnScroll);
}

void UZooVRComponent::OnMove(const FInputActionValue& Value)
{
	const APlayerController* Controller = GetController();
	ACharacter* Visitor = Controller ? Controller->GetPawn<ACharacter>() : nullptr;
	if (!Visitor || !Controller->PlayerCameraManager) return;

	// Forward / right of the head on the ground plane (the AR flow's move locks still apply)
	const FRotationMatrix Yaw { FRotator{ 0.f, Controller->PlayerCameraManager->GetCameraRotation().Yaw, 0.f } };
	const FVector2D Stick = Value.Get<FVector2D>();
	Visitor->AddMovementInput(Yaw.GetUnitAxis(EAxis::X), Stick.Y);
	Visitor->AddMovementInput(Yaw.GetUnitAxis(EAxis::Y), Stick.X);
}

void UZooVRComponent::OnSnapTurn(const FInputActionValue& Value)
{
	APlayerController* Controller = GetController();
	const float Stick = Value.Get<float>();
	if (!Controller || !bSnapTurnArmed || FMath::Abs(Stick) < SnapTurnThreshold || Controller->IsLookInputIgnored()) return;

	// One step per push
	bSnapTurnArmed = false;
	Controller->SetControlRotation(Controller->GetControlRotation() + FRotator{ 0.f, FMath::Sign(Stick) * SnapTurnAngle, 0.f });
}

void UZooVRComponent::OnSelectStarted()
{
	// Decide on release: a tap clicks, a sweep scrolls
	bTriggerHeld = true;
	bScrolling = false;
	StartPitch = GetLaserPitch();
}

void UZooVRComponent::OnSelectCompleted()
{
	if (bTriggerHeld && !bScrolling) Click();
	bTriggerHeld = false;
	bScrolling = false;
}

void UZooVRComponent::OnMenuStarted()
{
	// Decide on release: a tap toggles the list, a hold scrolls
	bMenuHeld = true;
	bScrolling = false;
	HeldTime = 0.f;
}

void UZooVRComponent::OnMenuCompleted()
{
	AZooPlayerController* Visitor = Cast<AZooPlayerController>(GetController());
	if (bMenuHeld && !bScrolling && Visitor) Visitor->SetDestinationMenuOpen(!Visitor->IsDestinationMenuOpen());
	bMenuHeld = false;
	bScrolling = false;
}

void UZooVRComponent::OnScroll(const FInputActionValue& Value)
{
	// Wheel notches over time (stick up = wheel up)
	Scroll(Value.Get<float>() * ScrollSpeed * GetWorld()->GetDeltaSeconds());
}

void UZooVRComponent::UpdateHoldScroll(const float DeltaTime)
{
	if (!bTriggerHeld && !bMenuHeld) return;

	// Becomes a scroll: the trigger after a sweep, A after a hold
	const float Pitch = GetLaserPitch();
	HeldTime += DeltaTime;
	if (!bScrolling && ((bTriggerHeld && FMath::Abs(Pitch - StartPitch) > DragThreshold) || (bMenuHeld && HeldTime > HoldTime)))
	{
		bScrolling = true;
		LastPitch = Pitch;
		ScrollRemainder = 0.f;
	}
	if (!bScrolling) return;

	// Laser up = wheel up, in whole notches
	ScrollRemainder += (Pitch - LastPitch) / DegreesPerNotch;
	LastPitch = Pitch;
	const float Notches = FMath::TruncToFloat(ScrollRemainder);
	if (Notches == 0.f) return;

	ScrollRemainder -= Notches;
	Scroll(Notches);
}

void UZooVRComponent::Click()
{
	// The extended video panel has no buttons: the first click plays it (P), the next closes it (X), the desktop's keys
	if (Pointer && IsMediaExtendPanel(Pointer->GetHoveredWidgetComponent()))
	{
		SimulateKey(bMediaExtendPlayed ? EKeys::X : EKeys::P);
		bMediaExtendPlayed = !bMediaExtendPlayed;

		return;
	}

	// A VR panel: the laser clicks it
	if (IsPointingAtVRPanel())
	{
		Pointer->PressPointerKey(EKeys::LeftMouseButton);
		Pointer->ReleasePointerKey(EKeys::LeftMouseButton);

		return;
	}

	// Otherwise a left click, exactly like the desktop: the AR exhibits trace it along the laser (GetPointerRay)
	SimulateLeftClick(IE_Pressed);
	if (bDemo)
	{
		for (TActorIterator<AZooARExhibit> It(GetWorld()); It; ++It)
		{
			if (!It->MouseInteraction || !It->IsZoneActive()) continue;
			FString Path;
			for (const TWeakPtr<SWidget>& Weak : It->MouseInteraction->GetHoveredWidgetPath().Widgets)
			{
				if (const TSharedPtr<SWidget> Widget = Weak.Pin()) Path += Widget->GetType().ToString() + TEXT(" ");
			}
			UE_LOG(LogTemp, Log, TEXT("ZooVR AR click %s: interact %d info %d over %d hovered %s path [%s]"), *It->GetActorNameOrLabel(), It->HasPresentationStarted() && It->IsLookingAtHabitat(), It->IsInformationOpen(), It->MouseInteraction->IsOverHitTestVisibleWidget(), *GetNameSafe(It->MouseInteraction->GetHoveredWidgetComponent()), *Path);
		}
	}
	SimulateLeftClick(IE_Released);
}

void UZooVRComponent::Scroll(const float Notches)
{
	// A VR panel: the laser scrolls it
	if (IsPointingAtVRPanel())
	{
		Pointer->ScrollWheel(Notches);

		return;
	}

	// Otherwise the mouse wheel, which the AR exhibits forward to their panel under the laser
	APlayerController* Controller = GetController();
	if (Controller) Controller->InputKey(FInputKeyEventArgs{ nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::MouseWheelAxis, Notches, GetWorld()->GetDeltaSeconds(), 1, FPlatformTime::Cycles64() });
}

float UZooVRComponent::GetLaserPitch() const
{
	return Pointer ? Pointer->GetForwardVector().Rotation().Pitch : 0.f;
}

void UZooVRComponent::UpdatePointerTarget()
{
	if (!Pointer) return;

	// Laser ray; an empty hit still draws the full laser
	const FVector Start = Pointer->GetComponentLocation();
	const FVector End = Start + Pointer->GetForwardVector() * PointerDistance;
	FHitResult Best;
	Best.TraceStart = Start;
	Best.TraceEnd = End;

	// Each panel on its own (they draw on top of the world): the nearest one with a button or scroll box there wins
	const FCollisionQueryParams Params { SCENE_QUERY_STAT(ZooVRPointer) };
	UWidgetComponent* BestPanel = nullptr;
	for (const auto& Panel : Panels)
	{
		FHitResult Hit;
		if (!Panel || !Panel->IsVisible() || !Panel->LineTraceComponent(Hit, Start, End, Params)) continue;
		if (Best.bBlockingHit && Hit.Distance >= Best.Distance) continue;
		if (!HasInteractiveWidgetAt(Panel, Hit.ImpactPoint) && !IsMediaExtendPanel(Panel)) continue;

		Best = Hit;
		BestPanel = Panel;
	}

	// The laser resolves the widget from the hit's component: make sure it names the panel
	if (BestPanel)
	{
		Best.bBlockingHit = true;
		Best.Component = BestPanel;
		Best.HitObjectHandle = FActorInstanceHandle{ BestPanel->GetOwner() };
	}
	Pointer->SetCustomHitResult(Best);
	UpdateLaserVisibility(Best.bBlockingHit, Start, End);
}

void UZooVRComponent::UpdateLaserVisibility(const bool bOverVRWidget, const FVector& Start, const FVector& End)
{
	// Something to touch in the world: an AR exhibit's panel or hologram in the laser's way
	bool bWanted = bOverVRWidget;
	if (!bWanted)
	{
		FCollisionQueryParams Params { SCENE_QUERY_STAT(ZooVRLaser) };
		if (const APlayerController* Controller = GetController()) Params.AddIgnoredActor(Controller->GetPawn());
		if (PanelActor) Params.AddIgnoredActor(PanelActor);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) bWanted = Hit.GetComponent() && (Hit.GetComponent()->IsA<UWidgetComponent>() || Cast<AZooARExhibit>(Hit.GetActor()) != nullptr);
	}

	// Or a list / extra panel to pick from (the destination menu, the extended media)
	const AZooPlayerController* Visitor = Cast<AZooPlayerController>(GetController());
	if (!bWanted) bWanted = (Visitor && Visitor->IsDestinationMenuOpen()) || Panels.Num() > 2;

	// Shown while wanted and a moment after
	LaserLinger = bWanted ? LaserLingerTime : FMath::Max(LaserLinger - GetWorld()->GetDeltaSeconds(), 0.f);
	Pointer->bShowDebug = LaserLinger > 0.f;
}

bool UZooVRComponent::IsMediaExtendPanel(const UWidgetComponent* Panel) const
{
	const int32 Index = Panel ? Panels.IndexOfByKey(Panel) : INDEX_NONE;
	const UUserWidget* Hosted = Index != INDEX_NONE && Hosts.IsValidIndex(Index) && Hosts[Index] ? Hosts[Index]->GetHostedWidget() : nullptr;

	return Hosted && Hosted->GetClass()->GetName().Contains(MediaExtendWidgetName);
}

void UZooVRComponent::SimulateKey(const FKey& Key) const
{
	APlayerController* Controller = GetController();
	if (!Controller) return;

	Controller->InputKey(FInputKeyEventArgs{ nullptr, FInputDeviceId::CreateFromInternalId(0), Key, IE_Pressed, 1.f, false, FPlatformTime::Cycles64() });
	Controller->InputKey(FInputKeyEventArgs{ nullptr, FInputDeviceId::CreateFromInternalId(0), Key, IE_Released, 0.f, false, FPlatformTime::Cycles64() });
}

bool UZooVRComponent::HasInteractiveWidgetAt(UWidgetComponent* Panel, const FVector& WorldLocation)
{
	FVector2D LocalLocation;
	Panel->GetLocalHitLocation(WorldLocation, LocalLocation);

	// Buttons (cards, Navigasi, close) or a scroll box / bar under that point
	for (const auto& Arranged : Panel->GetHitWidgetPath(LocalLocation, false))
	{
		const FString Type = Arranged.Widget->GetType().ToString();
		if ((Arranged.Widget->IsInteractable() && Arranged.Widget->IsEnabled()) || Type.Contains(TEXT("Scroll"))) return true;
	}

	return false;
}

bool UZooVRComponent::IsPointingAtVRPanel() const
{
	if (!Pointer || !Pointer->IsOverHitTestVisibleWidget()) return false;

	return Panels.Contains(Pointer->GetHoveredWidgetComponent());
}

void UZooVRComponent::SimulateLeftClick(const EInputEvent Event) const
{
	APlayerController* Controller = GetController();
	if (!Controller) return;

	Controller->InputKey(FInputKeyEventArgs{ nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::LeftMouseButton, Event, Event == IE_Pressed ? 1.f : 0.f, false, FPlatformTime::Cycles64() });
}

// ====== Rig ====== //

void UZooVRComponent::HandleNewPawn(APawn* Pawn)
{
	if (Pawn) BuildRig(Pawn);
}

void UZooVRComponent::BuildRig(APawn* Pawn)
{
	UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
	const ACharacter* Visitor = Cast<ACharacter>(Pawn);
	if (!Camera) return;

	// Tracking origin at the capsule's feet; the camera follows the headset from there
	VROrigin = NewObject<USceneComponent>(Pawn, TEXT("ZooVR_Origin"));
	VROrigin->SetupAttachment(Pawn->GetRootComponent());
	VROrigin->SetRelativeLocation({ 0.f, 0.f, Visitor ? -Visitor->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 0.f });
	VROrigin->RegisterComponent();
	Camera->AttachToComponent(VROrigin, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Camera->bUsePawnControlRotation = false;
	// Demo: the director poses the camera itself (no headset to lock to)
	Camera->bLockToHmd = !bDemo;
	HeadCamera = Camera;
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);

	// Hands with the runtime's controller models (the demo shows its own hand meshes instead)
	const auto MakeHand = [this, Pawn](const TCHAR* Name, const FName Source)
	{
		UMotionControllerComponent* Hand = NewObject<UMotionControllerComponent>(Pawn, Name);
		Hand->SetupAttachment(VROrigin);
		Hand->SetTrackingMotionSource(Source);
		Hand->RegisterComponent();

		return Hand;
	};
	for (const FName Source : { FName{ TEXT("LeftGrip") }, FName{ TEXT("RightGrip") } })
	{
		UMotionControllerComponent* Grip = MakeHand(*FString::Printf(TEXT("ZooVR_%s"), *Source.ToString()), Source);
		if (Source == TEXT("LeftGrip")) LeftGrip = Grip;
		else RightGrip = Grip;
		if (bDemo) continue;

		UXRDeviceVisualizationComponent* Model = NewObject<UXRDeviceVisualizationComponent>(Pawn, *FString::Printf(TEXT("ZooVR_%sModel"), *Source.ToString()));
		Model->SetupAttachment(Grip);
		Model->MotionSource = Source;
		Model->RegisterComponent();
		Model->SetIsVisualizationActive(true);
	}

	// Laser on the right aim pose
	UMotionControllerComponent* Aim = MakeHand(TEXT("ZooVR_RightAim"), TEXT("RightAim"));
	RightAim = Aim;
	Pointer = NewObject<UWidgetInteractionComponent>(Pawn, TEXT("ZooVR_Pointer"));
	Pointer->SetupAttachment(Aim);
	// Custom: UpdatePointerTarget picks the panel each tick, before the laser ticks. Its own virtual user and pointer:
	// the AR exhibits' mouse interaction owns user / pointer 0 and would clear this laser's hover every frame.
	Pointer->InteractionSource = EWidgetInteractionSource::Custom;
	Pointer->VirtualUserIndex = 1;
	Pointer->PointerIndex = 1;
	Pointer->TraceChannel = ECC_Visibility;
	Pointer->InteractionDistance = PointerDistance;
	Pointer->bShowDebug = true;
	Pointer->DebugColor = LaserColor;
	Pointer->DebugLineThickness = 0.4f;
	Pointer->DebugSphereLineThickness = 0.4f;
	Pointer->RegisterComponent();
	Pointer->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
}

bool UZooVRComponent::GetPointerRay(const APlayerController* Controller, FVector& OutStart, FVector& OutDirection)
{
	const UZooVRComponent* VR = Controller ? Controller->FindComponentByClass<UZooVRComponent>() : nullptr;
	if (!VR || !VR->bVRActive || !VR->Pointer) return false;

	OutStart = VR->Pointer->GetComponentLocation();
	OutDirection = VR->Pointer->GetForwardVector();

	return true;
}

// ====== Panels ====== //

void UZooVRComponent::RelayScreenWidgets()
{
	APlayerController* Controller = GetController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn) return;

	for (const auto& RelayClass : RelayWidgetClasses)
	{
		if (!RelayClass) continue;

		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, RelayClass, true);
		for (const auto Widget : Found)
		{
			if (!Widget->IsInViewport()) continue;

			// The extended video finishes its own opening on the screen first (its FinishExtend leaves when it is not in the viewport)
			if (Widget->GetClass()->GetName().Contains(MediaExtendWidgetName))
			{
				const double Now = GetWorld()->GetTimeSeconds();
				const double& Seen = FirstSeenInViewport.FindOrAdd(Widget, Now);
				if (Now - Seen < MediaExtendRelayDelay) continue;
			}

			// Off the screen, into a host (so the game's own RemoveFromParent closes it)
			Widget->RemoveFromParent();
			HideScreenOnlyWidgets(Widget);
			UZooVRPanelHost* PanelHost = CreateWidget<UZooVRPanelHost>(Controller, UZooVRPanelHost::StaticClass());
			PanelHost->Host(Widget);
			// A fresh extended video waits for its first click (play)
			if (Widget->GetClass()->GetName().Contains(MediaExtendWidgetName)) bMediaExtendPlayed = false;

			// Panel actor, once: not the pawn (the laser ignores its owner), ignored by the AR exhibits' trace
			if (!PanelActor)
			{
				PanelActor = GetWorld()->SpawnActor<AActor>();
				USceneComponent* Root = NewObject<USceneComponent>(PanelActor, TEXT("Root"));
				PanelActor->SetRootComponent(Root);
				Root->RegisterComponent();
				PanelActor->Tags.Add(TEXT("ARIgnoreTrace"));
			}

			// World panel, placed by UpdatePanels
			UWidgetComponent* Panel = NewObject<UWidgetComponent>(PanelActor);
			Panel->SetupAttachment(PanelActor->GetRootComponent());
			Panel->SetUsingAbsoluteLocation(true);
			Panel->SetUsingAbsoluteRotation(true);
			Panel->SetUsingAbsoluteScale(true);
			Panel->SetWorldScale3D(FVector{ PanelWidth / PanelDrawSize.X });
			Panel->SetWidgetSpace(EWidgetSpace::World);
			Panel->SetDrawSize(FVector2D{ PanelDrawSize });
			Panel->SetPivot({ 0.5f, 0.5f });
			Panel->SetTwoSided(false);
			Panel->SetBlendMode(EWidgetBlendMode::Transparent);
			if (UMaterialInterface* Material = PanelMaterial.LoadSynchronous()) Panel->SetMaterial(0, Material);
			Panel->SetCastShadow(false);
			Panel->SetWindowFocusable(false);
			Panel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Panel->SetCollisionResponseToAllChannels(ECR_Ignore);
			Panel->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			Panel->SetWidget(PanelHost);
			Panel->RegisterComponent();
			Panels.Add(Panel);
			Hosts.Add(PanelHost);

			// A new panel (e.g. the extended media) appears right in front of the visitor
			bHasTarget = false;
			UE_LOG(LogTemp, Log, TEXT("ZooVR panel: relayed %s (%s), %d panels"), *Widget->GetName(), *Widget->GetClass()->GetName(), Panels.Num());
		}
	}
}

void UZooVRComponent::HideScreenOnlyWidgets(UUserWidget* Widget) const
{
	if (!Widget || !Widget->WidgetTree || HiddenWidgetNames.IsEmpty()) return;

	Widget->WidgetTree->ForEachWidgetAndDescendants([this](UWidget* Child)
	{
		if (HiddenWidgetNames.Contains(Child->GetFName())) Child->SetVisibility(ESlateVisibility::Collapsed);
	});
}

void UZooVRComponent::UpdatePanels(const float DeltaTime)
{
	// Closed widgets take their panel with them
	for (int32 Index = Panels.Num() - 1; Index >= 0; --Index)
	{
		if (Hosts[Index] && !Hosts[Index]->IsEmpty()) continue;

		UE_LOG(LogTemp, Log, TEXT("ZooVR panel: closed panel %d"), Index);
		if (Panels[Index]) Panels[Index]->DestroyComponent();
		Panels.RemoveAt(Index);
		Hosts.RemoveAt(Index);
	}

	FVector Head;
	FRotator Yaw;
	if (Panels.IsEmpty() || !GetHeadView(Head, Yaw)) return;

	// Where the panel belongs right now
	const FVector Desired = Head + Yaw.Vector() * PanelDistance + FVector{ 0.f, 0.f, PanelHeightOffset };
	const FRotator Facing = Yaw + FRotator{ 0.f, 180.f, 0.f };

	// First frame: snap. Then follow only after a large turn or a walk.
	if (!bHasTarget)
	{
		bHasTarget = true;
		bFollowing = true;
		TargetLocation = Desired;
		TargetRotation = Facing;
	}
	const FVector ToPanel = (TargetLocation - Head).GetSafeNormal2D();
	const float AngleOff = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToPanel, Yaw.Vector()), -1.f, 1.f)));
	if (AngleOff > FollowAngle || FVector::Dist2D(Head, AnchorLocation) > FollowDistance) bFollowing = true;

	// Glide in front of the head, then rest there
	if (bFollowing)
	{
		TargetLocation = FMath::VInterpTo(TargetLocation, Desired, DeltaTime, FollowSpeed);
		TargetRotation = FMath::RInterpTo(TargetRotation, Facing, DeltaTime, FollowSpeed);
		if (FVector::Dist(TargetLocation, Desired) < 2.f)
		{
			bFollowing = false;
			AnchorLocation = Head;
		}
	}

	// Stacked panels, a hair apart so they never z-fight
	for (int32 Index = 0; Index < Panels.Num(); ++Index)
	{
		if (Panels[Index]) Panels[Index]->SetWorldLocationAndRotation(TargetLocation - Yaw.Vector() * (0.5f * Index), TargetRotation);
	}
}

// ====== Hover ====== //

void UZooVRComponent::UpdateHover(const float DeltaTime)
{
	// The button the laser hovers on its panel
	const int32 PanelIndex = Pointer ? Panels.IndexOfByKey(Pointer->GetHoveredWidgetComponent()) : INDEX_NONE;
	UZooVRPanelHost* Host = PanelIndex != INDEX_NONE ? Hosts[PanelIndex].Get() : nullptr;
	UButton* Button = Host ? FindHoveredButton(Host->GetHostedWidget()) : nullptr;

	// A new button: the old one goes back at once, the new one grows from its own scale
	if (Button != HoveredButton.Get())
	{
		ClearHover();
		HoveredButton = Button;
		HoveredHost = Host;
		HoverAlpha = 0.f;
		if (Button)
		{
			HoveredBaseScale = Button->GetRenderTransform().Scale;
			HoveredBasePivot = Button->GetRenderTransformPivot();
			Button->SetRenderTransformPivot({ 0.5f, 0.5f });
		}
	}
	if (!Button) return;

	// Grow and glow in
	HoverAlpha = FMath::FInterpTo(HoverAlpha, 1.f, DeltaTime, HoverSpeed);
	Button->SetRenderScale(HoveredBaseScale * FMath::Lerp(1.f, HoverScale, HoverAlpha));
	Host->SetHighlight(Button, HoverGlowColor, HoverAlpha);
}

void UZooVRComponent::ClearHover()
{
	if (UButton* Button = HoveredButton.Get())
	{
		Button->SetRenderScale(HoveredBaseScale);
		Button->SetRenderTransformPivot(HoveredBasePivot);
	}
	if (UZooVRPanelHost* Host = HoveredHost.Get()) Host->SetHighlight(nullptr, HoverGlowColor, 0.f);

	HoveredButton.Reset();
	HoveredHost.Reset();
	HoverAlpha = 0.f;
}

UButton* UZooVRComponent::FindHoveredButton(const UUserWidget* Widget)
{
	if (!Widget || !Widget->WidgetTree) return nullptr;

	// Cards are user widgets inside the list: look inside them too
	TArray<UWidget*> Children;
	Widget->WidgetTree->GetAllWidgets(Children);
	for (UWidget* Child : Children)
	{
		UButton* Button = Cast<UButton>(Child);
		if (Button && Button->GetIsEnabled() && Button->IsHovered()) return Button;

		if (UButton* Nested = FindHoveredButton(Cast<UUserWidget>(Child))) return Nested;
	}

	return nullptr;
}

void UZooVRComponent::QueryClutter(const APlayerController* Controller, TArray<FZooViewRect>& OutRects) const
{
	const APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	if (!Camera) return;

	// Every HUD element on every panel: its corners on the panel, as angles from the camera
	const FVector CamLoc = Camera->GetCameraLocation();
	const FRotator CamRot = Camera->GetCameraRotation();
	for (int32 Index = 0; Index < Panels.Num(); ++Index)
	{
		const UWidgetComponent* Panel = Panels[Index];
		const UZooVRPanelHost* Host = Hosts.IsValidIndex(Index) ? Hosts[Index].Get() : nullptr;
		if (!Panel || !Host || !Panel->IsVisible()) continue;

		TArray<const UUserWidget*> Elements;
		FZooViewClutter::CollectElements(Host->GetHostedWidget(), Elements);
		const FGeometry& HostGeometry = Host->GetCachedGeometry();
		const FVector2D Draw = Panel->GetDrawSize();
		const FVector2D Pivot = Panel->GetPivot();
		for (const UUserWidget* Element : Elements)
		{
			const FGeometry& Geometry = Element->GetCachedGeometry();
			const FVector2D TopLeft = HostGeometry.AbsoluteToLocal(Geometry.GetAbsolutePosition());
			const FVector2D BottomRight = HostGeometry.AbsoluteToLocal(Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize());
			FZooViewRect Rect { { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };
			for (const FVector2D& Local : { TopLeft, FVector2D{ BottomRight.X, TopLeft.Y }, BottomRight, FVector2D{ TopLeft.X, BottomRight.Y } })
			{
				// Inverse of GetLocalHitLocation, then into the camera's view
				const FVector World = Panel->GetComponentTransform().TransformPosition(FVector{ 0.0, -(Local.X - Draw.X * Pivot.X), -(Local.Y - Draw.Y * Pivot.Y) });
				const FVector2D Angles = FZooViewClutter::AnglesOf(CamRot.UnrotateVector(World - CamLoc));
				Rect.Min = FVector2D::Min(Rect.Min, Angles);
				Rect.Max = FVector2D::Max(Rect.Max, Angles);
			}
			OutRects.Add(Rect);
		}
	}
}

bool UZooVRComponent::GetHeadView(FVector& OutLocation, FRotator& OutYaw) const
{
	const APlayerController* Controller = GetController();
	const APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	if (!Camera) return false;

	OutLocation = Camera->GetCameraLocation();
	OutYaw = { 0.f, Camera->GetCameraRotation().Yaw, 0.f };

	return true;
}

// ====== Prompts ====== //

void UZooVRComponent::ReplacePrompts()
{
	if (PromptReplacements.IsEmpty()) return;

	// Every user widget, screen or world (VR panels, AR information panels, media)
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), false);
	for (const auto Widget : Widgets)
	{
		ReplacePromptsIn(Widget);
	}
}

void UZooVRComponent::ReplacePromptsIn(UUserWidget* Widget)
{
	if (!Widget || !Widget->WidgetTree) return;

	// The MEDIA card's body is not meant to scroll: never shorter than its content, so no button is cut off
	if (Widget->GetClass()->GetName().Contains(TEXT("WBP_AR_Media")) && !Widget->GetClass()->GetName().Contains(MediaExtendWidgetName))
	{
		USizeBox* Body = Cast<USizeBox>(Widget->WidgetTree->FindWidget(TEXT("Body")));
		const UScrollBox* Scroller = Cast<UScrollBox>(Widget->WidgetTree->FindWidget(TEXT("Scroller")));
		const float Need = Scroller ? Scroller->GetDesiredSize().Y + 8.f : 0.f;
		if (Body && Need > 0.f && Body->GetMinDesiredHeight() < Need) Body->SetMinDesiredHeight(Need);
	}

	Widget->WidgetTree->ForEachWidget([this](UWidget* Child)
	{
		ReplacePromptText(Child);
	});
}

void UZooVRComponent::ReplacePromptText(UWidget* Child) const
{
	// Plain and rich text
	UTextBlock* Plain = Cast<UTextBlock>(Child);
	URichTextBlock* Rich = Cast<URichTextBlock>(Child);
	if (!Plain && !Rich) return;

	// Keyboard hints -> controller buttons
	FString Text = Plain ? Plain->GetText().ToString() : Rich->GetText().ToString();
	bool bChanged = false;
	for (const auto& [Keyboard, Button] : PromptReplacements)
	{
		if (Text.Contains(Keyboard, ESearchCase::CaseSensitive)) bChanged |= Text.ReplaceInline(*Keyboard, *Button, ESearchCase::CaseSensitive) > 0;
	}
	if (!bChanged) return;

	if (Plain) Plain->SetText(FText::FromString(Text));
	else Rich->SetText(FText::FromString(Text));
}

// ====== Performance ====== //

void UZooVRComponent::ApplyConsoleVariables()
{
	for (const auto& [Name, VRValue] : VRConsoleVariables)
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
		if (!Variable) continue;

		// Remember, then set
		SavedConsoleVariables.Add(Name, Variable->GetString());
		Variable->Set(*VRValue, ECVF_SetByCode);
	}
}

void UZooVRComponent::RestoreConsoleVariables()
{
	for (const auto& [Name, SavedValue] : SavedConsoleVariables)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name)) Variable->Set(*SavedValue, ECVF_SetByCode);
	}

	SavedConsoleVariables.Reset();
}

// ====== Demo ====== //

void UZooVRComponent::SetDemoPoses(const FTransform& Head, const FTransform& Left, const FTransform& Right, const FTransform& Aim) const
{
	if (!bDemo) return;

	// The rig's components stand in for the tracked devices
	if (HeadCamera) HeadCamera->SetRelativeTransform(Head);
	if (LeftGrip) LeftGrip->SetRelativeTransform(Left);
	if (RightGrip) RightGrip->SetRelativeTransform(Right);
	if (RightAim) RightAim->SetRelativeTransform(Aim);
}

// ====== State ====== //

APlayerController* UZooVRComponent::GetController() const
{
	return GetOwner<APlayerController>();
}
