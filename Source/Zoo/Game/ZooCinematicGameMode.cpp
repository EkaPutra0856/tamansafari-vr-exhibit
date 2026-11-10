// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/ZooCinematicGameMode.h"

#include "AR/ZooARExhibit.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetComponent.h"
#include "Duma/ZooDuma.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Hologram/ZooHologramComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"
#include "LevelSequenceActor.h"
#include "PathGuide/Components/PathGuideComponent.h"
#include "WzNarrativeSettings.h"

static const FString GuideTagPrefix{ TEXT("CineGuide=") };
static const FString PanelExhibitTagPrefix{ TEXT("CinePanelExhibit=") };

// ====== Lifecycle ====== //

AZooCinematicGameMode::AZooCinematicGameMode()
{
	// Hidden flying visitor: AR viewing zones need a pawn, the sequence camera is the view
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	HUDClass = AHUD::StaticClass();

	// After the sequence moved the marker, before the AR exhibits (TG_PostUpdateWork) test their zones
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void AZooCinematicGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// No narrative in a render: no companion Duma, dialogue or input locks (restored in EndPlay, never saved)
	UWzNarrativeSettings* Narrative = GetMutableDefault<UWzNarrativeSettings>();
	SavedDirector = Narrative->Director;
	Narrative->Director.Reset();
	bNarrativeDisabled = true;

	Super::InitGame(MapName, Options, ErrorMessage);

	// Placed sequences must not fight the render sequence
	for (TActorIterator<ALevelSequenceActor> It{ GetWorld() }; It; ++It)
	{
		if (!It->PlaybackSettings.bAutoPlay) continue;
		It->PlaybackSettings.bAutoPlay = false;
		UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Auto play disabled on %s"), *It->GetName());
	}
}

void AZooCinematicGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController) return;

	// Hidden visitor, no HUD; movement and look stay free (an AR exhibit ignores a visitor whose input is locked)
	PlayerController->SetCinematicMode(true, true, true, false, false);

	// The visitor marker and its guide destinations
	TArray<AActor*> Markers;
	UGameplayStatics::GetAllActorsWithTag(this, VisitorMarkerTag, Markers);
	if (Markers.Num() > 0) VisitorMarker = Markers[0];
	if (const AActor* Marker = VisitorMarker.Get())
	{
		for (const auto& Tag : Marker->Tags)
		{
			const FString Text = Tag.ToString();
			if (!Text.StartsWith(GuideTagPrefix)) continue;
			const FGameplayTag Place = FGameplayTag::RequestGameplayTag(FName{ *Text.RightChop(GuideTagPrefix.Len()) }, false);
			if (Place.IsValid()) GuidePlaces.Add(Place);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Visitor marker %s, %d guide places"), *GetNameSafe(VisitorMarker.Get()), GuidePlaces.Num());

	// The card demonstration marker and the exhibits it names
	Markers.Reset();
	UGameplayStatics::GetAllActorsWithTag(this, PanelMarkerTag, Markers);
	if (Markers.Num() > 0) PanelMarker = Markers[0];
	if (const AActor* Marker = PanelMarker.Get())
	{
		for (const auto& Tag : Marker->Tags)
		{
			const FString Text = Tag.ToString();
			if (Text.StartsWith(PanelExhibitTagPrefix)) PanelExhibitLabels.Add(Text.RightChop(PanelExhibitTagPrefix.Len()));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Panel marker %s, %d exhibits"), *GetNameSafe(PanelMarker.Get()), PanelExhibitLabels.Num());

	// The 3D route of the Path Guide (normally a component of the Zoo controller)
	if (GuidePlaces.Num() > 0)
	{
		PathGuide = NewObject<UWzPathGuideComponent>(PlayerController, TEXT("CinePathGuide"));
		PathGuide->RegisterComponent();
		PathGuide->OnArrived.AddUObject(this, &AZooCinematicGameMode::HandleGuideArrived);
	}
}

void AZooCinematicGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Back to the project's narrative
	if (bNarrativeDisabled) GetMutableDefault<UWzNarrativeSettings>()->Director = SavedDirector;
	bNarrativeDisabled = false;

	Super::EndPlay(EndPlayReason);
}

void AZooCinematicGameMode::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FollowVisitorMarker();
	OpenInformationCards(DeltaSeconds);
	DemonstrateCards();
	ShowSequenceDumas();
	UpdateGuidance();
}

// ====== Visitor ====== //

void AZooCinematicGameMode::FollowVisitorMarker()
{
	const AActor* Marker = VisitorMarker.Get();
	const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Visitor = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Marker || !Visitor) return;

	// Only a marker move teleports (the sequence keys are steps)
	const FVector Location = Marker->GetActorLocation();
	if (Location.Equals(LastMarkerLocation, 1.f)) return;
	LastMarkerLocation = Location;
	Visitor->TeleportTo(Location, Marker->GetActorRotation(), false, true);
	UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Visitor at %s (t %.2f)"), *Location.ToCompactString(), GetWorld()->GetTimeSeconds());
}

void AZooCinematicGameMode::OpenInformationCards(const float DeltaSeconds)
{
	for (TActorIterator<AZooARExhibit> It{ GetWorld() }; It; ++It)
	{
		AZooARExhibit* Exhibit = *It;
		float& Shown = CardShownSeconds.FindOrAdd(Exhibit);

		// The card is up only while the view looks into the habitat; it closes by itself when the view leaves
		if (!Exhibit->InformationPanel->IsVisible())
		{
			Shown = 0.f;
			continue;
		}

		// The visitor's click, a moment after the card appeared
		Shown += DeltaSeconds;
		if (Shown >= InformationOpenDelay && !Exhibit->IsInformationOpen()) Exhibit->SetInformationOpen(true);
	}
}

void AZooCinematicGameMode::DemonstrateCards()
{
	const AActor* Marker = PanelMarker.Get();
	if (!Marker) return;

	// Exhibit and card from the marker (stepped keys), scroll from its height (linear keys)
	const FVector Location = Marker->GetActorLocation();
	const int32 ExhibitIndex = FMath::RoundToInt(Location.X) - 1;
	const int32 CardIndex = FMath::RoundToInt(Location.Y) - 1;
	const int32 Key = (ExhibitIndex + 1) * 100 + CardIndex + 1;
	if (Key != CardKey)
	{
		AZooARExhibit* Exhibit = nullptr;
		if (PanelExhibitLabels.IsValidIndex(ExhibitIndex))
		{
			for (TActorIterator<AZooARExhibit> It{ GetWorld() }; It && !Exhibit; ++It)
			{
				if (It->GetActorNameOrLabel() == PanelExhibitLabels[ExhibitIndex]) Exhibit = *It;
			}
		}
		UUserWidget* Card = Exhibit && Exhibit->IsInformationOpen() ? FindCard(Exhibit, CardIndex) : nullptr;

		// Wait for the panel to be up before taking the step (the card buttons exist once it is open)
		if (CardIndex >= 0 && !Card)
		{
			if (WaitingCardKey != Key)
			{
				WaitingCardKey = Key;
				const UUserWidget* Panel = Exhibit ? Exhibit->InformationPanel->GetUserWidgetObject() : nullptr;
				FString Widgets;
				for (TObjectIterator<UUserWidget> It; It; ++It)
				{
					if (It->GetWorld() == GetWorld() && It->GetClass()->GetName().StartsWith(TEXT("WBP_AR_"))) Widgets += It->GetPathName() + TEXT(" ");
				}
				UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Card %d of %s waits: information open %d, panel %s, cards [%s] (t %.2f)"), CardIndex + 1, *GetNameSafe(Exhibit),
					Exhibit && Exhibit->IsInformationOpen(), *GetNameSafe(Panel), *Widgets, GetWorld()->GetTimeSeconds());
			}
			return;
		}

		// Fold the previous card, then unfold the new one
		if (UUserWidget* Previous = OpenCard.Get()) ClickCardHeader(Previous);
		OpenCard = Card;
		CardExhibit = Exhibit;
		CardKey = Key;
		if (Card) ClickCardHeader(Card);
		CardOpenSeconds = 0.f;
		bCardPlacementLogged = false;
		UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Card %d of %s open (t %.2f)"), CardIndex + 1, *GetNameSafe(Exhibit), GetWorld()->GetTimeSeconds());
	}

	// Read the open card from top to bottom
	UUserWidget* Card = OpenCard.Get();
	if (!Card || !Card->WidgetTree) return;
	LogCardPlacement(Card);
	UScrollBox* Scroll = nullptr;
	Card->WidgetTree->ForEachWidget([&Scroll](UWidget* Child)
	{
		if (!Scroll) Scroll = Cast<UScrollBox>(Child);
	});
	if (Scroll) Scroll->SetScrollOffset(FMath::Clamp(Location.Z / 100.f, 0.f, 1.f) * Scroll->GetScrollOffsetOfEnd());
}

UUserWidget* AZooCinematicGameMode::FindCard(const AZooARExhibit* Exhibit, const int32 CardIndex) const
{
	if (!CardWidgetNames.IsValidIndex(CardIndex)) return nullptr;
	const UUserWidget* Panel = Exhibit->InformationPanel->GetUserWidgetObject();
	if (!Panel) return nullptr;

	// The card widgets live in the panel's own widget tree (the tree walk does not enter child user widgets, so match by outer)
	const FString& Name = CardWidgetNames[CardIndex];
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		const FString ClassName = It->GetClass()->GetName();
		if (ClassName.StartsWith(Name) && !ClassName.Contains(TEXT("Extend")) && It->IsIn(Panel)) return *It;
	}

	return nullptr;
}

void AZooCinematicGameMode::LogCardPlacement(const UUserWidget* Card)
{
	// Once per card, after its unfold settled: where it is in the world (for framing the shots)
	CardOpenSeconds += GetWorld()->GetDeltaSeconds();
	if (bCardPlacementLogged || CardOpenSeconds < 2.f || !CardExhibit.IsValid()) return;
	bCardPlacementLogged = true;

	const UWidgetComponent* Panel = CardExhibit->InformationPanel;
	const UUserWidget* Top = Panel->GetUserWidgetObject();
	const FGeometry& TopGeometry = Top->GetCachedGeometry();
	const FGeometry& Geometry = Card->GetCachedGeometry();
	const FVector2D TopSize = TopGeometry.GetLocalSize();
	if (TopSize.IsNearlyZero()) return;
	const FVector2D Draw = Panel->GetCurrentDrawSize();
	const FVector2D Pivot = Panel->GetPivot();
	auto World = [&](const FVector2D Absolute)
	{
		const FVector2D Local = TopGeometry.AbsoluteToLocal(Absolute) * FVector2D{ Draw.X / TopSize.X, Draw.Y / TopSize.Y };
		return Panel->GetComponentTransform().TransformPosition(FVector{ 0.0, -(Local.X - Draw.X * Pivot.X), -(Local.Y - Draw.Y * Pivot.Y) });
	};
	const FVector Min = World(Geometry.GetAbsolutePosition());
	const FVector Max = World(Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize());
	UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Card placement %s: centre %s, corner %s, corner %s, panel %s facing %s, draw %s"), *Card->GetClass()->GetName(),
		*((Min + Max) * 0.5).ToCompactString(), *Min.ToCompactString(), *Max.ToCompactString(), *Panel->GetComponentLocation().ToCompactString(),
		*Panel->GetForwardVector().ToCompactString(), *Draw.ToString());
}

void AZooCinematicGameMode::ClickCardHeader(UUserWidget* Card)
{
	if (!Card->WidgetTree) return;

	// The header button toggles the card (its Blueprint folds the other cards)
	UButton* Header = Cast<UButton>(Card->WidgetTree->FindWidget(TEXT("HeaderButton")));
	if (!Header)
	{
		Card->WidgetTree->ForEachWidget([&Header](UWidget* Child)
		{
			if (!Header) Header = Cast<UButton>(Child);
		});
	}
	if (Header) Header->OnClicked.Broadcast();
}

void AZooCinematicGameMode::ShowSequenceDumas()
{
	for (TActorIterator<AZooDuma> It{ GetWorld() }; It; ++It)
	{
		AZooDuma* Duma = *It;
		if (Duma->GetController() || ShownDumas.Contains(Duma)) continue;

		// A spawnable of the sequence: materialize once
		ShownDumas.Add(Duma);
		Duma->GetHologram()->Appear(DumaAppearSeconds);
		UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Duma %s appears (t %.2f)"), *Duma->GetName(), GetWorld()->GetTimeSeconds());
	}
}

void AZooCinematicGameMode::UpdateGuidance()
{
	if (!PathGuide) return;

	// First destination at once, the next one after each arrival
	const bool bStartNext = GuideIndex == INDEX_NONE || bGuideArrived;
	if (!bStartNext || GuideIndex + 1 >= GuidePlaces.Num()) return;
	bGuideArrived = false;
	++GuideIndex;
	const bool bStarted = PathGuide->StartGuidance(GuidePlaces[GuideIndex]);
	UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Guidance to %s: %s"), *GuidePlaces[GuideIndex].ToString(), bStarted ? TEXT("route") : TEXT("no route"));
}

void AZooCinematicGameMode::HandleGuideArrived(const FGameplayTag& PlaceTag)
{
	bGuideArrived = true;
	UE_LOG(LogTemp, Log, TEXT("[ZooCinematic] Guidance arrived at %s"), *PlaceTag.ToString());
}
