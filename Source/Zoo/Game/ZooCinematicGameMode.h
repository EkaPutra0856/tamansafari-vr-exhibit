// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "ZooCinematicGameMode.generated.h"

class AZooARExhibit;
class AZooDuma;
class UWzNarrativeDirector;
class UUserWidget;
class UWzPathGuideComponent;

/** Movie Render Queue game mode (L_Zoo_Cine presets only): a hidden visitor follows a sequence marker so the AR exhibits play for the cinematic camera; no Zoo HUD, VR rig, narrative or input. */
UCLASS(Blueprintable)
class AZooCinematicGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AZooCinematicGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	// ====== Visitor ====== //

	/** Actor tag of the level marker the sequence moves; the visitor pawn teleports to it whenever it moves. Its "CineGuide=<PlaceTag>" tags are Path Guide destinations, in order. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Cinematic")
	FName VisitorMarkerTag = TEXT("CineVisitor");

	/** Seconds an AR card stays shown before its information panels open (the visitor's click). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Cinematic", meta = (ClampMin = "0", Units = "s"))
	float InformationOpenDelay = 1.f;

	/** Actor tag of the level marker whose location the sequence keys to demonstrate the AR cards: X = exhibit (1-based, its "CinePanelExhibit=<label>" tags in order), Y = card (1-based, CardWidgetNames order, 0 = all folded), Z = scroll of the open card in percent. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Cinematic")
	FName PanelMarkerTag = TEXT("CinePanels");

	/** Widget class name prefixes of the cards inside an AR information panel, in marker order. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Cinematic")
	TArray<FString> CardWidgetNames = { TEXT("WBP_AR_Media"), TEXT("WBP_AR_Characteristic"), TEXT("WBP_AR_GeneralDescription"), TEXT("WBP_AR_Habitat") };

	/** Seconds a sequence-spawned Duma takes to materialize. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|Cinematic", meta = (ClampMin = "0", Units = "s"))
	float DumaAppearSeconds = 1.2f;

private:
	/** Teleports the visitor onto the marker when the marker moved (AR viewing zones test the pawn). */
	void FollowVisitorMarker();

	/** Opens each AR card's panels once it has been shown for InformationOpenDelay. */
	void OpenInformationCards(float DeltaSeconds);

	/** Unfolds the card the panel marker names (the visitor's click on its header) and scrolls it. */
	void DemonstrateCards();

	/** The card widget of an exhibit's information panel, by CardWidgetNames index. */
	UUserWidget* FindCard(const AZooARExhibit* Exhibit, int32 CardIndex) const;

	/** Logs the world box of the open card once it settled (framing data for the card shots). */
	void LogCardPlacement(const UUserWidget* Card);

	/** Clicks a card's header button (folds or unfolds it). */
	static void ClickCardHeader(UUserWidget* Card);

	/** Materializes Dumas the sequence spawned (no controller, so nobody else calls Appear). */
	void ShowSequenceDumas();

	/** Starts the next Path Guide destination once the previous one is reached. */
	void UpdateGuidance();

	/** The guided destination was reached. */
	void HandleGuideArrived(const FGameplayTag& PlaceTag);

	TWeakObjectPtr<AActor> VisitorMarker;
	FVector LastMarkerLocation = FVector{ 1.e9 };
	TMap<TWeakObjectPtr<AZooARExhibit>, float> CardShownSeconds;
	TSet<TWeakObjectPtr<AZooDuma>> ShownDumas;

	TWeakObjectPtr<AActor> PanelMarker;
	TArray<FString> PanelExhibitLabels;
	TWeakObjectPtr<AZooARExhibit> CardExhibit;
	TWeakObjectPtr<UUserWidget> OpenCard;
	int32 CardKey = 0;
	int32 WaitingCardKey = 0;
	float CardOpenSeconds = 0.f;
	bool bCardPlacementLogged = false;

	UPROPERTY(Transient)
	TObjectPtr<UWzPathGuideComponent> PathGuide;

	TArray<FGameplayTag> GuidePlaces;
	int32 GuideIndex = INDEX_NONE;
	bool bGuideArrived = false;

	/** The project's narrative director, restored at the end (only this render process runs without it). */
	TSoftObjectPtr<UWzNarrativeDirector> SavedDirector;
	bool bNarrativeDisabled = false;
};
