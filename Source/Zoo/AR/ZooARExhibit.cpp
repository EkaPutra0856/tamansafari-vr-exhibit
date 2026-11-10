    #include "AR/ZooARExhibit.h"

    #include "AR/ZooARHabitatMath.h"
    #include "AR/ZooAnimal.h"
    #include "AR/ZooARCameraFocusComponent.h"
    #include "Components/BoxComponent.h"
    #include "Components/InputComponent.h"
    #include "Components/SkeletalMeshComponent.h"
    #include "Components/WidgetComponent.h"
    #include "Components/WidgetInteractionComponent.h"
    #include "Engine/World.h"
    #include "GameFramework/Pawn.h"
    #include "GameFramework/PlayerController.h"
    #include "Hologram/ZooHologramComponent.h"
    #include "Kismet/GameplayStatics.h"
    #include "UI/ZooARInfoWidget.h"
    #include "VR/ZooVRComponent.h"

    namespace
    {
        constexpr float InteractionStartProgress = .75f;
        constexpr float LookAwayGracePeriod = .15f;
        constexpr float MouseTraceDistance = 15000.f;
        constexpr float ClickBoundsPadding = 8.f;
        constexpr float MinimumEntranceMoveSpeed = .1f;
        constexpr float MovingAnimationStartProgress = .01f;
        constexpr float StationaryAnimationStartProgress = .9f;
        constexpr float PanelBobScale = .45f;

        const FVector2D InformationDrawSize(1800., 1100.);
        const FVector2D InformationPivot(.5, .5);
        const FName IgnoreTraceTag(TEXT("ARIgnoreTrace"));

        int32 GZooARCursorUsers = 0;
    }

    AZooARExhibit::AZooARExhibit()
    {
        PrimaryActorTick.bCanEverTick = true;
        PrimaryActorTick.TickGroup = TG_PostUpdateWork;

        RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ExhibitAnchor"));
        HologramRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HologramRoot"));
        HologramRoot->SetupAttachment(RootComponent);

        AnimalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimalMesh"));
        AnimalMesh->SetupAttachment(HologramRoot);
        AnimalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        AnimalMesh->SetCastShadow(false);
        AnimalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

        Hologram = CreateDefaultSubobject<UZooHologramComponent>(TEXT("Hologram"));

        AnimalClickBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("AnimalClickBounds"));
        AnimalClickBounds->SetupAttachment(HologramRoot);
        AnimalClickBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        AnimalClickBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
        AnimalClickBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        AnimalClickBounds->SetGenerateOverlapEvents(false);
        AnimalClickBounds->SetHiddenInGame(true);

        InformationPanel = CreateDefaultSubobject<UWidgetComponent>(TEXT("InformationPanel"));
        InformationPanel->SetupAttachment(RootComponent);
        InformationPanel->SetWidgetSpace(EWidgetSpace::World);
        InformationPanel->SetDrawSize(InformationDrawSize);
        InformationPanel->SetPivot(InformationPivot);
        InformationPanel->SetTwoSided(true);
        InformationPanel->SetBlendMode(EWidgetBlendMode::Transparent);
        InformationPanel->SetCastShadow(false);
        InformationPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        InformationPanel->SetCollisionResponseToAllChannels(ECR_Ignore);
        InformationPanel->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

        MouseInteraction = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("MouseInteraction"));
        MouseInteraction->SetupAttachment(RootComponent);
        MouseInteraction->InteractionSource = EWidgetInteractionSource::Custom;
        MouseInteraction->InteractionDistance = MouseTraceDistance;

        ReferencePresentation = CreateDefaultSubobject<UZooARPresentationComponent>(TEXT("ReferencePresentation"));

        InformationWidgetClass = UZooARInfoWidget::StaticClass();
        HabitatBounds = FBox(EForceInit::ForceInit);
    }

    void AZooARExhibit::OnConstruction(const FTransform& Transform)
    {
        Super::OnConstruction(Transform);
        ConfigureExhibit();
    }

    void AZooARExhibit::BeginPlay()
    {
        Super::BeginPlay();
        ConfigureExhibit();

        LocalController = UGameplayStatics::GetPlayerController(this, 0);
        InitializeInput();
        MouseInteraction->PointerIndex = PointerIndex;

        // Our hologram look dresses the animal; the presentation only keeps the timing and the animations
        Hologram->SetTarget(AnimalMesh);
        if (ActivationMode == EZooARActivationMode::WaitForAnimal)
        {
            ReferencePresentation->Initialize(this);
        }

        InformationPanel->InitWidget();
        if (UZooARInfoWidget* Widget = GetInformationWidget())
        {
            Widget->BindExhibit(this);
        }

        AnimalMesh->bPauseAnims = true;
        if (bAnimateEntranceMovement)
        {
            AnimalMesh->SetRelativeLocation(EntranceStartLocation);
        }

        bAppearActive = false;
        bAppearCompleted = false;
        SetVisualsVisible(false);
        InformationPanel->SetVisibility(false);

        if (!bSafePlacement)
        {
            UE_LOG(LogTemp, Error, TEXT("AR %s disabled: its animal or information card crosses the habitat boundary."), *GetName());
        }
    }

    void AZooARExhibit::EndPlay(const EEndPlayReason::Type EndPlayReason)
    {
        EndARSession();
        HandleMouseReleased();
        ReleaseCursor();
        bAppearActive = false;
        bAppearCompleted = false;

        if (APlayerController* PC = LocalController.Get())
        {
            DisableInput(PC);
        }

        Super::EndPlay(EndPlayReason);
    }

    void AZooARExhibit::Tick(float DeltaSeconds)
    {
        Super::Tick(DeltaSeconds);
        UpdateActivation(DeltaSeconds);
        UpdateAnimation(DeltaSeconds);
        UpdateMouseTrace();
    }

    void AZooARExhibit::ConfigureExhibit()
    {
        RestHologramLocation = HologramRoot->GetRelativeLocation();
        ConfigureInformationPanel();
        CacheMeshBoundsAndPlacement();
        RestAnimalLocation = AnimalMesh->GetRelativeLocation();
    }

    void AZooARExhibit::ConfigureInformationPanel()
    {
        RestPanelLocation = GetActorLocation() + PanelOffset;
        InformationPanel->SetDrawSize(InformationDrawSize);
        InformationPanel->SetPivot(InformationPivot);
        InformationPanel->SetWorldLocation(RestPanelLocation);
        InformationPanel->SetWorldScale3D(FVector(PanelWorldScale));
        InformationPanel->SetWidgetClass(InformationWidgetClass);
    }

    void AZooARExhibit::CacheMeshBoundsAndPlacement()
    {
        bSafePlacement = false;
        if (!AnimalMesh->GetSkeletalMeshAsset())
        {
            return;
        }

        // Editor preview, click bounds and placement all use the entrance target.
        if (bAnimateEntranceMovement || ActivationMode == EZooARActivationMode::WaitForAnimal)
        {
            AnimalMesh->SetRelativeLocation(EntranceTargetLocation);
        }

        CachedMeshBounds = AnimalMesh->CalcBounds(AnimalMesh->GetRelativeTransform());
        AnimalClickBounds->SetRelativeLocation(CachedMeshBounds.Origin);
        AnimalClickBounds->SetBoxExtent(CachedMeshBounds.BoxExtent + FVector(ClickBoundsPadding));

        const FBoxSphereBounds WorldBounds = AnimalMesh->CalcBounds(AnimalMesh->GetComponentTransform());
        const double AnimalRadius = WorldBounds.SphereRadius + FloatAmplitude;
        const double PanelRadius = FVector2D(InformationPanel->GetDrawSize()).Length() * .5 * PanelWorldScale + FloatAmplitude;
        bSafePlacement = ZooAR::InsideHabitat(WorldBounds.Origin, HabitatBounds, HabitatCornerRadius, AnimalRadius)
            && ZooAR::InsideHabitat(RestPanelLocation, HabitatBounds, HabitatCornerRadius, PanelRadius);
    }

    void AZooARExhibit::InitializeInput()
    {
        if (APlayerController* PC = LocalController.Get())
        {
            EnableInput(PC);
            InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AZooARExhibit::HandleMousePressed).bConsumeInput = false;
            InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AZooARExhibit::HandleMouseReleased).bConsumeInput = false;

            FInputAxisKeyBinding& WheelBinding = InputComponent->BindAxisKey(EKeys::MouseWheelAxis);
            WheelBinding.bConsumeInput = false;
            WheelBinding.AxisDelegate.BindDelegate(this, &AZooARExhibit::HandleMouseScrolled);
        }
    }

    UZooARInfoWidget* AZooARExhibit::GetInformationWidget() const
    {
        return InformationPanel ? Cast<UZooARInfoWidget>(InformationPanel->GetUserWidgetObject()) : nullptr;
    }

    bool AZooARExhibit::IsVisitorInZone() const
    {
        const APlayerController* PC = LocalController.Get();
        const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Pawn || !ViewingZone || !bSafePlacement)
        {
            return false;
        }

        const UBoxComponent* Box = ViewingZone->FindComponentByClass<UBoxComponent>();
        if (!Box)
        {
            return false;
        }

        const FVector LocalPosition = Box->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
        const FVector Extent = Box->GetUnscaledBoxExtent();
        return FMath::Abs(LocalPosition.X) <= Extent.X
            && FMath::Abs(LocalPosition.Y) <= Extent.Y
            && FMath::Abs(LocalPosition.Z) <= Extent.Z;
    }

    bool AZooARExhibit::IsEntranceMovementComplete() const
    {
        return !bAnimateEntranceMovement || AnimalMesh->GetRelativeLocation().Equals(EntranceTargetLocation);
    }

    bool AZooARExhibit::CanInteract() const
    {
        return bZoneActive && bPresentationStarted && bLookingAtHabitat;
    }

    bool AZooARExhibit::CanInteractWithInformation() const
    {
        // Completion is recorded after visibility updates, so the widget appears on the following frame.
        return CanInteract() && (ActivationMode != EZooARActivationMode::WaitForAnimal
            || (bAppearCompleted && ReferencePresentation->IsComplete()));
    }

    bool AZooARExhibit::CanInteractWithAnimal() const
    {
        return CanInteract() && RevealProgress >= InteractionStartProgress && IsEntranceMovementComplete()
            && (ActivationMode != EZooARActivationMode::WaitForAnimal || ReferencePresentation->IsComplete());
    }

    FVector AZooARExhibit::GetAnimalTargetLocation() const
    {
        FTransform StableTransform = HologramRoot->GetComponentTransform();
        StableTransform.SetTranslation(GetRootComponent()->GetComponentTransform().TransformPosition(RestHologramLocation));
        const FVector Target = bAnimateEntranceMovement ? EntranceTargetLocation : RestAnimalLocation;
        return StableTransform.TransformPosition(Target);
    }

    AZooAnimal* AZooARExhibit::GetSelectedAnimal() const
    {
        AZooAnimal* Animal = SelectedAnimal.Get();
        return Animal && Animal->GetAssignedExhibit() == this ? Animal : nullptr;
    }

    void AZooARExhibit::NotifyAnimalArrived(AZooAnimal* Animal)
    {
        if (bZoneActive && bWaitingForAnimal && bARSessionStarted && GetSelectedAnimal() == Animal
            && IsValid(Animal) && Animal->GetAnimalState() == EZooAnimalState::WaitingAtExhibit)
        {
            StartAppearance();
        }
    }

    void AZooARExhibit::UpdateActivation(float DeltaSeconds)
    {
        const bool bWasActive = bZoneActive;
        // A visitor whose input is locked (dialogue, menu) enters once it is free, so the deck walk can take over
        const bool bInputLocked = LocalController.IsValid() && (LocalController->IsMoveInputIgnored() || LocalController->IsLookInputIgnored());
        bZoneActive = IsVisitorInZone() && (bWasActive || !bInputLocked);
        if (!bZoneActive && bWasActive)
        {
            ReleaseCursor();
            EndARSession();
        }

        bLookingAtHabitat = false;
        if (bZoneActive && LocalController.IsValid())
        {
            FVector Camera;
            FRotator Rotation;
            LocalController->GetPlayerViewPoint(Camera, Rotation);
            bLookingAtHabitat = ZooAR::RayIntersectsHabitat(Camera, Rotation.Vector(), HabitatBounds, HabitatCornerRadius);
        }

        if (bZoneActive && !bWasActive)
        {
            if (ActivationMode == EZooARActivationMode::WaitForAnimal)
            {
                BeginAnimalArrivalWait();
            }
            else
            {
                StartAppearance();
            }
        }
        else if (bZoneActive && bWaitingForAnimal)
        {
            UpdateAnimalArrivalWait(DeltaSeconds);
        }

        if (!bZoneActive || !bLookingAtHabitat)
        {
            LookAwayTime += DeltaSeconds;
            if (!bZoneActive || LookAwayTime >= LookAwayGracePeriod)
            {
                SetInformationOpen(false);
            }

            HandleMouseReleased();
        }
        else
        {
            LookAwayTime = 0.f;
        }
    }

    void AZooARExhibit::BeginAnimalArrivalWait()
    {
        ReferencePresentation->ResetPresentation();
        bWaitingForAnimal = true;
        bGuidedArrivalSession = bWalkToDeckCenterBeforeApproach;
        bWalkingToDeckCenter = bGuidedArrivalSession;
        bPresentationStarted = false;
        AnimalWaitElapsed = 0.f;
        RevealProgress = 0.f;
        FloatTime = 0.f;
        HologramRoot->SetRelativeLocation(RestHologramLocation);
        if (bAnimateEntranceMovement)
        {
            AnimalMesh->SetRelativeLocation(EntranceStartLocation);
        }
        AnimalMesh->bPauseAnims = true;
        HandleMouseReleased();
        SetInformationOpen(false);
        InformationPanel->SetVisibility(false);
        InformationPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        bPanelWasVisible = false;
        SetVisualsVisible(false);
        // A fast re-entry may interrupt the previous session's fade-out.
        UpdateAppearanceEvents();

        if (bGuidedArrivalSession)
        {
            UZooARCameraFocusComponent* Focus = UZooARCameraFocusComponent::GetOrCreate(LocalController.Get());
            if (Focus && Focus->StartDeckWalk(this))
            {
                ArrivalCameraFocus = Focus;
            }
            else
            {
                CancelDeckSequence();
            }
            return;
        }

        BeginARSession();
        // An animal already at its goal can synchronously open the presentation gate.
        if (!bWaitingForAnimal || !bZoneActive)
        {
            return;
        }
        const float Timeout = FMath::IsFinite(AnimalArrivalTimeout) ? FMath::Max(AnimalArrivalTimeout, 0.f) : 2.f;
        if (Timeout <= 0.f)
        {
            StartAppearance();
            return;
        }
        if (bEnableArrivalCameraFocus)
        {
            UZooARCameraFocusComponent* Focus = UZooARCameraFocusComponent::GetOrCreate(LocalController.Get());
            if (Focus && Focus->StartFocus(this, Timeout))
            {
                ArrivalCameraFocus = Focus;
            }
        }
    }

    void AZooARExhibit::UpdateAnimalArrivalWait(float DeltaSeconds)
    {
        if (!bZoneActive || !bWaitingForAnimal || bWalkingToDeckCenter)
        {
            return;
        }
        AnimalWaitElapsed += FMath::IsFinite(DeltaSeconds) ? FMath::Max(DeltaSeconds, 0.f) : 0.f;
        const float Timeout = FMath::IsFinite(AnimalArrivalTimeout) ? FMath::Max(AnimalArrivalTimeout, 0.f) : 2.f;
        const AZooAnimal* Animal = GetSelectedAnimal();
        if ((Animal && Animal->GetAnimalState() == EZooAnimalState::WaitingAtExhibit) || AnimalWaitElapsed >= Timeout)
        {
            // A short timeout must finish the turn and dispatch the approach before releasing the camera.
            if (bGuidedArrivalSession && !bAnimalApproachRequested)
            {
                if (UZooARCameraFocusComponent* Focus = ArrivalCameraFocus.Get())
                {
                    Focus->FinishAnimalTurn(this);
                }
                RequestAnimalApproach();
            }
            StartAppearance();
        }
    }

    void AZooARExhibit::NotifyDeckCenterReached()
    {
        if (!bZoneActive || !bWaitingForAnimal || !bWalkingToDeckCenter || !bGuidedArrivalSession)
        {
            return;
        }
        bWalkingToDeckCenter = false;
        AnimalWaitElapsed = 0.f;
        BeginARSession(true);
        if (!bEnableArrivalCameraFocus)
        {
            NotifyAnimalFocusFinished();
        }
        UpdateAnimalArrivalWait(0.f);
    }

    void AZooARExhibit::NotifyAnimalFocusFinished()
    {
        if (bZoneActive && bWaitingForAnimal && bGuidedArrivalSession && !bWalkingToDeckCenter)
        {
            RequestAnimalApproach();
        }
    }

    void AZooARExhibit::CancelDeckSequence()
    {
        if (bGuidedArrivalSession && bWaitingForAnimal)
        {
            UE_LOG(LogTemp, Warning, TEXT("AR %s cancelled its deck sequence. Check the ViewingZone box, walkable floor, stand-point offset, player control, and DeckWalkTimeout. Leave and re-enter the zone to retry."), *GetName());
            EndARSession();
        }
    }

    void AZooARExhibit::StopArrivalCameraFocus()
    {
        if (UZooARCameraFocusComponent* Focus = ArrivalCameraFocus.Get())
        {
            Focus->StopFocus(this);
        }
        ArrivalCameraFocus.Reset();
    }

    void AZooARExhibit::StartAppearance()
    {
        if (!bZoneActive || bPresentationStarted)
        {
            return;
        }
        bPresentationStarted = true;
        bWaitingForAnimal = false;
        StopArrivalCameraFocus();
        AcquireCursor();
        FloatTime = 0.f;
        if (ActivationMode == EZooARActivationMode::WaitForAnimal)
        {
            ReferencePresentation->BeginIn();
            Hologram->Appear(ReferencePresentation->GetInDuration());
            RevealProgress = 0.f;
        }
        else
        {
            SetVisualsVisible(true);
            Hologram->Appear(RevealDuration);
        }
        bAppearActive = true;
        OnExhibitAppearStarted.Broadcast(this);
    }

    void AZooARExhibit::UpdateInformationVisibility()
    {
        // A world card only appears while the visitor actually looks into this exhibit.
        const bool bCardVisible = CanInteractWithInformation();
        InformationPanel->SetVisibility(bCardVisible);
        InformationPanel->SetCollisionEnabled(bCardVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        const bool bPanelJustShown = bCardVisible && !bPanelWasVisible;
        bPanelWasVisible = bCardVisible;
        if (bPanelJustShown)
        {
            BeginARSession();
            OnARPanelShow.Broadcast(this);
        }
    }

    void AZooARExhibit::BeginARSession(bool bDeferApproach)
    {
        if (bARSessionStarted)
        {
            return;
        }

        // Reserve a single animal before broadcasting; every listener sees the same selection.
        bARSessionStarted = true;
        AZooAnimal* Animal = AZooAnimal::FindNearestForExhibit(this, MaxApproachDistance);
        if (Animal && Animal->BindToExhibit(this, bDeferApproach))
        {
            SelectedAnimal = Animal;
        }
        if (!bDeferApproach)
        {
            RequestAnimalApproach();
        }
    }

    void AZooARExhibit::RequestAnimalApproach()
    {
        if (!bARSessionStarted || bAnimalApproachRequested)
        {
            return;
        }
        bAnimalApproachRequested = true;
        OnAnimalApproachRequested.Broadcast(this);
    }

    void AZooARExhibit::EndARSession()
    {
        if (bPresentationStarted)
        {
            // No linger: the animal is gone when the hide ends, which is when the visitor's companion may return
            if (ActivationMode == EZooARActivationMode::WaitForAnimal)
            {
                ReferencePresentation->BeginOut();
                Hologram->Disappear(ReferencePresentation->GetOutDuration(), false);
            }
            else
            {
                Hologram->Disappear(HideDuration, false);
            }
        }
        StopArrivalCameraFocus();
        bWaitingForAnimal = false;
        bWalkingToDeckCenter = false;
        bGuidedArrivalSession = false;
        bAnimalApproachRequested = false;
        bPresentationStarted = false;
        AnimalWaitElapsed = 0.f;
        bPanelWasVisible = false;
        if (!bARSessionStarted)
        {
            return;
        }

        bARSessionStarted = false;
        bPanelWasVisible = false;
        SelectedAnimal.Reset();
        OnARInteractionEnded.Broadcast(this);
    }

    void AZooARExhibit::AcquireCursor()
    {
        APlayerController* PC = LocalController.Get();
        if (!PC || bOwnsMouseCursor)
        {
            return;
        }

        bOwnsMouseCursor = true;
        ++GZooARCursorUsers;
        PC->bShowMouseCursor = true;
    }

    void AZooARExhibit::ReleaseCursor()
    {
        if (!bOwnsMouseCursor)
        {
            return;
        }

        bOwnsMouseCursor = false;
        if (--GZooARCursorUsers <= 0)
        {
            GZooARCursorUsers = 0;
            if (APlayerController* PC = LocalController.Get())
            {
                PC->bShowMouseCursor = false;
            }
        }
    }

    void AZooARExhibit::UpdateAnimation(float DeltaSeconds)
    {
        if (ActivationMode == EZooARActivationMode::WaitForAnimal)
        {
            ReferencePresentation->Advance(DeltaSeconds);
            RevealProgress = ReferencePresentation->GetProgress();
            const bool bAnimalClickable = CanInteractWithAnimal();
            AnimalClickBounds->SetCollisionEnabled(bAnimalClickable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
            UpdateInformationPanel(0.f);
            UpdateInformationVisibility();
            UpdateAppearanceEvents();
            return;
        }
        const bool bShowing = bZoneActive && bPresentationStarted;
        RevealProgress = FMath::Clamp(RevealProgress + DeltaSeconds * (bShowing ? 1.f / RevealDuration : -1.f / HideDuration), 0.f, 1.f);

        const float SmoothProgress = RevealProgress * RevealProgress * (3.f - 2.f * RevealProgress);
        FloatTime += DeltaSeconds;
        const float BobOffset = FMath::Sin(FloatTime * 2.f * PI * FloatFrequency) * FloatAmplitude * SmoothProgress;
        HologramRoot->SetRelativeLocation(RestHologramLocation + FVector(0., 0., BobOffset));

        UpdateEntranceMovement(DeltaSeconds);

        const float AnimStartProgress = bAnimateEntranceMovement ? MovingAnimationStartProgress : StationaryAnimationStartProgress;
        AnimalMesh->bPauseAnims = RevealProgress < AnimStartProgress;

        if (RevealProgress <= 0.f)
        {
            SetVisualsVisible(false);
        }

        const bool bAnimalClickable = CanInteractWithAnimal();
        AnimalClickBounds->SetCollisionEnabled(bAnimalClickable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        UpdateInformationPanel(BobOffset);
        UpdateInformationVisibility();
        UpdateAppearanceEvents();
    }

    void AZooARExhibit::UpdateAppearanceEvents()
    {
        if (bZoneActive && bPresentationStarted && RevealProgress >= 1.f && IsEntranceMovementComplete() && !bAppearCompleted)
        {
            bAppearCompleted = true;
            OnExhibitAppearCompleted.Broadcast(this);
        }
        else if (RevealProgress < 1.f && bAppearCompleted)
        {
            bAppearCompleted = false;
        }

        if (RevealProgress <= 0.f && bAppearActive
            && (ActivationMode != EZooARActivationMode::WaitForAnimal
                || ReferencePresentation->GetVisualPhase() == EZooARVisualPhase::Hidden))
        {
            bAppearActive = false;
            OnExhibitHidden.Broadcast(this);
        }
    }

    void AZooARExhibit::UpdateEntranceMovement(float DeltaSeconds)
    {
        if (ActivationMode == EZooARActivationMode::WaitForAnimal
            && ReferencePresentation->GetVisualPhase() != EZooARVisualPhase::Moving)
        {
            return;
        }
        if (!bAnimateEntranceMovement)
        {
            return;
        }

        if (bZoneActive && !bPresentationStarted)
        {
            AnimalMesh->SetRelativeLocation(EntranceStartLocation);
            return;
        }

        // Reset only after the fade-out, so a short hide does not accelerate visible return movement.
        if (!bZoneActive && RevealProgress <= 0.f)
        {
            AnimalMesh->SetRelativeLocation(EntranceStartLocation);
            return;
        }

        if (DeltaSeconds <= 0.f)
        {
            return;
        }

        const float Speed = FMath::IsFinite(EntranceMoveSpeed)
            ? FMath::Max(EntranceMoveSpeed, MinimumEntranceMoveSpeed)
            : MinimumEntranceMoveSpeed;
        const FVector Destination = bZoneActive ? EntranceTargetLocation : EntranceStartLocation;

        // Interpolate in world space so parent scale does not change the configured speed.
        const FTransform& ParentTransform = HologramRoot->GetComponentTransform();
        const FVector CurrentLocation = ParentTransform.TransformPosition(AnimalMesh->GetRelativeLocation());
        const FVector TargetLocation = ParentTransform.TransformPosition(Destination);
        const FVector NextLocation = FMath::VInterpConstantTo(CurrentLocation, TargetLocation, DeltaSeconds, Speed);
        AnimalMesh->SetRelativeLocation(ParentTransform.InverseTransformPosition(NextLocation));
    }

    void AZooARExhibit::UpdateInformationPanel(float BobOffset)
    {
        if (LocalController.IsValid())
        {
            FVector Camera;
            FRotator Rotation;
            LocalController->GetPlayerViewPoint(Camera, Rotation);
            InformationPanel->SetWorldLocation(RestPanelLocation + FVector(0., 0., BobOffset * PanelBobScale));
            InformationPanel->SetWorldRotation((Camera - InformationPanel->GetComponentLocation()).Rotation());
        }

        if (UZooARInfoWidget* Widget = GetInformationWidget())
        {
            Widget->SetRenderOpacity(1.f);
        }
    }

    void AZooARExhibit::SetVisualsVisible(bool bVisible)
    {
        HologramRoot->SetVisibility(bVisible, true);
        AnimalClickBounds->SetCollisionEnabled(bVisible && CanInteractWithAnimal() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }

    bool AZooARExhibit::GetMouseRay(FVector& Start, FVector& Direction) const
    {
        APlayerController* PC = LocalController.Get();
        if (!PC)
        {
            return false;
        }

        // VR: point with the right-hand laser instead of the view / mouse
        if (UZooVRComponent::GetPointerRay(PC, Start, Direction))
        {
            return true;
        }

        FRotator Rotation;
        PC->GetPlayerViewPoint(Start, Rotation);
        Direction = Rotation.Vector();
        if (PC->bShowMouseCursor)
        {
            float X, Y;
            if (PC->GetMousePosition(X, Y))
            {
                PC->DeprojectScreenPositionToWorld(X, Y, Start, Direction);
            }
        }

        return true;
    }

    void AZooARExhibit::UpdateMouseTrace()
    {
        MouseHit = FHitResult();
        FVector Start, Direction;
        if (CanInteract() && GetMouseRay(Start, Direction))
        {
            FCollisionQueryParams Params(SCENE_QUERY_STAT(ZooARMouse), false);
            // Navigation barriers are invisible and explicitly excluded from AR pointing.
            TArray<AActor*> ARTransparentBarriers;
            UGameplayStatics::GetAllActorsWithTag(this, IgnoreTraceTag, ARTransparentBarriers);
            Params.AddIgnoredActors(ARTransparentBarriers);
            if (APawn* Pawn = LocalController->GetPawn())
            {
                Params.AddIgnoredActor(Pawn);
            }

            GetWorld()->LineTraceSingleByChannel(MouseHit, Start, Start + Direction * MouseTraceDistance, ECC_Visibility, Params);
        }

        MouseInteraction->SetCustomHitResult(MouseHit);
    }

    void AZooARExhibit::HandleMouseScrolled(float Value)
    {
        if (CanInteractWithInformation() && Value != 0.f)
        {
            MouseInteraction->ScrollWheel(Value);
        }
    }

    bool AZooARExhibit::IsMouseOverAnimalBounds() const
    {
        FVector Start, Direction;
        if (!GetMouseRay(Start, Direction))
        {
            return false;
        }

        FHitResult AnimalHit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(ZooARModelClick), false);
        return AnimalClickBounds->LineTraceComponent(AnimalHit, Start, Start + Direction * MouseTraceDistance, Params);
    }

    void AZooARExhibit::SetInformationOpen(bool bOpen)
    {
        const bool bNewValue = bOpen && CanInteractWithInformation();
        if (bInformationOpen == bNewValue)
        {
            return;
        }

        bInformationOpen = bNewValue;

        if (UZooARInfoWidget* Widget = GetInformationWidget())
        {
            Widget->RefreshInformation();
        }
    }

    void AZooARExhibit::HandleMousePressed()
    {
        if (!CanInteract())
        {
            return;
        }

        UpdateMouseTrace();
        if (MouseHit.GetActor() != this)
        {
            return;
        }

        if (MouseHit.GetComponent() == InformationPanel && CanInteractWithInformation())
        {
            if (MouseInteraction->IsOverHitTestVisibleWidget())
            {
                MouseInteraction->PressPointerKey(EKeys::LeftMouseButton);
                bPointerPressed = true;
            }
            else if (CanInteractWithAnimal() && IsMouseOverAnimalBounds())
            {
                // The transparent gap between the four panels lets the hologram receive clicks.
                SetInformationOpen(true);
            }
        }
        else if (CanInteractWithAnimal() && MouseHit.GetComponent() == AnimalClickBounds)
        {
            SetInformationOpen(true);
        }
    }

    void AZooARExhibit::HandleMouseReleased()
    {
        if (bPointerPressed)
        {
            MouseInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
        }

        bPointerPressed = false;
    }
