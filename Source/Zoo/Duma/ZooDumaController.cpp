// Copyright Epic Games, Inc. All Rights Reserved.

#include "Duma/ZooDumaController.h"

#include "Camera/PlayerCameraManager.h"
#include "Dialogue/Camera/WzDialogueCameraEffect.h"
#include "CollisionQueryParams.h"
#include "Components/SphereComponent.h"
#include "Dialogue/Data/WzDialogueAsset.h"
#include "Dialogue/WzBarkComponent.h"
#include "Duma/Behavior/ZooDumaMotion.h"
#include "Duma/Behavior/ZooDumaShowComponent.h"
#include "UI/ZooViewClutter.h"
#include "Duma/ZooDuma.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Hologram/ZooHologramComponent.h"
#include "Dialogue/WzDialogueSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogZooDuma, Log, All);

// ====== Lifecycle ====== //

AZooDumaController::AZooDumaController()
{
	Show = CreateDefaultSubobject<UZooDumaShowComponent>(TEXT("Show"));
	Bark = CreateDefaultSubobject<UWzBarkComponent>(TEXT("Bark"));
}

void AZooDumaController::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	AZooDuma* Duma = GetPawn<AZooDuma>();
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!Duma || !PC || bDespawned) return;

	// The follow spot, in the visitor's view, clear of the HUD.
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Spot = CamLoc + CamRot.RotateVector(KeepClearOfHUD(FollowOffset, PC));
	const FVector Loc = Duma->GetActorLocation();
	const bool bFacing = Duma->GetState() == EZooDumaState::Facing;

	// First tick: start right at the spot (at the front when a dialogue already asked) and appear there, slowly when the quest says so.
	if (!bPlaced)
	{
		Relocate(Duma, bFacing ? CamLoc + CamRot.RotateVector(KeepClearOfHUD(FacingOffset, PC)) : Spot);
		SnapTurn(Duma);
		bPlaced = true;
		bSpawnAppearing = true;
		Duma->GetHologram()->Appear(SpawnAppearSeconds);

		return;
	}

	// Move: Flying glides to the spot, Facing hovers. The step shrinks the distance by exp(-k dt), so it never overshoots, however long the frame.
	UPawnMovementComponent* Movement = Duma->GetMovementComponent();
	if (!bFacing && Movement)
	{
		const FVector Step = ((Spot - Loc) * (1.f - FMath::Exp(-FollowStiffness * DeltaTime))).GetClampedToMaxSize(Movement->GetMaxSpeed() * DeltaTime);
		FHitResult Hit;
		Movement->SafeMoveUpdatedComponent(Step, Duma->GetActorQuat(), true, Hit);
		if (Hit.IsValidBlockingHit()) Movement->SlideAlongSurface(Step, 1.f - Hit.Time, Hit.Normal, Hit);
	}

	// Turn
	Duma->SetActorRotation(FMath::RInterpTo(Duma->GetActorRotation(), GetWantedRotation(Duma, CamLoc, CamRot), DeltaTime, TurnSpeed));

	// Perform: drift and showpieces on the mesh.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZooDumaLost), false, Duma);
	Params.AddIgnoredActor(PC->GetPawn());
	Perform(Duma, PC, CamLoc, CamRot, Params, DeltaTime);

	// Bark: only while flying, settled and visible.
	Bark->SetAllowed(!bFacing && !bSpawnAppearing && Blink == EZooDumaBlink::None && Duma->GetHologram()->GetPhase() == EZooHologramPhase::Shown);

	// Facing, blinking or still appearing: never lost.
	if (bFacing || bSpawnAppearing || Blink != EZooDumaBlink::None)
	{
		LostTime = 0.f;

		return;
	}

	// Lost: out of the view cone, behind geometry, or too far.
	const FVector ToDuma = Loc - CamLoc;
	const bool bOutOfCone = FVector::DotProduct(ToDuma.GetSafeNormal(), CamRot.Vector()) < FMath::Cos(FMath::DegreesToRadians(ViewHalfAngle));
	const bool bBehindGeometry = World->LineTraceTestByChannel(CamLoc, Loc, ECC_Visibility, Params);
	const bool bLost = bOutOfCone || bBehindGeometry || ToDuma.SizeSquared() > FMath::Square(LostDistance);
	LostTime = bLost ? LostTime + DeltaTime : 0.f;
	if (LostTime <= LostSeconds) return;

	// Lost for too long: jump back to the spot when it is free, behind a glitch.
	const USphereComponent* Sphere = Cast<USphereComponent>(Duma->GetRootComponent());
	const float Radius = Sphere ? Sphere->GetScaledSphereRadius() : 25.f;
	if (World->OverlapBlockingTestByChannel(Spot, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params)) return;

	Duma->GetHologram()->Glitch();
	Relocate(Duma, Spot);
	SnapTurn(Duma);
	LostTime = 0.f;
}

void AZooDumaController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Blink = EZooDumaBlink::None;
	bBlinkOut = false;
	bSpawnAppearing = false;
	bPlaced = false;
	bDespawned = false;
	bHasLast = false;
	LostTime = 0.f;

	// His lines and the end of the dialogue drive facing and the glow.
	if (UWzDialogueSubsystem* Dialogue = UWzDialogueSubsystem::Get(this))
	{
		Dialogue->OnLineStarted.AddUObject(this, &AZooDumaController::HandleLineStarted);
		Dialogue->OnDialogueEnded.AddUObject(this, &AZooDumaController::HandleDialogueEnded);
	}

	// The hologram tells him when he is back and when he is gone.
	if (const AZooDuma* Duma = Cast<AZooDuma>(InPawn))
	{
		Duma->GetHologram()->OnAppeared.AddUObject(this, &AZooDumaController::HandleAppeared);
		Duma->GetHologram()->OnDisappeared.AddUObject(this, &AZooDumaController::HandleDisappeared);
	}

	// The show asks for a peek's blink and for the sparkle burst.
	Show->OnPeekBlink.AddWeakLambda(this, [this]
	{
		BeginBlink(EZooDumaBlink::Peek);
	});
	Show->OnBurst.AddWeakLambda(this, [this]
	{
		if (const AZooDuma* Duma = GetPawn<AZooDuma>()) Duma->PlaySparkleBurst();
	});
}

void AZooDumaController::OnUnPossess()
{
	if (UWzDialogueSubsystem* Dialogue = UWzDialogueSubsystem::Get(this))
	{
		Dialogue->OnLineStarted.RemoveAll(this);
		Dialogue->OnDialogueEnded.RemoveAll(this);
	}
	if (const AZooDuma* Duma = GetPawn<AZooDuma>())
	{
		Duma->GetHologram()->OnAppeared.RemoveAll(this);
		Duma->GetHologram()->OnDisappeared.RemoveAll(this);
	}
	Show->OnPeekBlink.RemoveAll(this);
	Show->OnBurst.RemoveAll(this);

	Super::OnUnPossess();
}

// ====== API ====== //

void AZooDumaController::Despawn()
{
	if (bDespawned) return;

	// A bark already running would keep talking from an invisible Duma
	if (UWzDialogueSubsystem* Dialogue = UWzDialogueSubsystem::Get(this)) Dialogue->EndAmbientDialogue();

	BeginBlink(EZooDumaBlink::Despawn);
}

void AZooDumaController::Spawn(const float AppearSeconds)
{
	AZooDuma* Duma = GetPawn<AZooDuma>();
	if (!Duma) return;

	// Still dematerializing: he rematerializes from where the line is
	if (!bDespawned)
	{
		if (Blink != EZooDumaBlink::Despawn) return;

		Blink = EZooDumaBlink::None;
		bBlinkOut = false;
		bSpawnAppearing = true;
		Duma->GetHologram()->Appear(AppearSeconds);

		return;
	}

	// Hidden: shown again, the next tick places him at the follow spot and starts the appear
	bDespawned = false;
	bBlinkOut = false;
	bPlaced = false;
	bHasLast = false;
	LostTime = 0.f;
	SpawnAppearSeconds = AppearSeconds;
	Duma->SetState(EZooDumaState::Flying);
	Duma->SetActorHiddenInGame(false);
	Duma->SetActorEnableCollision(true);
}

// ====== Blink ====== //

void AZooDumaController::BeginBlink(const EZooDumaBlink Purpose)
{
	// Nothing outranks a despawn; a newer purpose replaces a pending one
	if (!GetPawn<AZooDuma>() || Blink == EZooDumaBlink::Despawn) return;

	Blink = Purpose;

	// Out now, unless one is under way, or the slow first appear is still running (a despawn cuts even that)
	if (bBlinkOut) return;
	if (bSpawnAppearing && Purpose != EZooDumaBlink::Despawn) return;

	StartBlinkOut();
}

void AZooDumaController::StartBlinkOut()
{
	AZooDuma* Duma = GetPawn<AZooDuma>();
	if (!Duma) return;

	// Flag first: the hologram may answer at once. A blink is fast; a despawn plays the profile's full dematerialize.
	bBlinkOut = true;
	Duma->SetRibbons(false, true);
	const bool bDespawn = Blink == EZooDumaBlink::Despawn;
	Duma->GetHologram()->Disappear(bDespawn ? -1.f : BlinkOutSeconds, bDespawn);
}

void AZooDumaController::Relocate(AZooDuma* Duma, const FVector& Where)
{
	// Ribbons off first, so no trail is drawn across the jump
	Duma->SetRibbons(false, true);
	Duma->SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
	bHasLast = false;
}

// ====== Performance ====== //

void AZooDumaController::Perform(AZooDuma* Duma, const APlayerController* PC, const FVector& CamLoc, const FRotator& CamRot, const FCollisionQueryParams& Params, const float DeltaTime)
{
	// Allowed while flying and settled; a peek's own blink keeps it going
	const bool bPeeking = Blink == EZooDumaBlink::Peek;
	Show->SetAllowed(Duma->GetState() == EZooDumaState::Flying && !bSpawnAppearing && (Blink == EZooDumaBlink::None || bPeeking));

	// The pose, kept inside the real screen; while he faces the visitor a gentle hover sway keeps him alive
	FZooDumaPose Pose = Show->GetPose(FollowOffset);
	if (Duma->GetState() == EZooDumaState::Facing)
	{
		const float Time = GetWorld()->GetTimeSeconds();
		Pose.Offset += FVector{ 0.f, FacingSway.Y * FMath::Sin(Time * 0.7f), FacingSway.Z * FMath::Sin(Time * 1.1f + 1.f) };
		Pose.Rotation.Roll += 3.f * FMath::Sin(Time * 0.7f);
	}
	int32 SizeX = 0;
	int32 SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	const float Aspect = SizeY > 0 ? static_cast<float>(SizeX) / SizeY : 16.f / 9.f;
	const float Fov = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 90.f;
	const FVector ViewOffset = ZooDuma::ClampToView(FollowOffset + Pose.Offset, Fov, Aspect, MinAhead) - FollowOffset;
	const FVector Loc = Duma->GetActorLocation();
	const FVector WorldOffset = CamRot.RotateVector(ViewOffset);
	const FVector Posed = Loc + WorldOffset;

	// A blocked view cuts a showpiece short
	if (Show->IsPerforming() && GetWorld()->LineTraceTestByChannel(CamLoc, Posed, ECC_Visibility, Params)) Show->Abort();

	// The actor's lag, for the lean
	const FVector ActorVelocity = bHasLast ? (Loc - LastLoc) / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER) : FVector::ZeroVector;
	LastLoc = Loc;
	bHasLast = true;

	// Lean into the sideways lag
	const float Lateral = FVector::DotProduct(ActorVelocity, CamRot.RotateVector(FVector::RightVector));
	Bank = FMath::FInterpTo(Bank, FMath::Clamp(Lateral * BankPerSpeed, -MaxBank, MaxBank), DeltaTime, 6.f);

	// The pose into the actor's space: a view-space rotation is turned by the camera, then undone by the actor
	FRotator ViewRotation = Pose.Rotation;
	ViewRotation.Roll += Bank;
	const FQuat Cam = CamRot.Quaternion();
	const FQuat Actor = Duma->GetActorQuat();
	const FQuat LocalRotation = Actor.Inverse() * Cam * ViewRotation.Quaternion() * Cam.Inverse() * Actor;
	Duma->SetPerformPose(Actor.UnrotateVector(WorldOffset), LocalRotation);

	// No trail while he glances back at the visitor (the rest is his own presentation)
	Duma->SetTrailAllowed(!Show->IsGlancing());
}

// ====== Dialogue ====== //

void AZooDumaController::SetFacing(const bool bFacing)
{
	AZooDuma* Duma = GetPawn<AZooDuma>();
	if (!Duma || bDespawned) return;

	// Not placed yet: he simply appears in the right state
	const EZooDumaState Want = bFacing ? EZooDumaState::Facing : EZooDumaState::Flying;
	if (!bPlaced)
	{
		Duma->SetState(Want);

		return;
	}

	// Already on his way there
	const EZooDumaBlink Purpose = bFacing ? EZooDumaBlink::ToFacing : EZooDumaBlink::ToFlying;
	if (Blink == Purpose) return;

	// Already there: nothing to do, unless the opposite blink is pending (it is cancelled) or running (it is redirected)
	const bool bThere = Duma->GetState() == Want;
	if (bThere && Blink != EZooDumaBlink::ToFacing && Blink != EZooDumaBlink::ToFlying) return;
	if (bThere && !bBlinkOut)
	{
		Blink = EZooDumaBlink::None;

		return;
	}

	BeginBlink(Purpose);
}

void AZooDumaController::SetSpeaking(const bool bSpeaking)
{
	if (const AZooDuma* Duma = GetPawn<AZooDuma>()) Duma->GetHologram()->SetSpeaking(bSpeaking);
}

void AZooDumaController::HandleLineStarted(const int32 LineIndex)
{
	UWzDialogueSubsystem* Dialogue = UWzDialogueSubsystem::Get(this);
	const UWzDialogueAsset* Asset = Dialogue ? Dialogue->GetActiveDialogue() : nullptr;
	if (!Asset || !Asset->Lines.IsValidIndex(LineIndex)) return;

	// He glows for any line of his. His own line with a camera effect (the visitor is being turned toward him) brings him to the front, where he stays until the dialogue ends.
	const FWzDialogueLine& Line = Asset->Lines[LineIndex];
	const bool bHisLine = Dialogue->FindSpeakerActor(Line.Speaker) == GetPawn();
	SetSpeaking(bHisLine);
	if (bHisLine && Line.CameraEffect) SetFacing(true);
}

void AZooDumaController::HandleDialogueEnded(UWzDialogueAsset* Dialogue)
{
	SetFacing(false);
	SetSpeaking(false);
}

// ====== Hologram ====== //

void AZooDumaController::HandleAppeared()
{
	bSpawnAppearing = false;

	// A blink asked for meanwhile starts now (it also cuts a blink-in)
	if (Blink != EZooDumaBlink::None)
	{
		if (!bBlinkOut) StartBlinkOut();

		return;
	}
}

void AZooDumaController::HandleDisappeared()
{
	bBlinkOut = false;
	AZooDuma* Duma = GetPawn<AZooDuma>();

	if (!Duma) return;

	// Away until Spawn: hidden, solid-less and silent, the brain paused
	if (Blink == EZooDumaBlink::Despawn)
	{
		Blink = EZooDumaBlink::None;
		bDespawned = true;
		Duma->SetActorHiddenInGame(true);
		Duma->SetActorEnableCollision(false);
		Bark->SetAllowed(false);
		Show->SetAllowed(false);

		return;
	}

	// Only a despawn removes him: an unexplained disappear brings him back
	const EZooDumaBlink Purpose = Blink;
	Blink = EZooDumaBlink::None;
	if (Purpose == EZooDumaBlink::None) UE_LOG(LogZooDuma, Warning, TEXT("Duma disappeared without a reason: bringing him back"));

	// A peek only moves his mesh while he is hidden
	if (Purpose == EZooDumaBlink::Peek) Show->NotifyBlinkHidden();

	// A dialogue line brings him to the front, its end back to the spot
	if (Purpose == EZooDumaBlink::ToFacing || Purpose == EZooDumaBlink::ToFlying)
	{
		const bool bToFacing = Purpose == EZooDumaBlink::ToFacing;
		FVector Where;
		if (GetViewSpot(bToFacing ? FacingOffset : FollowOffset, Where)) Relocate(Duma, Where);

		Duma->SetState(bToFacing ? EZooDumaState::Facing : EZooDumaState::Flying);
		SnapTurn(Duma);
		LostTime = 0.f;
		UE_LOG(LogZooDuma, Log, TEXT("Duma -> %s"), bToFacing ? TEXT("Facing") : TEXT("Flying"));
	}

	Duma->GetHologram()->Appear(BlinkInSeconds);
}

// ====== View ====== //

bool AZooDumaController::GetViewSpot(const FVector& ViewOffset, FVector& OutSpot)
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC) return false;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	OutSpot = CamLoc + CamRot.RotateVector(KeepClearOfHUD(ViewOffset, PC));

	return true;
}

FVector AZooDumaController::KeepClearOfHUD(const FVector& ViewOffset, const APlayerController* PC)
{
	TArray<FZooViewRect> Rects;
	FZooViewClutter::Query(PC, Rects);
	if (Rects.IsEmpty()) return ViewOffset;

	// Him as a disc in view angles
	const float Distance = ViewOffset.Size();
	const FVector2D Base = FZooViewClutter::AnglesOf(ViewOffset);
	const float Radius = FMath::RadiansToDegrees(FMath::Atan2(ClearRadius, Distance)) + ClearMargin;
	const auto IsFree = [&Rects, Radius](const FVector2D& Centre)
	{
		return !Rects.ContainsByPredicate([&Centre, Radius](const FZooViewRect& Rect) { return Rect.Touches(Centre, Radius); });
	};

	// Where he belongs first, then where he last went, then sideways at eye height, only then a little up, further and further
	static const FVector2D Shifts[] { { 0.f, 0.f }, { -8.f, 0.f }, { 8.f, 0.f }, { -16.f, 0.f }, { 16.f, 0.f }, { 0.f, 6.f }, { -8.f, 6.f }, { 8.f, 6.f }, { -24.f, 0.f }, { 24.f, 0.f }, { -16.f, 6.f }, { 16.f, 6.f }, { 0.f, 12.f }, { -24.f, 8.f }, { 24.f, 8.f }, { 0.f, -8.f }, { -16.f, -8.f }, { 16.f, -8.f }, { -32.f, 0.f }, { 32.f, 0.f }, { 0.f, 18.f } };
	TArray<FVector2D, TInlineAllocator<24>> Order;
	Order.Add(FVector2D::ZeroVector);
	Order.Add(LastClearShift);
	Order.Append(Shifts);
	for (const FVector2D& Shift : Order)
	{
		if (!IsFree(Base + Shift)) continue;

		LastClearShift = Shift;

		return FZooViewClutter::DirectionOf(Base + Shift) * Distance;
	}

	return ViewOffset;
}

FRotator AZooDumaController::GetWantedRotation(const AZooDuma* Duma, const FVector& CamLoc, const FRotator& CamRot) const
{
	const bool bFacing = Duma->GetState() == EZooDumaState::Facing;

	return { 0.f, bFacing ? (CamLoc - Duma->GetActorLocation()).Rotation().Yaw : CamRot.Yaw, 0.f };
}

void AZooDumaController::SnapTurn(AZooDuma* Duma) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	Duma->SetActorRotation(GetWantedRotation(Duma, CamLoc, CamRot));
}