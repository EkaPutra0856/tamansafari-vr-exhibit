// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZooVRComponent.generated.h"

class APawn;
class APlayerController;
class UButton;
class UCameraComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UMotionControllerComponent;
class UUserWidget;
class UWidget;
class UWidgetComponent;
class UWidgetInteractionComponent;
class UXRDeviceVisualizationComponent;
class UZooVRDemoDirector;
class UZooVRPanelHost;
struct FInputActionValue;

/**
 * Everything VR, added to the local PlayerController by AZooVRGameMode, while the Zoo's own pawn, controller and HUD
 * Blueprints stay untouched (other Blueprints cast to them). Does nothing without a headset, so desktop play is unchanged.
 *  - Rig: puts the pawn's camera on the headset (tracking origin at the capsule's feet), shows both controllers, adds a
 *    right-hand laser that clicks world widgets.
 *  - Input: head-relative walk, snap turn, trigger click (laser on a widget, otherwise a left click for the gaze-pointed
 *    AR exhibits). The Zoo's own actions (sprint, dialogue skip, destination menu) are mapped in the VR context.
 *  - Panels: screen widgets (Path Guide, narrative) move onto a world panel that lazily follows the head.
 *  - Performance: console variables set only while the headset renders, restored on end play.
 */
UCLASS(Blueprintable, ClassGroup = (Zoo), meta = (BlueprintSpawnableComponent))
class ZOO_API UZooVRComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UZooVRComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ====== Input ====== //

protected:
	/** VR-only actions plus the Zoo's own actions on controller buttons. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input")
	TObjectPtr<UInputMappingContext> VRMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input")
	int32 VRMappingPriority = 2;

	/** Left thumbstick (Axis2D): walk where the head looks. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Right thumbstick X (Axis1D): snap turn. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input")
	TObjectPtr<UInputAction> SnapTurnAction;

	/**
	 * Right trigger. Tap: click what the laser points at (a VR panel directly, otherwise a left click that the AR
	 * exhibits trace along the laser). Hold and sweep the laser up / down: scroll.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input")
	TObjectPtr<UInputAction> SelectAction;

	/** Right A. Tap: open / close the destination list (Navigasi). Hold and sweep the laser up / down: scroll. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input")
	TObjectPtr<UInputAction> MenuAction;

	/** Right thumbstick Y (Axis1D): scrolls the widget under the laser too. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input")
	TObjectPtr<UInputAction> ScrollAction;

	/** Wheel notches per second at full stick. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input", meta = (ClampMin = "0.1"))
	float ScrollSpeed = 8.f;

	/** Laser sweep per wheel notch while scrolling by hold. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input", meta = (ClampMin = "0.1", Units = "deg"))
	float DegreesPerNotch = 2.f;

	/** A trigger hold becomes a scroll (no click) once the laser moved this far. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input", meta = (ClampMin = "0.1", Units = "deg"))
	float DragThreshold = 3.f;

	/** An A press longer than this is a scroll hold, not a menu tap. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input", meta = (ClampMin = "0.05", Units = "s"))
	float HoldTime = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input", meta = (ClampMin = "5", ClampMax = "90", Units = "deg"))
	float SnapTurnAngle = 45.f;

	/** Stick deflection that fires one snap turn; it re-arms when the stick returns to the centre. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Input", meta = (ClampMin = "0.1", ClampMax = "1"))
	float SnapTurnThreshold = 0.6f;

private:
	void BindInput();
	void OnMove(const FInputActionValue& Value);
	void OnSnapTurn(const FInputActionValue& Value);
	void OnSelectStarted();
	void OnSelectCompleted();
	void OnMenuStarted();
	void OnMenuCompleted();
	void OnScroll(const FInputActionValue& Value);
	/** Hold-to-scroll: wheel notches from the laser's pitch change. */
	void UpdateHoldScroll(float DeltaTime);
	/** Click what the laser points at (press and release). */
	void Click();
	/** A keyboard key, pressed and released, through the controller (the desktop's own keys, e.g. P / X of the extended video). */
	void SimulateKey(const FKey& Key) const;
	/** Wheel notches on what the laser points at. */
	void Scroll(float Notches);
	/**
	 * Aims the laser (custom hit) at the nearest VR panel with a button or scroll box under it. The panels are stacked
	 * full-screen layouts, so the plain first-hit trace would stop on a transparent area of the front one.
	 */
	void UpdatePointerTarget();
	/** True when a widget is under the laser at the given point of a panel that takes clicks or scrolls. */
	static bool HasInteractiveWidgetAt(UWidgetComponent* Panel, const FVector& WorldLocation);
	void SimulateLeftClick(EInputEvent Event) const;

public:
	/** True when the laser is over a widget of a VR panel (not an AR exhibit's, which handles its own clicks). */
	bool IsPointingAtVRPanel() const;

	FORCEINLINE UInputMappingContext* GetVRMappingContext() const { return VRMappingContext; }
	FORCEINLINE UInputAction* GetMoveAction() const { return MoveAction; }
	FORCEINLINE UInputAction* GetSnapTurnAction() const { return SnapTurnAction; }
	FORCEINLINE UInputAction* GetSelectAction() const { return SelectAction; }
	FORCEINLINE UInputAction* GetMenuAction() const { return MenuAction; }
	FORCEINLINE UInputAction* GetScrollAction() const { return ScrollAction; }

private:
	float GetLaserPitch() const;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> VRInput;

	bool bSnapTurnArmed = true;
	bool bTriggerHeld = false;
	bool bMenuHeld = false;
	bool bScrolling = false;
	float HeldTime = 0.f;
	float StartPitch = 0.f;
	float LastPitch = 0.f;
	float ScrollRemainder = 0.f;

	// ====== Rig ====== //

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Rig", meta = (ClampMin = "10", Units = "cm"))
	float PointerDistance = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Rig")
	FLinearColor LaserColor { 0.35f, 0.75f, 1.f };

	/** The laser shows only while it has something to touch (a widget, an AR panel or hologram, an open list), and this long after. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Rig", meta = (ClampMin = "0", Units = "s"))
	float LaserLingerTime = 0.6f;

private:
	void HandleNewPawn(APawn* Pawn);
	void BuildRig(APawn* Pawn);

	/** Laser on demand: over a VR widget, aimed at an AR panel / hologram, or while a list is open. */
	void UpdateLaserVisibility(bool bOverVRWidget, const FVector& Start, const FVector& End);

	float LaserLinger = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> VROrigin;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetInteractionComponent> Pointer;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> HeadCamera;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> LeftGrip;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> RightGrip;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> RightAim;

public:
	/** The laser's ray for the controller's player, when VR is active; mouse-driven systems (AR exhibits) point with it. */
	static bool GetPointerRay(const APlayerController* Controller, FVector& OutStart, FVector& OutDirection);

	FORCEINLINE USceneComponent* GetVROrigin() const { return VROrigin; }
	FORCEINLINE UWidgetInteractionComponent* GetPointer() const { return Pointer; }
	FORCEINLINE UMotionControllerComponent* GetLeftGrip() const { return LeftGrip; }
	FORCEINLINE UMotionControllerComponent* GetRightGrip() const { return RightGrip; }
	FORCEINLINE UMotionControllerComponent* GetRightAim() const { return RightAim; }

	// ====== Panels ====== //

protected:
	/** Screen widgets of these classes leave the screen and go onto a VR panel (default: every user widget, e.g. Path Guide, narrative, extended media). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel")
	TArray<TSubclassOf<UUserWidget>> RelayWidgetClasses;

	/** Widgets inside the relayed widgets to collapse by name (screen-projected markers, e.g. WaypointPin). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel")
	TArray<FName> HiddenWidgetNames;

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel", meta = (ClampMin = "0.05", Units = "s"))
	float RelayInterval = 0.25f;

	/** Widget resolution of the panel (the screen layout is drawn at this size). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel")
	FIntPoint PanelDrawSize { 1920, 1080 };

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel", meta = (ClampMin = "30", Units = "cm"))
	float PanelDistance = 120.f;

	/** World width of the panel; bigger = bigger text. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel", meta = (ClampMin = "30", Units = "cm"))
	float PanelWidth = 170.f;

	/** Height of the panel centre relative to the eyes. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel", meta = (Units = "cm"))
	float PanelHeightOffset = -5.f;

	/** The panel re-centres when the head turns further than this from it ... */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel", meta = (ClampMin = "0", ClampMax = "90", Units = "deg"))
	float FollowAngle = 35.f;

	/** ... or when the head moves further than this from where the panel settled. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel", meta = (ClampMin = "0", Units = "cm"))
	float FollowDistance = 40.f;

	/** How fast the panel glides back in front (interp speed). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel", meta = (ClampMin = "0.1"))
	float FollowSpeed = 3.f;

	/** Widget material with depth test off, so near geometry never cuts the panel (M_ARWidget_OnTop). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel")
	TSoftObjectPtr<UMaterialInterface> PanelMaterial;

	/** The extended video widget (class name contains this): it has no buttons, so a laser click plays it, the next closes it. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel")
	FString MediaExtendWidgetName = TEXT("MediaExtend");

	/** Seconds the extended video stays on the screen before it moves to a panel (its own opening step must finish on the screen). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Panel", meta = (ClampMin = "0", Units = "s"))
	float MediaExtendRelayDelay = 1.8f;

private:
	/** When a screen widget was first seen in the viewport (for delayed relays). */
	TMap<TWeakObjectPtr<UUserWidget>, double> FirstSeenInViewport;

protected:

private:
	/** True for a panel hosting the extended video. */
	bool IsMediaExtendPanel(const UWidgetComponent* Panel) const;

	/** The extended video got its play click; the next click closes it. */
	bool bMediaExtendPlayed = false;

protected:

private:
	void RelayScreenWidgets();
	void HideScreenOnlyWidgets(UUserWidget* Widget) const;
	void UpdatePanels(float DeltaTime);
	bool GetHeadView(FVector& OutLocation, FRotator& OutYaw) const;

	/** The HUD elements on the panels, in view angles (FZooViewClutter source while VR is active). */
	void QueryClutter(const APlayerController* Controller, TArray<struct FZooViewRect>& OutRects) const;

	/** Panels live on their own actor: the laser ignores its owner's components, and the AR exhibits' trace ignores this actor (ARIgnoreTrace tag). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> PanelActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> Panels;

	/** One host per panel (same index); empty once the game removed (closed) its widget. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UZooVRPanelHost>> Hosts;

public:
	FORCEINLINE const TArray<TObjectPtr<UWidgetComponent>>& GetPanels() const { return Panels; }
	FORCEINLINE const TArray<TObjectPtr<UZooVRPanelHost>>& GetHosts() const { return Hosts; }

private:
	FTimerHandle RelayTimer;
	FVector AnchorLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;
	bool bHasTarget = false;
	bool bFollowing = false;

	// ====== Hover ====== //

protected:
	/** A button under the laser grows to this scale ... */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Hover", meta = (ClampMin = "1", ClampMax = "1.5"))
	float HoverScale = 1.06f;

	/** ... and gets a thin glow outline in this colour. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Hover")
	FLinearColor HoverGlowColor { 0.45f, 0.85f, 1.f };

	/** How fast the hover grows and glows in (interp speed). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Hover", meta = (ClampMin = "0.1"))
	float HoverSpeed = 14.f;

private:
	void UpdateHover(float DeltaTime);
	/** Puts the hovered button back as it was and hides its glow. */
	void ClearHover();
	/** The enabled button the laser hovers inside the widget (nested user widgets included), or null. */
	static UButton* FindHoveredButton(const UUserWidget* Widget);

	TWeakObjectPtr<UButton> HoveredButton;
	TWeakObjectPtr<UZooVRPanelHost> HoveredHost;
	FVector2D HoveredBaseScale { 1.f, 1.f };
	FVector2D HoveredBasePivot { 0.5f, 0.5f };
	float HoverAlpha = 0.f;

	// ====== Prompts ====== //

protected:
	/** Keyboard hints in any UI text, replaced by the controller buttons while VR is active (e.g. "[N]" -> "[A]"). */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Prompts")
	TMap<FString, FString> PromptReplacements
	{
		{ TEXT("[N]"), TEXT("[A]") },
		{ TEXT("N / Esc"), TEXT("A / B") },
		{ TEXT("[Esc]"), TEXT("[B]") },
		{ TEXT("[Spasi]"), TEXT("[Trigger]") },
		{ TEXT("Spasi"), TEXT("Trigger") },
		{ TEXT("[Alt]"), TEXT("[Trigger]") },
	};

	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Prompts", meta = (ClampMin = "0.02", Units = "s"))
	float PromptInterval = 0.1f;

private:
	void ReplacePrompts();
	void ReplacePromptsIn(UUserWidget* Widget);
	void ReplacePromptText(UWidget* Child) const;

	float PromptTimer = 0.f;

	// ====== Performance ====== //

protected:
	/** Set only while the headset renders, restored on end play. Defaults trade Lumen GI and Virtual Shadow Maps for a VR frame rate; VFX stay as is. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoo|VR|Performance")
	TMap<FString, FString> VRConsoleVariables
	{
		{ TEXT("r.DynamicGlobalIlluminationMethod"), TEXT("0") },
		{ TEXT("r.ReflectionMethod"), TEXT("2") },
		{ TEXT("r.Shadow.Virtual.Enable"), TEXT("0") },
		{ TEXT("r.Shadow.CSM.MaxCascades"), TEXT("2") },
		{ TEXT("r.Streaming.PoolSize"), TEXT("2000") },
		{ TEXT("vr.PixelDensity"), TEXT("0.9") },
	};

private:
	void ApplyConsoleVariables();
	void RestoreConsoleVariables();

	TMap<FString, FString> SavedConsoleVariables;

	// ====== Demo ====== //

	/**
	 * Dev autopilot (zoo.vr.Demo 1 or -ZooVRDemo, never in Shipping): the VR layer runs without a headset and
	 * UZooVRDemoDirector drives the head, the hands and the controller keys from a script. Off by default.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UZooVRDemoDirector> Demo;

	bool bDemo = false;

public:
	FORCEINLINE bool IsDemo() const { return bDemo; }

	/** Demo poses relative to the tracking origin (the pawn's feet): head (camera), both grips and the right aim (laser). */
	void SetDemoPoses(const FTransform& Head, const FTransform& Left, const FTransform& Right, const FTransform& Aim) const;

private:
	// ====== State ====== //

	/** True when a headset renders this play session; everything above stays off otherwise. */
	bool bVRActive = false;

public:
	APlayerController* GetController() const;
};
