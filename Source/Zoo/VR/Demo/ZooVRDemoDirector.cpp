// Copyright Epic Games, Inc. All Rights Reserved.

#include "VR/Demo/ZooVRDemoDirector.h"

#include "AR/ZooARExhibit.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Button.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Dialogue/Data/WzDialogueAsset.h"
#include "Dialogue/WzDialogueSubsystem.h"
#include "Duma/ZooDuma.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImagePixelData.h"
#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "MotionControllerComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "PathGuide/Components/PathGuideComponent.h"
#include "Player/ZooPlayerController.h"
#include "UnrealClient.h"
#include "VR/ZooVRComponent.h"
#include "VR/ZooVRPanelHost.h"

DEFINE_LOG_CATEGORY_STATIC(LogZooVRDemo, Log, All);

static TAutoConsoleVariable<int32> CVarZooVRDemo(TEXT("zoo.vr.Demo"), 0, TEXT("1: run the VR layer without a headset, driven by the demo script (dev only)."));
static TAutoConsoleVariable<int32> CVarZooVRDemoCapture(TEXT("zoo.vr.DemoCapture"), 0, TEXT("1: the demo writes every frame to Saved/DemoVideo/Frames as PNG."));

namespace
{
	constexpr int32 LeftHand = 0;
	constexpr int32 RightHand = 1;
	constexpr float CaptureFps = 30.f;

	/** Case-insensitive "contains" with the spaces squeezed out of both sides. */
	bool TextMatches(const FString& Haystack, const FString& Needle)
	{
		return Haystack.Replace(TEXT(" "), TEXT("")).Contains(Needle.Replace(TEXT(" "), TEXT("")), ESearchCase::IgnoreCase);
	}

	FString WidgetText(const UWidget* Widget)
	{
		if (const UTextBlock* Plain = Cast<UTextBlock>(Widget)) return Plain->GetText().ToString();
		if (const URichTextBlock* Rich = Cast<URichTextBlock>(Widget)) return Rich->GetText().ToString();

		return FString();
	}
}

// ====== Lifecycle ====== //

UZooVRDemoDirector::UZooVRDemoDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UZooVRDemoDirector::BeginPlay()
{
	Super::BeginPlay();

	VR = GetOwner()->FindComponentByClass<UZooVRComponent>();
	LoadScript();

	// Arrival of the guided route
	if (const AZooPlayerController* Visitor = Cast<AZooPlayerController>(GetController())) Visitor->GetPathGuide()->OnArrived.AddUObject(this, &UZooVRDemoDirector::HandleArrived);

	// Frame capture into a fresh folder
	bCapturing = CVarZooVRDemoCapture.GetValueOnGameThread() != 0 || FParse::Param(FCommandLine::Get(), TEXT("ZooVRDemoCapture"));
	FramesDir = FPaths::ProjectSavedDir() / TEXT("DemoVideo") / TEXT("Frames");
	if (bCapturing)
	{
		IFileManager::Get().DeleteDirectory(*FramesDir, false, true);
		IFileManager::Get().MakeDirectory(*FramesDir, true);
		UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UZooVRDemoDirector::HandleScreenshotCaptured);
	}
	UE_LOG(LogZooVRDemo, Log, TEXT("Demo director up: %d steps, capture %d"), Script.Steps.Num(), bCapturing);
}

void UZooVRDemoDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Markers even when the run was cut short
	UGameViewportClient::OnScreenshotCaptured().RemoveAll(this);
	if (!Markers.IsEmpty()) FFileHelper::SaveStringArrayToFile(Markers, *(FPaths::ProjectSavedDir() / TEXT("DemoVideo") / TEXT("markers.txt")));
	for (const FString& Button : HeldButtons.Array())
	{
		SetButton(Button, false);
	}

	Super::EndPlay(EndPlayReason);
}

void UZooVRDemoDirector::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The rig exists once the pawn does
	if (bEnded || !VR || !VR->GetVROrigin() || !GetPawn()) return;
	if (!bHandsBuilt) BuildHands();

	// First step after a short settle
	if (StepIndex == INDEX_NONE)
	{
		WaitedBeforeStart += DeltaTime;
		if (WaitedBeforeStart >= Script.StartDelay && !Script.Steps.IsEmpty())
		{
			StepIndex = 0;
			BeginStep();
		}
	}
	else if (StepIndex < Script.Steps.Num())
	{
		StepTime += DeltaTime;
		PhaseTime += DeltaTime;
		if (TickStep(DeltaTime)) FinishStep();
	}

	UpdatePoses(DeltaTime);
	UpdateHands(DeltaTime);
	SendSticks();
	CaptureFrame();
}

// ====== API ====== //

bool UZooVRDemoDirector::IsRequested()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return CVarZooVRDemo.GetValueOnGameThread() != 0 || FParse::Param(FCommandLine::Get(), TEXT("ZooVRDemo"));
#endif
}

// ====== Script ====== //

void UZooVRDemoDirector::LoadScript()
{
	// -ZooVRDemoScript=<path> or the default next to the VR content
	FString Path;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ZooVRDemoScript="), Path)) Path = FPaths::ProjectContentDir() / TEXT("ZooVR/Demo/ZooVRDemoScript.json");
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogZooVRDemo, Error, TEXT("Demo script not found: %s"), *Path);

		return;
	}
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Script)) UE_LOG(LogZooVRDemo, Error, TEXT("Demo script failed to parse: %s"), *Path);
	UE_LOG(LogZooVRDemo, Log, TEXT("Demo script %s: %d steps"), *Path, Script.Steps.Num());
}

void UZooVRDemoDirector::BeginStep()
{
	const FZooVRDemoStep& Step = Script.Steps[StepIndex];
	StepTime = 0.f;
	Phase = 0;
	PhaseTime = 0.f;
	UE_LOG(LogZooVRDemo, Log, TEXT("[%7.2f] step %d/%d: %s %s %s %s"), GetWorld()->GetTimeSeconds(), StepIndex + 1, Script.Steps.Num(), *Step.Type, *Step.Target, *Step.Button, *Step.Cond);
}

void UZooVRDemoDirector::FinishStep()
{
	// Nothing held across steps
	for (const FString& Button : HeldButtons.Array())
	{
		SetButton(Button, false);
	}
	LeftStick = FVector2D::ZeroVector;
	SweepPitch = 0.f;
	if (!Script.Steps[StepIndex].bInserted) RevealScrolls = 0;

	++StepIndex;
	if (StepIndex < Script.Steps.Num()) BeginStep();
	else UE_LOG(LogZooVRDemo, Log, TEXT("Demo script finished"));
}

bool UZooVRDemoDirector::TickStep(const float DeltaTime)
{
	FZooVRDemoStep& Step = Script.Steps[StepIndex];
	const FString& Type = Step.Type;
	const bool bTimedOut = StepTime > Step.Timeout;
	if (bTimedOut) UE_LOG(LogZooVRDemo, Warning, TEXT("step %d (%s %s) timed out after %.1fs"), StepIndex + 1, *Type, *Step.Target, StepTime);

	// Wait
	if (Type == TEXT("wait")) return StepTime >= Step.Seconds;

	// Marker: a named moment for the timestamp list
	if (Type == TEXT("marker"))
	{
		WriteMarker(Step.Name);

		return true;
	}

	// Probe: logs where an exhibit's hologram body really is (its mesh may sit far off its pivot) and where the camera is
	if (Type == TEXT("probe"))
	{
		if (const AZooARExhibit* Exhibit = FindExhibit(Step.Target))
		{
			const USkeletalMeshComponent* Mesh = Exhibit->AnimalMesh;
			FVector Eye;
			FRotator View;
			if (APlayerController* PC = GetController()) PC->GetPlayerViewPoint(Eye, View);
			UE_LOG(LogZooVRDemo, Log, TEXT("probe %s: pivot %s body %s extent %s visible %d | eye %s view %s"), *Step.Target, *Mesh->GetComponentLocation().ToCompactString(),
				*Mesh->Bounds.Origin.ToCompactString(), *Mesh->Bounds.BoxExtent.ToCompactString(), Mesh->IsVisible(), *Eye.ToCompactString(), *View.ToCompactString());
		}

		return true;
	}

	// Hands: named poses
	if (Type == TEXT("hands"))
	{
		if (!Step.Left.IsEmpty()) SetHandPose(LeftHand, Step.Left);
		if (!Step.Right.IsEmpty()) SetHandPose(RightHand, Step.Right);

		return true;
	}

	// Rest: the right hand drops the aim
	if (Type == TEXT("rest"))
	{
		if (Phase == 0)
		{
			StopAiming();
			Phase = 1;
		}

		return StepTime >= FMath::Max(Step.Seconds, 0.6f);
	}

	// Look: at a target (tracked until another look) or a head angle
	if (Type == TEXT("look"))
	{
		FVector Point;
		if (!Step.Target.IsEmpty() && ResolveTarget(Step.Target, Step.Offset, Point)) LookAtPoint(Point);
		else if (Step.Target.IsEmpty()) LookRelative(Step.Yaw, Step.Pitch);
		const float Blend = Step.Seconds > 0.f ? Step.Seconds : 1.5f;
		const bool bConverged = HeadTarget.Equals(HeadRelative, 2.f) && SnapCooldown <= 0.f;

		return bTimedOut || (StepTime >= Blend && bConverged) || StepTime >= Blend * 2.5f;
	}

	// Aim: the right index points at the target
	if (Type == TEXT("aim"))
	{
		FVector Point;
		if (ResolveTarget(Step.Target, Step.Offset, Point))
		{
			AimAt(Point);
			if (Step.bHead) LookAtPoint(Point);
		}
		const float Blend = Step.Seconds > 0.f ? Step.Seconds : 0.8f;

		return bTimedOut || StepTime >= Blend;
	}

	// Snap turns: one stick flick per step
	if (Type == TEXT("snap"))
	{
		if (Phase < FMath::Abs(Step.Count) && PhaseTime >= 0.7f)
		{
			RightStick.X = FMath::Sign(Step.Count);
			SnapCooldown = 0.6f;
			++Phase;
			PhaseTime = 0.f;
		}

		return Phase >= FMath::Abs(Step.Count) && PhaseTime >= 0.7f;
	}

	// Teleport: the pawn to a point, facing Yaw (demo staging only)
	if (Type == TEXT("teleport"))
	{
		FVector Point;
		APawn* Pawn = GetPawn();
		APlayerController* Controller = GetController();
		if (Pawn && Controller && ResolveTarget(Step.Target, Step.Offset, Point))
		{
			Pawn->SetActorLocation(Point, false, nullptr, ETeleportType::TeleportPhysics);
			Controller->SetControlRotation({ 0.f, Step.Yaw, 0.f });
			HeadRelative = HeadTarget = { Step.Pitch, 0.f, 0.f };
			bHeadTrackPoint = false;
			bHandsPlaced = false;
		}

		return true;
	}

	// Raw left stick
	if (Type == TEXT("stick"))
	{
		LeftStick = { Step.Yaw, Step.Pitch };

		return StepTime >= Step.Seconds;
	}

	// Press: a controller button with a squeeze of the hand
	if (Type == TEXT("press"))
	{
		const float Hold = Step.Seconds > 0.f ? Step.Seconds : 0.15f;
		const bool bLeftHandButton = Step.Button == TEXT("sprint");
		if (Phase == 0)
		{
			PlaySqueeze(bLeftHandButton ? LeftHand : RightHand);
			Phase = 1;
			PhaseTime = 0.f;
		}
		else if (Phase == 1 && PhaseTime >= 0.1f)
		{
			SetButton(Step.Button, true);
			Phase = 2;
			PhaseTime = 0.f;
		}
		else if (Phase == 2 && PhaseTime >= Hold)
		{
			SetButton(Step.Button, false);
			Phase = 3;
			PhaseTime = 0.f;
		}

		return Phase == 3 && PhaseTime >= 0.35f;
	}

	// Click: aim, settle, tap the trigger
	if (Type == TEXT("click"))
	{
		FVector Point;
		const bool bHasTarget = !Step.Target.IsEmpty();
		if (bHasTarget && ResolveTarget(Step.Target, Step.Offset, Point))
		{
			AimAt(Point);
			if (Step.bHead) LookAtPoint(Point);
		}
		else if (bHasTarget && Phase == 0)
		{
			// Not there (yet): wait for it
			return bTimedOut;
		}
		const float Settle = Step.Seconds > 0.f ? Step.Seconds : 0.9f;
		if (Phase == 0)
		{
			// A card scrolled out of its list: scroll it into view first
			int32 Direction = 0;
			UWidgetComponent* Panel = nullptr;
			UUserWidget* Top = nullptr;
			const UWidget* Found = bHasTarget && Step.Target.StartsWith(TEXT("ui:")) ? FindUIWidget(Step.Target.Mid(3), Panel, Top) : nullptr;
			if (Found && RevealScrolls < 2 && FindClippingScrollBox(Found, Direction))
			{
				FZooVRDemoStep Scroll;
				Scroll.Type = TEXT("scroll");
				Scroll.Target = TEXT("scrollof:") + Step.Target.Mid(3);
				Scroll.Notches = -3.f * Direction;
				Scroll.Timeout = 10.f;
				Scroll.bInserted = true;
				++RevealScrolls;
				const int32 Keep = RevealScrolls;
				Script.Steps.Insert(Scroll, StepIndex);
				BeginStep();
				RevealScrolls = Keep;

				return false;
			}
			if (PhaseTime >= Settle)
			{
				PlayTap(RightHand);
				Phase = 1;
				PhaseTime = 0.f;
			}
		}
		else if (Phase == 1 && PhaseTime >= 0.1f)
		{
			SetButton(TEXT("select"), true);
			Phase = 2;
			PhaseTime = 0.f;
		}
		else if (Phase == 2 && PhaseTime >= 0.12f)
		{
			SetButton(TEXT("select"), false);
			Phase = 3;
			PhaseTime = 0.f;
		}

		return bTimedOut || (Phase == 3 && PhaseTime >= 0.4f);
	}

	// Scroll: hold the trigger and sweep the laser (the VR layer's hold-to-scroll)
	if (Type == TEXT("scroll"))
	{
		FVector Point;
		if (!Step.Target.IsEmpty() && ResolveTarget(Step.Target, Step.Offset, Point))
		{
			AimAt(Point);
			if (Step.bHead) LookAtPoint(Point);
		}
		const float Sweep = 4.f + 2.f * FMath::Abs(Step.Notches);
		const float SweepTime = 0.35f + 0.22f * FMath::Abs(Step.Notches);
		if (Phase == 0 && PhaseTime >= 0.7f)
		{
			SetHandPose(RightHand, TEXT("point"));
			SetButton(TEXT("select"), true);
			Phase = 1;
			PhaseTime = 0.f;
		}
		else if (Phase == 1)
		{
			// Notches down = laser down
			SweepPitch = FMath::Sign(Step.Notches) * Sweep * FMath::Clamp(PhaseTime / SweepTime, 0.f, 1.f);
			if (PhaseTime >= SweepTime + 0.25f)
			{
				SetButton(TEXT("select"), false);
				Phase = 2;
				PhaseTime = 0.f;
			}
		}
		else if (Phase == 2)
		{
			SweepPitch = FMath::FInterpTo(SweepPitch, 0.f, DeltaTime, 6.f);
		}

		return bTimedOut || (Phase == 2 && PhaseTime >= 0.6f);
	}

	// Dialogue: read each line, then tap to go on, until it ends
	if (Type == TEXT("dialogue"))
	{
		const FVector Duma = GetDumaPoint();
		if (!Duma.IsZero())
		{
			// A little below him, so he and the subtitle under him share the view
			LookAtPoint(Duma - FVector{ 0.f, 0.f, 18.f });
			if (Step.bHead) AimAt(Duma);
		}
		if (!IsDialogueActive())
		{
			// Gone (or never came)
			return Phase > 0 || StepTime >= FMath::Min(Step.Timeout, 6.f);
		}
		if (Phase == 0)
		{
			// Let the line type out, then read it
			if (IsDialogueTyping()) PhaseTime = 0.f;
			const float Read = Step.Seconds > 0.f ? Step.Seconds : 1.6f;
			if (PhaseTime >= Read)
			{
				PlayTap(RightHand);
				Phase = 1;
				PhaseTime = 0.f;
			}
		}
		else if (Phase == 1 && PhaseTime >= 0.1f)
		{
			SetButton(TEXT("select"), true);
			Phase = 2;
			PhaseTime = 0.f;
		}
		else if (Phase == 2 && PhaseTime >= 0.12f)
		{
			SetButton(TEXT("select"), false);
			Phase = 3;
			PhaseTime = 0.f;
		}
		else if (Phase == 3 && PhaseTime >= 0.5f)
		{
			Phase = 4;
			PhaseTime = 0.f;
		}
		else if (Phase == 4)
		{
			// Next line
			Phase = 0;
			PhaseTime = 0.f;
		}

		return bTimedOut;
	}

	// Walk: to a point, on the NavMesh (an open destination list blocks walking: close it first)
	if (Type == TEXT("walk") || Type == TEXT("route"))
	{
		const AZooPlayerController* Visitor = Cast<AZooPlayerController>(GetController());
		if (Phase == 0 && Visitor && Visitor->IsDestinationMenuOpen() && !Step.bInserted)
		{
			FZooVRDemoStep Close;
			Close.Type = TEXT("press");
			Close.Button = TEXT("close");
			Close.bInserted = true;
			Script.Steps.Insert(Close, StepIndex);
			BeginStep();

			return false;
		}
	}
	if (Type == TEXT("walk"))
	{
		if (Phase == 0)
		{
			FVector Goal;
			if (!ResolveTarget(Step.Target, Step.Offset, Goal)) return bTimedOut;

			PlanWalk(Goal, Step.bNav);
			if (Step.bSprint) SetButton(TEXT("sprint"), true);
			Phase = 1;
		}
		const bool bThere = TickWalk(DeltaTime, Step.Radius, Step.bHead);
		if (bThere || bTimedOut)
		{
			LeftStick = FVector2D::ZeroVector;
			if (Step.bSprint) SetButton(TEXT("sprint"), false);
		}

		return bThere || bTimedOut;
	}

	// Route: follow the Path Guide's route until the arrival
	if (Type == TEXT("route"))
	{
		const AZooPlayerController* Visitor = Cast<AZooPlayerController>(GetController());
		const UWzPathGuideComponent* Guide = Visitor ? Visitor->GetPathGuide() : nullptr;
		if (!Guide) return true;

		if (Phase == 0)
		{
			bArrived = false;
			if (Step.bSprint) SetButton(TEXT("sprint"), true);
			Phase = 1;
		}
		const TArray<FVector>& Route = Guide->GetRoute();
		if (Guide->GetState() != EPathGuideState::Guiding || Route.Num() < 2)
		{
			LeftStick = FVector2D::ZeroVector;

			return bArrived || StepTime > 2.f || bTimedOut;
		}

		// Aim ~250 cm ahead of the nearest route point
		const FVector Here = GetPawn()->GetActorLocation();
		int32 Nearest = 0;
		float Best = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Route.Num(); ++Index)
		{
			const float Dist = FVector::DistSquared2D(Route[Index], Here);
			if (Dist < Best)
			{
				Best = Dist;
				Nearest = Index;
			}
		}
		int32 Ahead = Nearest;
		while (Ahead < Route.Num() - 1 && FVector::Dist2D(Route[Ahead], Here) < 250.f) ++Ahead;
		Waypoints = { Route[Ahead] };
		WaypointIndex = 0;
		TickWalk(DeltaTime, 40.f, Step.bHead);

		return bArrived || bTimedOut;
	}

	// Wait until a game condition holds
	if (Type == TEXT("wait_until"))
	{
		const FString& Cond = Step.Cond;
		const AZooPlayerController* Visitor = Cast<AZooPlayerController>(GetController());
		bool bHolds = false;
		if (Cond == TEXT("dialogue")) bHolds = IsDialogueActive();
		else if (Cond == TEXT("no_dialogue")) bHolds = !IsDialogueActive();
		else if (Cond == TEXT("menu_open")) bHolds = Visitor && Visitor->IsDestinationMenuOpen();
		else if (Cond == TEXT("menu_closed")) bHolds = Visitor && !Visitor->IsDestinationMenuOpen();
		else if (Cond == TEXT("guiding")) bHolds = Visitor && Visitor->GetPathGuide()->GetState() == EPathGuideState::Guiding;
		else if (Cond == TEXT("not_guiding")) bHolds = Visitor && Visitor->GetPathGuide()->GetState() != EPathGuideState::Guiding;
		else if (Cond == TEXT("arrived")) bHolds = bArrived;
		else if (Cond.StartsWith(TEXT("exhibit_panel:")))
		{
			const AZooARExhibit* Exhibit = FindExhibit(Cond.Mid(14));
			bHolds = Exhibit && Exhibit->InformationPanel && Exhibit->InformationPanel->IsVisible();
		}
		else if (Cond.StartsWith(TEXT("exhibit_started:")))
		{
			const AZooARExhibit* Exhibit = FindExhibit(Cond.Mid(16));
			bHolds = Exhibit && Exhibit->HasPresentationStarted();
		}
		else if (Cond.StartsWith(TEXT("exhibit_hidden:")))
		{
			const AZooARExhibit* Exhibit = FindExhibit(Cond.Mid(15));
			bHolds = Exhibit && !Exhibit->HasPresentationStarted() && !Exhibit->IsZoneActive();
		}
		else if (Cond.StartsWith(TEXT("ui:")))
		{
			UWidgetComponent* Panel = nullptr;
			UUserWidget* Top = nullptr;
			bHolds = FindUIWidget(Cond.Mid(3), Panel, Top) != nullptr;
		}
		else if (Cond.StartsWith(TEXT("no_ui:")))
		{
			UWidgetComponent* Panel = nullptr;
			UUserWidget* Top = nullptr;
			bHolds = FindUIWidget(Cond.Mid(6), Panel, Top) == nullptr;
		}
		else bHolds = true;

		// The optional pause after it holds
		if (bHolds && Phase == 0)
		{
			Phase = 1;
			PhaseTime = 0.f;
		}

		return bTimedOut || (Phase == 1 && PhaseTime >= Step.Seconds);
	}

	// End: flush, then quit
	if (Type == TEXT("end"))
	{
		if (Phase == 0)
		{
			WriteMarker(TEXT("end"));
			bCapturing = false;
			FFileHelper::SaveStringArrayToFile(Markers, *(FPaths::ProjectSavedDir() / TEXT("DemoVideo") / TEXT("markers.txt")));
			Phase = 1;
			PhaseTime = 0.f;
		}
		if (PhaseTime >= 3.f)
		{
			bEnded = true;
			UE_LOG(LogZooVRDemo, Log, TEXT("Demo end: %d frames captured"), FrameIndex);
			if (APlayerController* Controller = GetController()) Controller->ConsoleCommand(TEXT("quit"));
		}

		return false;
	}

	UE_LOG(LogZooVRDemo, Warning, TEXT("Unknown step type '%s'"), *Type);

	return true;
}

// ====== Targets ====== //

bool UZooVRDemoDirector::ResolveTarget(const FString& Spec, const FVector& Offset, FVector& OutPoint) const
{
	// Duma's face
	if (Spec == TEXT("duma"))
	{
		OutPoint = GetDumaPoint();
		if (OutPoint.IsZero()) return false;

		OutPoint += Offset;

		return true;
	}

	// A fixed world point
	if (Spec.StartsWith(TEXT("point:")))
	{
		TArray<FString> Parts;
		Spec.Mid(6).ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 3) return false;

		OutPoint = FVector{ FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]) } + Offset;

		return true;
	}

	// An actor by label
	if (Spec.StartsWith(TEXT("actor:")))
	{
		const AActor* Actor = FindActor(Spec.Mid(6));
		if (!Actor) return false;

		OutPoint = Actor->GetActorLocation() + Offset;

		return true;
	}

	// An exhibit's hologram
	if (Spec.StartsWith(TEXT("exhibit:")))
	{
		const AZooARExhibit* Exhibit = FindExhibit(Spec.Mid(8));
		if (!Exhibit) return false;

		OutPoint = (Exhibit->HologramRoot ? Exhibit->HologramRoot->GetComponentLocation() : Exhibit->GetActorLocation()) + Offset;

		return true;
	}

	// A button (or its enclosing scroll box) on a panel
	if (Spec.StartsWith(TEXT("ui:")) || Spec.StartsWith(TEXT("scrollof:")))
	{
		const bool bScrollBox = Spec.StartsWith(TEXT("scrollof:"));
		UWidgetComponent* Panel = nullptr;
		UUserWidget* Top = nullptr;
		const UWidget* Widget = FindUIWidget(Spec.Mid(bScrollBox ? 9 : 3), Panel, Top);
		if (Widget && bScrollBox) Widget = FindScrollBox(Widget);
		if (!Widget || !WidgetWorldPoint(Widget, Panel, Top, OutPoint)) return false;

		OutPoint += Offset;

		return true;
	}

	return false;
}

UWidget* UZooVRDemoDirector::FindUIWidget(const FString& Query, UWidgetComponent*& OutPanel, UUserWidget*& OutTop) const
{
	// The panels that take the laser: the VR panels, then the visible AR information panels
	TArray<TPair<UWidgetComponent*, UUserWidget*>> Panels;
	if (VR)
	{
		const TArray<TObjectPtr<UWidgetComponent>>& VRPanels = VR->GetPanels();
		const TArray<TObjectPtr<UZooVRPanelHost>>& Hosts = VR->GetHosts();
		for (int32 Index = 0; Index < VRPanels.Num(); ++Index)
		{
			if (VRPanels[Index] && Hosts.IsValidIndex(Index) && Hosts[Index]) Panels.Emplace(VRPanels[Index], Hosts[Index]);
		}
	}
	for (TActorIterator<AZooARExhibit> It(GetWorld()); It; ++It)
	{
		UWidgetComponent* Panel = It->InformationPanel;
		if (Panel && Panel->IsVisible() && Panel->GetUserWidgetObject()) Panels.Emplace(Panel, Panel->GetUserWidgetObject());
	}

	for (const auto& [Panel, Top] : Panels)
	{
		if (!Top->WidgetTree) continue;

		// A visible widget named like the query, or a text containing it
		UWidget* Found = nullptr;
		Top->WidgetTree->ForEachWidgetAndDescendants([&Found, &Query](UWidget* Child)
		{
			if (!Found && Child->IsVisible() && (Child->GetName() == Query || TextMatches(WidgetText(Child), Query))) Found = Child;
		});
		if (!Found) continue;

		// A text: the button around it, else the first button of its own widget
		if (!Found->IsA<UButton>() && !Found->IsA<UScrollBox>())
		{
			UWidget* Button = nullptr;
			for (UWidget* Up = Found->GetParent(); Up && !Button; Up = Up->GetParent())
			{
				if (Up->IsA<UButton>()) Button = Up;
			}
			if (!Button)
			{
				const UUserWidget* Owner = Found->GetTypedOuter<UUserWidget>();
				if (Owner && Owner->WidgetTree) Owner->WidgetTree->ForEachWidgetAndDescendants([&Button](UWidget* Child)
				{
					if (!Button && Child->IsA<UButton>() && Child->IsVisible()) Button = Child;
				});
			}
			if (Button) Found = Button;
		}
		OutPanel = Panel;
		OutTop = Top;

		return Found;
	}

	return nullptr;
}

bool UZooVRDemoDirector::WidgetWorldPoint(const UWidget* Widget, const UWidgetComponent* Panel, const UUserWidget* Top, FVector& OutPoint)
{
	if (!Widget || !Panel || !Top) return false;

	// Centre of the widget in the top widget's space (which is the panel's draw space)
	const FGeometry& TopGeometry = Top->GetCachedGeometry();
	const FGeometry& Geometry = Widget->GetCachedGeometry();
	if (Geometry.GetAbsoluteSize().IsNearlyZero() || TopGeometry.GetAbsoluteSize().IsNearlyZero()) return false;

	// Its centre, kept inside the visible part of an enclosing scroll box (a half-clipped button is clicked where it shows)
	FVector2D Centre = Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize() * 0.5f;
	if (const UScrollBox* Scroll = FindScrollBox(Widget))
	{
		const FGeometry& Box = Scroll->GetCachedGeometry();
		const FVector2D BoxMin = Box.GetAbsolutePosition() + FVector2D{ 6.f, 6.f };
		const FVector2D BoxMax = Box.GetAbsolutePosition() + Box.GetAbsoluteSize() - FVector2D{ 6.f, 6.f };
		const FVector2D ItemMin = Geometry.GetAbsolutePosition() + FVector2D{ 4.f, 4.f };
		const FVector2D ItemMax = Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize() - FVector2D{ 4.f, 4.f };
		const FVector2D Min = FVector2D::Max(BoxMin, ItemMin);
		const FVector2D Max = FVector2D::Min(BoxMax, ItemMax);
		if (Min.X < Max.X && Min.Y < Max.Y) Centre = (Min + Max) * 0.5f;
	}
	// Top widget units to the panel's current draw units (an AR panel drawn at its desired size grows when a card opens)
	const FVector2D Draw = Panel->GetCurrentDrawSize();
	const FVector2D TopSize = TopGeometry.GetLocalSize();
	const FVector2D Local = TopGeometry.AbsoluteToLocal(Centre) * FVector2D{ Draw.X / TopSize.X, Draw.Y / TopSize.Y };
	const FVector2D Pivot = Panel->GetPivot();

	// Inverse of UWidgetComponent::GetLocalHitLocation
	OutPoint = Panel->GetComponentTransform().TransformPosition(FVector{ 0.0, -(Local.X - Draw.X * Pivot.X), -(Local.Y - Draw.Y * Pivot.Y) });

	return true;
}

UScrollBox* UZooVRDemoDirector::FindScrollBox(const UWidget* Widget)
{
	// Up the tree; a user widget's root has no parent, so hop to the user widget itself (its slot sits in the list)
	const UWidget* Current = Widget;
	while (Current)
	{
		UWidget* Up = Current->GetParent();
		if (!Up) Up = Current->GetTypedOuter<UUserWidget>();
		if (Up == Current) break;
		if (UScrollBox* Scroll = Cast<UScrollBox>(Up)) return Scroll;
		Current = Up;
	}

	return nullptr;
}

UScrollBox* UZooVRDemoDirector::FindClippingScrollBox(const UWidget* Widget, int32& OutDirection)
{
	OutDirection = 0;
	UScrollBox* Scroll = FindScrollBox(Widget);
	if (!Scroll) return nullptr;

	// Mostly out of the box's view (a quarter of its height or more), above or below
	const FGeometry& Box = Scroll->GetCachedGeometry();
	const FGeometry& Item = Widget->GetCachedGeometry();
	const float Top = Box.GetAbsolutePosition().Y;
	const float Bottom = Top + Box.GetAbsoluteSize().Y;
	const float ItemTop = Item.GetAbsolutePosition().Y;
	const float ItemBottom = ItemTop + Item.GetAbsoluteSize().Y;
	const float Allowed = FMath::Max(Item.GetAbsoluteSize().Y * 0.25f, 2.f);
	if (ItemBottom > Bottom + Allowed) OutDirection = 1;
	else if (ItemTop < Top - Allowed) OutDirection = -1;

	return OutDirection != 0 ? Scroll : nullptr;
}

AZooARExhibit* UZooVRDemoDirector::FindExhibit(const FString& Label) const
{
	for (TActorIterator<AZooARExhibit> It(GetWorld()); It; ++It)
	{
		if (It->GetActorNameOrLabel() == Label || It->GetName() == Label) return *It;
	}

	return nullptr;
}

AActor* UZooVRDemoDirector::FindActor(const FString& Label) const
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetActorNameOrLabel() == Label || It->GetName() == Label) return *It;
	}

	return nullptr;
}

FVector UZooVRDemoDirector::GetDumaPoint() const
{
	const AZooDuma* Duma = Cast<AZooDuma>(UGameplayStatics::GetActorOfClass(this, AZooDuma::StaticClass()));
	if (!Duma || Duma->IsHidden()) return FVector::ZeroVector;

	return Duma->GetActorLocation() + FVector{ 0.f, 0.f, 10.f };
}

// ====== Body ====== //

void UZooVRDemoDirector::LookAtPoint(const FVector& Point)
{
	bHeadTrackPoint = true;
	HeadPoint = Point;
}

void UZooVRDemoDirector::LookRelative(const float Yaw, const float Pitch)
{
	bHeadTrackPoint = false;
	HeadTarget = { Pitch, Yaw, 0.f };
}

void UZooVRDemoDirector::AimAt(const FVector& Point)
{
	if (!bAiming) SetHandPose(RightHand, TEXT("point"));
	bAiming = true;
	AimPoint = Point;
}

void UZooVRDemoDirector::StopAiming()
{
	if (bAiming) SetHandPose(RightHand, TEXT("relaxed"));
	bAiming = false;
}

float UZooVRDemoDirector::GetBodyYaw() const
{
	return VR && VR->GetVROrigin() ? VR->GetVROrigin()->GetComponentRotation().Yaw : 0.f;
}

FVector UZooVRDemoDirector::GetEyeLocation() const
{
	return VR && VR->GetVROrigin() ? VR->GetVROrigin()->GetComponentTransform().TransformPosition({ 0.0, 0.0, Script.EyeHeight }) : FVector::ZeroVector;
}

void UZooVRDemoDirector::UpdatePoses(const float DeltaTime)
{
	USceneComponent* Origin = VR->GetVROrigin();
	const APlayerController* Controller = GetController();
	IdleTime += DeltaTime;
	SnapCooldown = FMath::Max(SnapCooldown - DeltaTime, 0.f);
	RightStick.X = 0.f;

	// Head target from a tracked point: yaw relative to the body, a snap turn when it is far round
	const FVector Eye = GetEyeLocation();
	const float BodyYaw = GetBodyYaw();
	if (bHeadTrackPoint)
	{
		const FRotator World = (HeadPoint - Eye).Rotation();
		const float RelativeYaw = FRotator::NormalizeAxis(World.Yaw - BodyYaw);
		const bool bCanSnap = Controller && !Controller->IsLookInputIgnored() && SnapCooldown <= 0.f;
		if (FMath::Abs(RelativeYaw) > Script.SnapBeyond && bCanSnap)
		{
			RightStick.X = FMath::Sign(RelativeYaw);
			SnapCooldown = 0.6f;
		}
		HeadTarget = { FMath::Clamp(World.Pitch, -60.f, 55.f), FMath::Clamp(RelativeYaw, -80.f, 80.f), 0.f };
	}

	// Smooth head, with a living sway
	HeadRelative = FMath::RInterpTo(HeadRelative, HeadTarget, DeltaTime, Script.HeadSpeed);
	const FRotator Sway { 0.35f * FMath::Sin(IdleTime * 0.6f), 0.5f * FMath::Sin(IdleTime * 0.37f + 1.f), 0.2f * FMath::Sin(IdleTime * 0.5f) };
	const FRotator HeadRotation = HeadRelative + Sway;

	// Walking bob
	const ACharacter* Visitor = Cast<ACharacter>(GetPawn());
	const float Speed = Visitor ? Visitor->GetVelocity().Size2D() : 0.f;
	const float Bob = Speed > 20.f ? 1.4f * FMath::Sin(IdleTime * (Speed > 300.f ? 11.f : 8.f)) : 0.f;
	const FTransform Head { HeadRotation, FVector{ 0.f, 0.f, Script.EyeHeight + Bob } };

	// Hands in head-yaw space: at rest, or the right one up and pointing. A grip is placed through its finger frame
	// (X = fingers, Z = back of the hand), measured from the posed mesh, so the fingers go where the script says.
	const FRotator YawRotation { 0.f, BodyYaw + HeadRelative.Yaw, 0.f };
	const float Swing = Speed > 20.f ? 3.f * FMath::Sin(IdleTime * (Speed > 300.f ? 11.f : 8.f)) : 0.f;
	FVector DesiredLocation[2];
	FQuat DesiredRotation[2];
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		const float Side = Hand == RightHand ? 1.f : -1.f;
		FVector Offset = Script.RestHand;
		Offset.Y *= Side;
		Offset.X += Swing * Side + 0.6f * FMath::Sin(IdleTime * 0.8f + Hand);
		Offset.Z += 0.5f * FMath::Sin(IdleTime * 0.9f + 2.f * Hand);
		FRotator Rest = Script.RestHandRotation;
		Rest.Yaw *= Side;
		Rest.Roll *= Side;
		DesiredLocation[Hand] = Eye + YawRotation.RotateVector(Offset);
		DesiredRotation[Hand] = (YawRotation + Rest).Quaternion() * GripToFingers[Hand].Inverse();
	}
	if (bAiming)
	{
		DesiredLocation[RightHand] = Eye + YawRotation.RotateVector(Script.PointHand);
		FRotator Aim = (AimPoint - DesiredLocation[RightHand]).Rotation();
		Aim.Pitch += SweepPitch;
		Aim.Roll = Script.AimRoll;
		DesiredRotation[RightHand] = Aim.Quaternion() * GripToFingers[RightHand].Inverse();
	}

	// Glide there (snap on the first frame)
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		HandLocation[Hand] = bHandsPlaced ? FMath::VInterpTo(HandLocation[Hand], DesiredLocation[Hand], DeltaTime, Script.HandSpeed) : DesiredLocation[Hand];
		HandRotation[Hand] = bHandsPlaced ? FMath::QInterpTo(HandRotation[Hand], DesiredRotation[Hand], DeltaTime, Script.HandSpeed) : DesiredRotation[Hand];
	}
	bHandsPlaced = true;

	// The laser leaves the right index tip, along the hand's aim
	const FTransform OriginTransform = Origin->GetComponentTransform();
	const FTransform RightWorld { HandRotation[RightHand], HandLocation[RightHand] };
	const FVector Tip = bTipKnown ? RightWorld.TransformPosition(TipLocal) : RightWorld.GetLocation();
	FQuat AimRotation = HandRotation[RightHand] * GripToFingers[RightHand];
	if (bAiming) AimRotation = (AimPoint - Tip).Rotation().Quaternion() * FRotator{ SweepPitch, 0.f, 0.f }.Quaternion();
	const FTransform AimWorld { AimRotation, Tip };

	VR->SetDemoPoses(Head, FTransform{ HandRotation[LeftHand], HandLocation[LeftHand] }.GetRelativeTransform(OriginTransform), RightWorld.GetRelativeTransform(OriginTransform), AimWorld.GetRelativeTransform(OriginTransform));
}

// ====== Hand Meshes ====== //

void UZooVRDemoDirector::BuildHands()
{
	bHandsBuilt = true;
	APawn* Pawn = GetPawn();
	UMotionControllerComponent* Grips[2] { VR->GetLeftGrip(), VR->GetRightGrip() };
	USkeletalMesh* Meshes[2] { LoadObject<USkeletalMesh>(nullptr, *Script.HandMeshLeft), LoadObject<USkeletalMesh>(nullptr, *Script.HandMeshRight) };
	UClass* AnimClass = LoadClass<UAnimInstance>(nullptr, *Script.HandAnimClass);
	if (!AnimClass) UE_LOG(LogZooVRDemo, Warning, TEXT("Hand anim class not found: %s"), *Script.HandAnimClass);

	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		if (!Grips[Hand] || !Meshes[Hand])
		{
			UE_LOG(LogZooVRDemo, Warning, TEXT("Hand %d: no grip or mesh (%s)"), Hand, Hand == LeftHand ? *Script.HandMeshLeft : *Script.HandMeshRight);
			continue;
		}

		// The XR template's hand on the grip
		USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Pawn, Hand == LeftHand ? TEXT("ZooVRDemo_HandLeft") : TEXT("ZooVRDemo_HandRight"));
		Mesh->SetupAttachment(Grips[Hand]);
		Mesh->SetSkeletalMesh(Meshes[Hand]);
		if (AnimClass) Mesh->SetAnimInstanceClass(AnimClass);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetRelativeLocationAndRotation(Hand == LeftHand ? Script.LeftMeshOffset : Script.RightMeshOffset, Hand == LeftHand ? Script.LeftMeshRotation : Script.RightMeshRotation);
		Mesh->RegisterComponent();
		HandMeshes[Hand] = Mesh;

		// The index chain (root, tip) and the pinky root of this side give the finger frame (the skeleton holds both hands)
		TArray<FName> Bones;
		Mesh->GetBoneNames(Bones);
		const FString Side = Hand == LeftHand ? TEXT("_l") : TEXT("_r");
		for (const FName Bone : Bones)
		{
			const FString Name = Bone.ToString().ToLower();
			if (!Name.EndsWith(Side)) continue;

			if (Name.StartsWith(TEXT("index_01"))) IndexRootBone[Hand] = Bone;
			if (Name.StartsWith(TEXT("index_03"))) FingertipBone[Hand] = Bone;
			if (Name.StartsWith(TEXT("pinky_01"))) PinkyRootBone[Hand] = Bone;
		}
		UE_LOG(LogZooVRDemo, Log, TEXT("Hand %d: %d bones, index %s..%s, pinky %s"), Hand, Bones.Num(), *IndexRootBone[Hand].ToString(), *FingertipBone[Hand].ToString(), *PinkyRootBone[Hand].ToString());
	}

	// The pose properties the Animation Blueprint exposes (for the log, when the names differ)
	if (AnimClass)
	{
		FString Names;
		for (TFieldIterator<FProperty> It(AnimClass, EFieldIterationFlags::None); It; ++It)
		{
			Names += It->GetName() + TEXT(":") + It->GetCPPType() + TEXT(" ");
		}
		UE_LOG(LogZooVRDemo, Log, TEXT("Hand anim properties: %s"), *Names);
	}
	SetHandPose(LeftHand, TEXT("relaxed"));
	SetHandPose(RightHand, TEXT("relaxed"));
}

void UZooVRDemoDirector::UpdateHands(const float DeltaTime)
{
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		USkeletalMeshComponent* Mesh = HandMeshes[Hand];
		UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!Anim) continue;

		// Blend to the pose, then the tap / squeeze envelopes on top
		FZooVRDemoHandPose& Current = HandPoseCurrent[Hand];
		const FZooVRDemoHandPose& Target = HandPoseTarget[Hand];
		Current.Grasp = FMath::FInterpTo(Current.Grasp, Target.Grasp, DeltaTime, 10.f);
		Current.IndexCurl = FMath::FInterpTo(Current.IndexCurl, Target.IndexCurl, DeltaTime, 10.f);
		Current.Point = FMath::FInterpTo(Current.Point, Target.Point, DeltaTime, 10.f);
		Current.ThumbUp = FMath::FInterpTo(Current.ThumbUp, Target.ThumbUp, DeltaTime, 10.f);
		FZooVRDemoHandPose Shown = Current;
		if (TapTime[Hand] >= 0.f)
		{
			TapTime[Hand] += DeltaTime;
			const float Tap = TapTime[Hand] < 0.12f ? TapTime[Hand] / 0.12f : TapTime[Hand] < 0.24f ? 1.f : 1.f - FMath::Min((TapTime[Hand] - 0.24f) / 0.2f, 1.f);
			Shown.IndexCurl = FMath::Max(Shown.IndexCurl, 0.9f * Tap);
			if (TapTime[Hand] > 0.5f) TapTime[Hand] = -1.f;
		}
		if (SqueezeTime[Hand] >= 0.f)
		{
			SqueezeTime[Hand] += DeltaTime;
			const float Squeeze = SqueezeTime[Hand] < 0.12f ? SqueezeTime[Hand] / 0.12f : SqueezeTime[Hand] < 0.3f ? 1.f : 1.f - FMath::Min((SqueezeTime[Hand] - 0.3f) / 0.25f, 1.f);
			Shown.Grasp = FMath::Max(Shown.Grasp, 0.75f * Squeeze);
			if (SqueezeTime[Hand] > 0.6f) SqueezeTime[Hand] = -1.f;
		}

		// Into the Animation Blueprint (float or double variables)
		const TPair<FName, float> Values[] { { TEXT("PoseAlphaGrasp"), Shown.Grasp }, { TEXT("PoseAlphaIndexCurl"), Shown.IndexCurl }, { TEXT("PoseAlphaPoint"), Shown.Point }, { TEXT("PoseAlphaThumbUp"), Shown.ThumbUp } };
		for (const auto& [Name, Value] : Values)
		{
			const FNumericProperty* Property = FindFProperty<FNumericProperty>(Anim->GetClass(), Name);
			if (Property && Property->IsFloatingPoint()) Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Anim), Value);
		}

		// The finger frame is read while the index is (nearly) straight
		if (Shown.Grasp < 0.3f && Shown.IndexCurl < 0.3f) MeasureHand(Hand);
	}
}

void UZooVRDemoDirector::MeasureHand(const int32 Hand)
{
	const USkeletalMeshComponent* Mesh = HandMeshes[Hand];
	const UMotionControllerComponent* Grip = Hand == RightHand ? VR->GetRightGrip() : VR->GetLeftGrip();
	if (!Mesh || !Grip || IndexRootBone[Hand].IsNone() || FingertipBone[Hand].IsNone() || PinkyRootBone[Hand].IsNone()) return;

	// Finger frame in the grip's space: X along the index, Z out of the back of the hand
	const FTransform GripTransform = Grip->GetComponentTransform();
	const FVector IndexRoot = GripTransform.InverseTransformPosition(Mesh->GetBoneLocation(IndexRootBone[Hand]));
	const FVector IndexTip = GripTransform.InverseTransformPosition(Mesh->GetBoneLocation(FingertipBone[Hand]));
	const FVector PinkyRoot = GripTransform.InverseTransformPosition(Mesh->GetBoneLocation(PinkyRootBone[Hand]));
	const FVector Fingers = (IndexTip - IndexRoot).GetSafeNormal();
	const FVector Across = (PinkyRoot - IndexRoot).GetSafeNormal();
	if (Fingers.IsNearlyZero() || Across.IsNearlyZero()) return;

	// Pinky is to the right of the index on the right hand, to the left on the left one
	const FVector Back = (Hand == RightHand ? FVector::CrossProduct(Fingers, Across) : FVector::CrossProduct(Across, Fingers)).GetSafeNormal();
	GripToFingers[Hand] = FRotationMatrix::MakeFromXZ(Fingers, Back).ToQuat();
	bFingersKnown[Hand] = true;

	// The right tip is the laser's origin
	if (Hand == RightHand)
	{
		TipLocal = IndexTip;
		bTipKnown = true;
	}
}

void UZooVRDemoDirector::SetHandPose(const int32 Hand, const FString& Pose)
{
	FZooVRDemoHandPose& Target = HandPoseTarget[Hand];
	Target = FZooVRDemoHandPose{};
	if (Pose == TEXT("relaxed"))
	{
		Target.Grasp = 0.25f;
		Target.IndexCurl = 0.2f;
	}
	else if (Pose == TEXT("point")) Target.Point = 1.f;
	else if (Pose == TEXT("grab")) Target.Grasp = 1.f;
	else if (Pose == TEXT("fist"))
	{
		Target.Grasp = 1.f;
		Target.IndexCurl = 1.f;
	}
}

void UZooVRDemoDirector::PlayTap(const int32 Hand)
{
	TapTime[Hand] = 0.f;
}

void UZooVRDemoDirector::PlaySqueeze(const int32 Hand)
{
	SqueezeTime[Hand] = 0.f;
}

FVector UZooVRDemoDirector::GetFingertip(const int32 Hand) const
{
	const USkeletalMeshComponent* Mesh = HandMeshes[Hand];
	if (!Mesh) return FVector::ZeroVector;

	return FingertipBone[Hand].IsNone() ? Mesh->GetComponentLocation() : Mesh->GetBoneLocation(FingertipBone[Hand]);
}

// ====== Input ====== //

FKey UZooVRDemoDirector::KeyForAction(const FName ActionName) const
{
	const UInputMappingContext* Context = VR ? VR->GetVRMappingContext() : nullptr;
	if (!Context) return EKeys::Invalid;

	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		if (Mapping.Action && Mapping.Action->GetFName() == ActionName) return Mapping.Key;
	}

	return EKeys::Invalid;
}

void UZooVRDemoDirector::SetButton(const FString& Button, const bool bDown)
{
	if (bDown == HeldButtons.Contains(Button)) return;

	// The Quest key the VR context binds to the feature
	FKey Key = EKeys::Invalid;
	if (Button == TEXT("select")) Key = KeyForAction(VR && VR->GetSelectAction() ? VR->GetSelectAction()->GetFName() : FName{ TEXT("IA_ZooVR_Select") });
	else if (Button == TEXT("menu")) Key = KeyForAction(VR && VR->GetMenuAction() ? VR->GetMenuAction()->GetFName() : FName{ TEXT("IA_ZooVR_Menu") });
	else if (Button == TEXT("close")) Key = KeyForAction(TEXT("IA_PG_CloseMenu"));
	else if (Button == TEXT("sprint")) Key = KeyForAction(TEXT("IA_Sprint"));
	else if (Button.StartsWith(TEXT("key:"))) Key = FKey{ *Button.Mid(4) };
	if (!Key.IsValid())
	{
		UE_LOG(LogZooVRDemo, Warning, TEXT("No key for button '%s'"), *Button);

		return;
	}

	SendKey(Key, bDown);
	if (bDown) HeldButtons.Add(Button);
	else HeldButtons.Remove(Button);
}

void UZooVRDemoDirector::SendKey(const FKey& Key, const bool bDown) const
{
	APlayerController* Controller = GetController();
	if (Controller) Controller->InputKey(FInputKeyEventArgs{ nullptr, FInputDeviceId::CreateFromInternalId(0), Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f, false, FPlatformTime::Cycles64() });
}

void UZooVRDemoDirector::SendAxis(const FKey& Key, const float Value) const
{
	APlayerController* Controller = GetController();
	if (Controller && Key.IsValid()) Controller->InputKey(FInputKeyEventArgs{ nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Value, GetWorld()->GetDeltaSeconds(), 1, FPlatformTime::Cycles64() });
}

void UZooVRDemoDirector::SendSticks()
{
	// Left stick: the 2D key's paired X / Y axes, like the runtime sends them
	const FKey MoveKey = KeyForAction(VR && VR->GetMoveAction() ? VR->GetMoveAction()->GetFName() : FName{ TEXT("IA_ZooVR_Move") });
	if (MoveKey.IsValid())
	{
		const FString Base = MoveKey.GetFName().ToString().Replace(TEXT("_2D"), TEXT(""));
		SendAxis(FKey{ *(Base + TEXT("_X")) }, LeftStick.X);
		SendAxis(FKey{ *(Base + TEXT("_Y")) }, LeftStick.Y);
	}

	// Right stick: snap turn (X) and scroll (Y) axes
	SendAxis(KeyForAction(VR && VR->GetSnapTurnAction() ? VR->GetSnapTurnAction()->GetFName() : FName{ TEXT("IA_ZooVR_SnapTurn") }), RightStick.X);
	SendAxis(KeyForAction(VR && VR->GetScrollAction() ? VR->GetScrollAction()->GetFName() : FName{ TEXT("IA_ZooVR_Scroll") }), RightStick.Y);
}

// ====== Walking ====== //

void UZooVRDemoDirector::PlanWalk(const FVector& Goal, const bool bNav)
{
	Waypoints.Reset();
	WaypointIndex = 0;
	StuckTime = 0.f;
	LastPawnLocation = GetPawn()->GetActorLocation();

	// NavMesh corners, else a straight line
	if (bNav)
	{
		const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), LastPawnLocation, Goal, GetPawn());
		if (Path && Path->IsValid() && Path->PathPoints.Num() >= 2)
		{
			Waypoints = Path->PathPoints;
			Waypoints.RemoveAt(0);
			// Nav paths end on the mesh: finish at the asked point
			Waypoints.Last() = Goal;
		}
	}
	if (Waypoints.IsEmpty()) Waypoints.Add(Goal);
	UE_LOG(LogZooVRDemo, Log, TEXT("walk: %d waypoints to %s"), Waypoints.Num(), *Goal.ToCompactString());
}

bool UZooVRDemoDirector::TickWalk(const float DeltaTime, const float Radius, const bool bLookAhead)
{
	const APawn* Pawn = GetPawn();
	const APlayerController* Controller = GetController();
	if (!Pawn || !Controller || !Waypoints.IsValidIndex(WaypointIndex)) return true;

	// Next corner
	const FVector Here = Pawn->GetActorLocation();
	const bool bLast = WaypointIndex == Waypoints.Num() - 1;
	const float Distance = FVector::Dist2D(Here, Waypoints[WaypointIndex]);
	if (Distance < (bLast ? Radius : 70.f))
	{
		++WaypointIndex;
		if (WaypointIndex >= Waypoints.Num())
		{
			LeftStick = FVector2D::ZeroVector;

			return true;
		}
	}

	// Stuck: skip the corner
	StuckTime = FVector::Dist2D(Here, LastPawnLocation) < 2.f ? StuckTime + DeltaTime : 0.f;
	LastPawnLocation = Here;
	if (StuckTime > 2.5f)
	{
		UE_LOG(LogZooVRDemo, Warning, TEXT("walk: stuck, skipping waypoint %d"), WaypointIndex);
		StuckTime = 0.f;
		++WaypointIndex;
		if (WaypointIndex >= Waypoints.Num()) return true;
	}

	// Stick in the head-yaw frame (the walk follows the head), eased near the goal
	const FVector Direction = (Waypoints[WaypointIndex] - Here).GetSafeNormal2D();
	const FRotationMatrix Yaw { FRotator{ 0.f, Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetCameraRotation().Yaw : GetBodyYaw(), 0.f } };
	const float Magnitude = bLast && Distance < 150.f ? 0.6f : 1.f;
	LeftStick = FVector2D{ FVector::DotProduct(Direction, Yaw.GetUnitAxis(EAxis::Y)), FVector::DotProduct(Direction, Yaw.GetUnitAxis(EAxis::X)) } * Magnitude;

	// Eyes ahead
	if (bLookAhead) LookAtPoint(Here + Direction * 600.f + FVector{ 0.f, 0.f, Script.EyeHeight - 40.f });

	return false;
}

// ====== Capture ====== //

void UZooVRDemoDirector::CaptureFrame()
{
	if (!bCapturing) return;

	// The render target only (no Slate, no cursor); the pixels arrive in HandleScreenshotCaptured
	FScreenshotRequest::RequestScreenshot(FramesDir / TEXT("frame"), false, false);
}

void UZooVRDemoDirector::HandleScreenshotCaptured(const int32 Width, const int32 Height, const TArray<FColor>& Colors)
{
	// Opaque copy for the writer threads
	TArray64<FColor> Pixels;
	Pixels.Append(Colors.GetData(), Colors.Num());
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	TUniquePtr<FImageWriteTask> Task = MakeUnique<FImageWriteTask>();
	Task->Format = EImageFormat::JPEG;
	Task->CompressionQuality = 95;
	Task->bOverwriteFile = true;
	Task->Filename = FramesDir / FString::Printf(TEXT("frame_%06d.jpg"), FrameIndex);
	Task->PixelData = MakeUnique<TImagePixelData<FColor>>(FIntPoint{ Width, Height }, MoveTemp(Pixels));
	FModuleManager::LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue().Enqueue(MoveTemp(Task));
	++FrameIndex;
}

void UZooVRDemoDirector::WriteMarker(const FString& Name)
{
	const FString Line = FString::Printf(TEXT("%d\t%.2f\t%.2f\t%s"), FrameIndex, FrameIndex / CaptureFps, GetWorld()->GetTimeSeconds(), *Name);
	Markers.Add(Line);
	UE_LOG(LogZooVRDemo, Log, TEXT("marker %s"), *Line);
}

// ====== State ====== //

void UZooVRDemoDirector::HandleArrived(const FGameplayTag& PlaceTag)
{
	bArrived = true;
	UE_LOG(LogZooVRDemo, Log, TEXT("arrived at %s"), *PlaceTag.ToString());
}

APlayerController* UZooVRDemoDirector::GetController() const
{
	return GetOwner<APlayerController>();
}

APawn* UZooVRDemoDirector::GetPawn() const
{
	const APlayerController* Controller = GetController();

	return Controller ? Controller->GetPawn() : nullptr;
}

bool UZooVRDemoDirector::IsDialogueActive() const
{
	const APlayerController* Controller = GetController();
	const UWzDialogueSubsystem* Dialogue = Controller && Controller->GetLocalPlayer() ? Controller->GetLocalPlayer()->GetSubsystem<UWzDialogueSubsystem>() : nullptr;

	return Dialogue && Dialogue->IsDialogueActive();
}

bool UZooVRDemoDirector::IsDialogueTyping() const
{
	const APlayerController* Controller = GetController();
	const UWzDialogueSubsystem* Dialogue = Controller && Controller->GetLocalPlayer() ? Controller->GetLocalPlayer()->GetSubsystem<UWzDialogueSubsystem>() : nullptr;
	const UWzDialogueAsset* Asset = Dialogue ? Dialogue->GetActiveDialogue() : nullptr;
	if (!Asset || !Asset->Lines.IsValidIndex(Dialogue->GetLineIndex())) return false;

	return Dialogue->GetRevealedChars() < Asset->Lines[Dialogue->GetLineIndex()].Text.ToString().Len();
}
