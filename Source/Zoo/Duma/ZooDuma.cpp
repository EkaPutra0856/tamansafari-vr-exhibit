// Copyright Epic Games, Inc. All Rights Reserved.

#include "Duma/ZooDuma.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Duma/ZooDumaController.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Hologram/ZooHologramComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

static const FName RibbonIntensityParameter(TEXT("User.Intensity"));
static const FName RibbonTrailParameter(TEXT("User.TrailDirection"));

// ====== Lifecycle ====== //

AZooDuma::AZooDuma()
{
	// Sphere root: blocks the world, ignores other pawns so the visitor is never pushed.
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(25.f);
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	SetRootComponent(Collision);

	// SK_Duma is ~100 tall with its pivot at the feet: half scale, lowered so its centre sits on the actor. His face is not on mesh +X: yawed so it looks along the actor's +X (tuned on BP_Duma).
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector{ 0.5f });
	Mesh->SetRelativeLocation(FVector{ 0.f, 0.f, -24.5f });
	Mesh->SetRelativeRotation(FRotator{ 0.f, -115.f, 0.f });
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	// Twin ribbons from the ear tips: off until the controller turns them on.
	RibbonLeft = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RibbonLeft"));
	RibbonLeft->SetupAttachment(Mesh, TEXT("DEF_EarTip_L"));
	RibbonLeft->SetAutoActivate(false);
	RibbonRight = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RibbonRight"));
	RibbonRight->SetupAttachment(Mesh, TEXT("DEF_EarTip_R"));
	RibbonRight->SetAutoActivate(false);

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->MaxSpeed = 1200.f;
	Movement->Acceleration = 2000.f;
	Movement->Deceleration = 4000.f;

	// The AR look of the mesh.
	Hologram = CreateDefaultSubobject<UZooHologramComponent>(TEXT("Hologram"));

	// The camera-look effect aims at the eye point: the centre of Duma, not 64 above him.
	BaseEyeHeight = 0.f;
	bUseControllerRotationYaw = false;
	AIControllerClass = AZooDumaController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// He ticks only for his own presentation (the ribbons): the controller moves him.
	PrimaryActorTick.bCanEverTick = true;
}

void AZooDuma::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// The Blueprint's mesh placement, before any pose moves it.
	MeshBase = Mesh->GetRelativeTransform();

	// Before any BeginPlay, so the hologram hides him from the very first frame.
	Hologram->SetTarget(Mesh);
}

void AZooDuma::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	TickRibbons(DeltaTime);
}

// ====== Ribbons ====== //

void AZooDuma::SetRibbons(const bool bOn, const bool bClear)
{
	// Clearing is a jump or a restart: his speed must not count it, and the live trail stays behind to fade where it was
	if (bClear) bHasLastMesh = false;
	if (bClear) ReleaseRibbons();
	SwitchRibbons(bOn, false);
}

void AZooDuma::ReleaseRibbons()
{
	for (TObjectPtr<UNiagaraComponent>* Slot : { &RibbonLeft, &RibbonRight })
	{
		UNiagaraComponent* Old = Slot->Get();
		if (!Old) continue;

		// Fresh ribbons on the same ear (a copy keeps the asset and its user parameters), not yet emitting
		UNiagaraComponent* Fresh = DuplicateObject<UNiagaraComponent>(Old, this);
		Fresh->SetAutoActivate(false);
		Fresh->RegisterComponent();
		Fresh->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform, Old->GetAttachSocketName());
		*Slot = Fresh;

		// The old trail is left in the world to finish, then thrown away
		if (!Old->IsActive()) Old->DestroyComponent();
		else
		{
			Old->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Old->Deactivate();
			FTimerHandle Discard;
			GetWorldTimerManager().SetTimer(Discard, FTimerDelegate::CreateWeakLambda(Old, [Old]
			{
				Old->DestroyComponent();
			}), RibbonFadeSeconds, false);
		}
	}
}

void AZooDuma::SwitchRibbons(const bool bOn, const bool bClear) const
{
	// On restarts the system when clearing, so no old trail joins the new one.
	for (const auto Ribbon : { RibbonLeft.Get(), RibbonRight.Get() })
	{
		if (bOn) Ribbon->Activate(bClear);
		else if (bClear) Ribbon->DeactivateImmediate();
		else Ribbon->Deactivate();
	}
}

void AZooDuma::SetRibbonIntensity(const float Intensity) const
{
	for (const auto Ribbon : { RibbonLeft.Get(), RibbonRight.Get() }) Ribbon->SetVariableFloat(RibbonIntensityParameter, Intensity);
}

void AZooDuma::UpdateRibbonTrail() const
{
	// Behind him, level, and his right
	const FVector Back = -GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(Back, FVector::UpVector);

	// Each ear fans away from the other and sags
	const FVector Sag = FVector::DownVector * RibbonSag;
	RibbonLeft->SetVariableVec3(RibbonTrailParameter, (Back - Right * RibbonOutward + Sag).GetSafeNormal());
	RibbonRight->SetVariableVec3(RibbonTrailParameter, (Back + Right * RibbonOutward + Sag).GetSafeNormal());
}

void AZooDuma::TickRibbons(const float DeltaTime)
{
	// His speed: the posed mesh, drift and showpieces included
	const FVector MeshLocation = Mesh->GetComponentLocation();
	const float Speed = bHasLastMesh ? FVector::Dist(MeshLocation, LastMeshLocation) / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER) : 0.f;
	LastMeshLocation = MeshLocation;
	bHasLastMesh = true;

	// His pace, remembered for a moment: a stop-and-go walk keeps the trail, a real stop lets it fade
	RibbonHold = Speed >= RibbonMinSpeed ? RibbonHoldSeconds : FMath::Max(RibbonHold - DeltaTime, 0.f);
	RibbonPace = FMath::Max(Speed, FMath::FInterpTo(RibbonPace, Speed, DeltaTime, 2.f));

	// Only while he moves (or just did), is allowed, and is fully there
	const bool bTrail = bTrailAllowed && RibbonHold > 0.f && Hologram->GetPhase() == EZooHologramPhase::Shown;
	RibbonStrength = FMath::FInterpTo(RibbonStrength, bTrail ? FMath::Clamp(RibbonPace / RibbonFullSpeed, 0.15f, 1.f) : 0.f, DeltaTime, bTrail ? 6.f : 2.5f);

	// Switched on (resumed, so a short pause never breaks the line); switched off gently (the live trail finishes) once faded
	const bool bOn = RibbonLeft->IsActive();
	if (bTrail && !bOn) SwitchRibbons(true, false);
	else if (!bTrail && bOn && RibbonStrength < 0.01f) SwitchRibbons(false, false);

	// Strength and aim
	SetRibbonIntensity(RibbonStrength);
	UpdateRibbonTrail();
}

void AZooDuma::PlaySparkleBurst() const
{
	if (!SparkleBurst) return;

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SparkleBurst, GetActorLocation());
}

// ====== Performance ====== //

void AZooDuma::SetPerformPose(const FVector& LocalOffset, const FQuat& LocalRotation) const
{
	// The pose moves the whole mesh around the actor centre; its own placement stays underneath.
	Mesh->SetRelativeTransform(MeshBase * FTransform{ LocalRotation, LocalOffset });
}