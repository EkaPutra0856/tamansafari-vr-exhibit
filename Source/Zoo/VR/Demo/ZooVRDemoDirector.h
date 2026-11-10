// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ZooVRDemoDirector.generated.h"

class AActor;
class APlayerController;
class AZooARExhibit;
class UButton;
class UScrollBox;
class USkeletalMeshComponent;
class UUserWidget;
class UWidget;
class UWidgetComponent;
class UZooVRComponent;
struct FKey;

/** One scripted beat of the demo (JSON: Content/ZooVR/Demo/ZooVRDemoScript.json, keys = these names). */
USTRUCT()
struct FZooVRDemoStep
{
	GENERATED_BODY()

	/** wait, look, walk, route, aim, click, press, scroll, snap, hands, rest, dialogue, wait_until, marker, stick, teleport, end */
	UPROPERTY()
	FString Type;

	/** actor:<Label> | point:x,y,z | ui:<button text or widget name> | duma | exhibit:<Label> | route_end */
	UPROPERTY()
	FString Target;

	/** Added to a resolved target (world cm). */
	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	/** wait: duration. look / aim: blend hint. press: hold time. dialogue: reading pause per line. stick: duration. */
	UPROPERTY()
	float Seconds = 0.f;

	/** Give up (and go on) after this long. */
	UPROPERTY()
	float Timeout = 40.f;

	/** look without a target: head yaw / pitch relative to the body, degrees. stick: stick X / Y. */
	UPROPERTY()
	float Yaw = 0.f;

	UPROPERTY()
	float Pitch = 0.f;

	/** walk: arrival distance (cm). */
	UPROPERTY()
	float Radius = 60.f;

	/** walk: sprint. */
	UPROPERTY()
	bool bSprint = false;

	/** walk: plan on the NavMesh (false = straight line). */
	UPROPERTY()
	bool bNav = true;

	/** aim / click: the head also turns to the target. */
	UPROPERTY()
	bool bHead = true;

	/** press: select | menu | close | sprint | key:<KeyName> */
	UPROPERTY()
	FString Button;

	/** scroll: wheel notches (negative = down). */
	UPROPERTY()
	float Notches = -3.f;

	/** wait_until: dialogue | no_dialogue | menu_open | menu_closed | guiding | not_guiding | exhibit_panel:<Label> | exhibit_hidden:<Label> | ui:<text> | no_ui:<text> */
	UPROPERTY()
	FString Cond;

	/** hands: idle | relaxed | point | grab | fist, per hand (empty = unchanged). */
	UPROPERTY()
	FString Left;

	UPROPERTY()
	FString Right;

	/** marker: name written with the frame / time into Saved/DemoVideo/markers.txt. */
	UPROPERTY()
	FString Name;

	/** snap: steps (sign = direction). */
	UPROPERTY()
	int32 Count = 1;

	/** Added by the director itself (a scroll that reveals a clipped button). */
	bool bInserted = false;
};

/** Tunables and the beats, loaded from JSON. */
USTRUCT()
struct FZooVRDemoScript
{
	GENERATED_BODY()

	UPROPERTY()
	float EyeHeight = 165.f;

	/** Hands at rest, in head space (X ahead, Y right, Z up from the eyes). The left mirrors Y. */
	UPROPERTY()
	FVector RestHand { 34.f, 20.f, -38.f };

	/** The right hand while it points. */
	UPROPERTY()
	FVector PointHand { 40.f, 15.f, -17.f };

	/** Roll added to the right hand's pointing frame (negative = palm turns inward); the left mirrors it. */
	UPROPERTY()
	float AimRoll = -25.f;

	/** The right hand's finger frame at rest (X = fingers, Z = back of the hand), in head-yaw space; the left mirrors yaw and roll. */
	UPROPERTY()
	FRotator RestHandRotation { -40.f, -10.f, 70.f };

	/** Seconds the demo waits after the pawn exists before the first step. */
	UPROPERTY()
	float StartDelay = 1.f;

	/** Hand mesh relative to its grip (the XR template's). */
	UPROPERTY()
	FVector RightMeshOffset { -3.f, 3.5f, 4.6f };

	UPROPERTY()
	FRotator RightMeshRotation { 25.f, 0.f, 90.f };

	UPROPERTY()
	FVector LeftMeshOffset { -3.f, -3.5f, 4.6f };

	UPROPERTY()
	FRotator LeftMeshRotation { -25.f, -180.f, 90.f };

	/** Head turn speed (interp). */
	UPROPERTY()
	float HeadSpeed = 2.4f;

	/** Hand move speed (interp). */
	UPROPERTY()
	float HandSpeed = 5.f;

	/** Beyond this head yaw from the body a snap turn is used (degrees). */
	UPROPERTY()
	float SnapBeyond = 55.f;

	UPROPERTY()
	FString HandMeshRight = TEXT("/Game/ZooVR/Hands/Meshes/SKM_MannyXR_right.SKM_MannyXR_right");

	UPROPERTY()
	FString HandMeshLeft = TEXT("/Game/ZooVR/Hands/Meshes/SKM_MannyXR_left.SKM_MannyXR_left");

	UPROPERTY()
	FString HandAnimClass = TEXT("/Game/ZooVR/Hands/Meshes/ABP_MannequinsXR.ABP_MannequinsXR_C");

	UPROPERTY()
	TArray<FZooVRDemoStep> Steps;
};

/** Pose blend weights of one hand (the XR template's Animation Blueprint reads them). */
struct FZooVRDemoHandPose
{
	float Grasp = 0.f;
	float IndexCurl = 0.f;
	float Point = 0.f;
	float ThumbUp = 0.f;
};

/**
 * Dev autopilot for the VR layer: runs the Zoo in VR without a headset and plays a scripted visit as a real player would.
 * Head and hands are posed from the script (hand meshes with point / tap / grab gestures), and every click, scroll, menu,
 * walk and snap turn is injected as the Quest controller's own keys, so the recorded features are the real ones.
 * Enabled by zoo.vr.Demo 1 or -ZooVRDemo (never in Shipping); zoo.vr.DemoCapture 1 writes every frame as a PNG.
 */
UCLASS()
class UZooVRDemoDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UZooVRDemoDirector();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ====== API ====== //

	/** True when the demo was asked for (cvar or command line); always false in Shipping. */
	static bool IsRequested();

private:
	// ====== Script ====== //

	/** Loads the JSON (-ZooVRDemoScript=<path> or the default) into Script. */
	void LoadScript();

	/** Starts the step at StepIndex. */
	void BeginStep();

	/** Runs the current step; returns true when it is done. */
	bool TickStep(float DeltaTime);

	/** Moves to the next step (or ends). */
	void FinishStep();

	FZooVRDemoScript Script;
	int32 StepIndex = INDEX_NONE;
	float StepTime = 0.f;
	float WaitedBeforeStart = 0.f;
	bool bEnded = false;

	/** Scroll-to-reveal insertions made for the current click (bounded). */
	int32 RevealScrolls = 0;

	/** Sub-phase of a step (0 = begin). */
	int32 Phase = 0;
	float PhaseTime = 0.f;

	// ====== Targets ====== //

	/** World point for a target spec; false when it cannot be resolved right now. */
	bool ResolveTarget(const FString& Spec, const FVector& Offset, FVector& OutPoint) const;

	/** Button under a text or a widget name, on the VR panels and the visible AR panels. */
	UWidget* FindUIWidget(const FString& Query, UWidgetComponent*& OutPanel, UUserWidget*& OutTop) const;

	/** World centre of a widget drawn by a widget component (its top widget fills the draw size). */
	static bool WidgetWorldPoint(const UWidget* Widget, const UWidgetComponent* Panel, const UUserWidget* Top, FVector& OutPoint);

	/** The widget's enclosing scroll box, or null. */
	static UScrollBox* FindScrollBox(const UWidget* Widget);

	/** The enclosing scroll box when the widget is scrolled out of its view (OutDirection: +1 below, -1 above), else null. */
	static UScrollBox* FindClippingScrollBox(const UWidget* Widget, int32& OutDirection);

	AZooARExhibit* FindExhibit(const FString& Label) const;
	AActor* FindActor(const FString& Label) const;
	FVector GetDumaPoint() const;

	// ====== Body ====== //

	/** Head and hands this frame, then the rig. */
	void UpdatePoses(float DeltaTime);

	/** World yaw / pitch the head should look at (set by steps). */
	void LookAtPoint(const FVector& Point);
	void LookRelative(float Yaw, float Pitch);

	/** Where the right index points (world), or none for rest. */
	void AimAt(const FVector& Point);
	void StopAiming();

	/** Body yaw: the pawn's (snap turns rotate it). */
	float GetBodyYaw() const;
	FVector GetEyeLocation() const;

	FRotator HeadRelative { 0.f, 0.f, 0.f };
	FRotator HeadTarget { 0.f, 0.f, 0.f };
	bool bHeadTrackPoint = false;
	FVector HeadPoint = FVector::ZeroVector;

	/** Right hand aim target (world) while bAiming. */
	bool bAiming = false;
	FVector AimPoint = FVector::ZeroVector;

	/** Extra pitch on the aim while a hold-to-scroll sweep runs (degrees). */
	float SweepPitch = 0.f;

	/** Snap turns are one stick flick each; re-armed after a short pause. */
	float SnapCooldown = 0.f;

	/** Right index tip in the grip's space, from the posed mesh (laser origin). */
	FVector TipLocal = FVector::ZeroVector;
	bool bTipKnown = false;

	/** Finger frame (X = index direction, Z = back of the hand) in each grip's space, measured from the posed mesh. */
	FQuat GripToFingers[2] { FQuat::Identity, FQuat::Identity };
	bool bFingersKnown[2] { false, false };

	/** Hand world transforms (smoothed). */
	FVector HandLocation[2] { FVector::ZeroVector, FVector::ZeroVector };
	FQuat HandRotation[2] { FQuat::Identity, FQuat::Identity };
	bool bHandsPlaced = false;

	float IdleTime = 0.f;

	// ====== Hand Meshes ====== //

	void BuildHands();
	void UpdateHands(float DeltaTime);

	/** Named pose for a hand (idle, relaxed, point, grab, fist). */
	void SetHandPose(int32 Hand, const FString& Pose);

	/** A quick index tap (click) or a squeeze (button) on a hand. */
	void PlayTap(int32 Hand);
	void PlaySqueeze(int32 Hand);

	/** Index fingertip of the right hand, in world space (laser origin). */
	FVector GetFingertip(int32 Hand) const;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> HandMeshes[2];

	FZooVRDemoHandPose HandPoseTarget[2];
	FZooVRDemoHandPose HandPoseCurrent[2];
	float TapTime[2] { -1.f, -1.f };
	float SqueezeTime[2] { -1.f, -1.f };
	FName FingertipBone[2];
	FName IndexRootBone[2];
	FName PinkyRootBone[2];
	bool bHandsBuilt = false;

	/** Measures the finger frame and the fingertip of a hand in its grip's space. */
	void MeasureHand(int32 Hand);

	// ====== Input ====== //

	/** Presses / releases the key the VR mapping context binds to the action name (IA_ZooVR_Select, IA_Sprint, ...). */
	void SetButton(const FString& Button, bool bDown);
	void SendKey(const FKey& Key, bool bDown) const;
	void SendAxis(const FKey& Key, float Value) const;

	/** Keys bound in the VR mapping context, by action name. */
	FKey KeyForAction(const FName ActionName) const;

	/** Left stick (walk) and right stick (snap / scroll) this frame; sent every frame, zero when idle. */
	FVector2D LeftStick = FVector2D::ZeroVector;
	FVector2D RightStick = FVector2D::ZeroVector;
	void SendSticks();

	TSet<FString> HeldButtons;

	// ====== Walking ====== //

	/** Builds the waypoints to a point (NavMesh or straight). */
	void PlanWalk(const FVector& Goal, bool bNav);

	/** Steers the left stick toward the next waypoint; true on arrival. */
	bool TickWalk(float DeltaTime, float Radius, bool bLookAhead);

	TArray<FVector> Waypoints;
	int32 WaypointIndex = 0;
	float StuckTime = 0.f;
	FVector LastPawnLocation = FVector::ZeroVector;

	// ====== Capture ====== //

	/** Asks the viewport for this frame's pixels (HandleScreenshotCaptured writes them). */
	void CaptureFrame();

	/** The frame's pixels: a JPEG write on the image write queue (the engine's own PNG save is synchronous and slow). */
	void HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors);
	void WriteMarker(const FString& Name);

	bool bCapturing = false;
	int32 FrameIndex = 0;
	FString FramesDir;
	TArray<FString> Markers;

	// ====== State ====== //

	UPROPERTY(Transient)
	TObjectPtr<UZooVRComponent> VR;

	bool bArrived = false;
	void HandleArrived(const FGameplayTag& PlaceTag);

	APlayerController* GetController() const;
	APawn* GetPawn() const;
	bool IsDialogueActive() const;
	bool IsDialogueTyping() const;
};
