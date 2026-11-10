// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hologram/ZooHologramComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/SkeletalMesh.h"
#include "Hologram/ZooHologramProfile.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"

/** The parameter contract with M_ZooHologram and the Niagara systems. Heights are mesh-local and unscaled: Z up from the mesh pivot (the feet). */
static const FName RevealHeightParameter(TEXT("RevealHeight"));
static const FName HoloHeightParameter(TEXT("HoloHeight"));
static const FName GlitchParameter(TEXT("Glitch"));
static const FName SpeakingParameter(TEXT("Speaking"));
static const FName NiagaraRevealHeight(TEXT("User.RevealHeight"));
static const FName NiagaraHoloHeight(TEXT("User.HoloHeight"));
static const FName NiagaraDirection(TEXT("User.Direction"));
static const FName NiagaraRingRadius(TEXT("User.RingRadius"));
static const FName NiagaraTint(TEXT("User.Tint"));
static const FName NiagaraMesh(TEXT("User.Mesh"));

/** A reveal height above any mesh: fully visible. */
static constexpr float RevealVisible = 10000.f;

/** The speaking glow snaps to its goal inside this distance, so the tick can sleep. */
static constexpr float BlendTolerance = 0.01f;

// ====== Lifecycle ====== //

UZooHologramComponent::UZooHologramComponent()
{
	// Ticks only while something animates.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UZooHologramComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickPhase(DeltaTime);
	TickGlitch(DeltaTime);
	TickSpeaking(DeltaTime);
	TickAmbient(DeltaTime);

	// Sleep when nothing animates.
	SetComponentTickEnabled(IsAnimating());
}

// ====== API ====== //

void UZooHologramComponent::SetTarget(USkeletalMeshComponent* InTarget)
{
	Target = InTarget;
	if (!Target) return;

	// Dynamic material on the first slot.
	Material = Target->CreateAndSetMaterialInstanceDynamic(0);
	if (!Profile || !Material) return;

	// Measure the mesh: height in mesh units, ring radius in world units.
	const USkeletalMesh* Asset = Target->GetSkeletalMeshAsset();
	const FBoxSphereBounds Bounds = Asset ? Asset->GetBounds() : FBoxSphereBounds{ ForceInit };
	HoloHeight = Bounds.Origin.Z + Bounds.BoxExtent.Z;
	RingRadius = FMath::Min(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * Profile->RingRadiusScale * Target->GetComponentScale().X, Profile->MaxRingRadius);

	// Hidden until Appear (the material's front-layer mask reads custom depth); the stencil tells the camera halo whose color to use.
	Target->SetRenderCustomDepth(true);
	Target->SetCustomDepthStencilValue(Profile->HaloStencil);
	Material->SetScalarParameterValue(HoloHeightParameter, HoloHeight);
	SetRevealHeight(0.f);
}

void UZooHologramComponent::Appear(const float Seconds)
{
	if (Phase == EZooHologramPhase::Appearing || Phase == EZooHologramPhase::Shown) return;

	// Without a profile there is nothing to play.
	if (!Profile)
	{
		Phase = EZooHologramPhase::Shown;
		OnAppeared.Broadcast();

		return;
	}

	// Resume from wherever the line is (a Disappear may be cut short).
	AppearSeconds = Seconds > 0.f ? Seconds : Profile->AppearSeconds;
	PhaseTime = RevealAlpha * AppearSeconds;
	Phase = EZooHologramPhase::Appearing;

	// Ring and particles, the hover effect, the sound.
	SpawnMaterialize(1.f);
	StopEffect(AmbientComponent);
	AmbientComponent = SpawnEffect(Profile->AmbientEffect);
	if (AmbientComponent) AmbientComponent->Activate(true);
	AmbientRemaining = 0.f;
	PlaySound(Profile->AppearSound);

	SetComponentTickEnabled(true);
}

void UZooHologramComponent::Disappear(const float Seconds, const bool bLinger)
{
	if (Phase == EZooHologramPhase::Disappearing) return;

	// Nothing to dematerialize: never shown, or no profile.
	if (Phase == EZooHologramPhase::Hidden || !Profile)
	{
		Phase = EZooHologramPhase::Hidden;
		OnDisappeared.Broadcast();

		return;
	}

	// Start from wherever the line is (an Appear may be cut short).
	DisappearSeconds = Seconds > 0.f ? Seconds : Profile->DisappearSeconds;
	LingerSeconds = bLinger ? Profile->LingerSeconds : 0.f;
	PhaseTime = (1.f - RevealAlpha) * DisappearSeconds;
	Phase = EZooHologramPhase::Disappearing;

	// The hover effect ends; ring and particles take over.
	StopEffect(AmbientComponent);
	SpawnMaterialize(-1.f);
	PlaySound(Profile->DisappearSound);

	SetComponentTickEnabled(true);
}

void UZooHologramComponent::Glitch()
{
	if (!Profile || Phase == EZooHologramPhase::Hidden) return;

	// Start the burst.
	GlitchRemaining = Profile->GlitchSeconds;
	PlaySound(Profile->GlitchSound);
	SetComponentTickEnabled(true);
}

void UZooHologramComponent::SetSpeaking(const bool bInSpeaking)
{
	if (!Profile) return;

	// The tick blends the glow toward the new goal.
	bSpeaking = bInSpeaking;
	SetComponentTickEnabled(true);
}

// ====== Helpers ====== //

void UZooHologramComponent::TickPhase(const float DeltaTime)
{
	const bool bAppearing = Phase == EZooHologramPhase::Appearing;
	if (!bAppearing && Phase != EZooHologramPhase::Disappearing) return;

	// The line follows the phase time: up when appearing, down when disappearing.
	PhaseTime += DeltaTime;
	RevealAlpha = bAppearing ? FMath::Clamp(PhaseTime / AppearSeconds, 0.f, 1.f) : 1.f - FMath::Clamp(PhaseTime / DisappearSeconds, 0.f, 1.f);
	SetRevealHeight(RevealHeightAt(RevealAlpha));

	// Appearing ends with the line at the top.
	if (bAppearing)
	{
		if (RevealAlpha >= 1.f) FinishAppear();

		return;
	}

	// Disappearing: the ring ends at the feet, the last particles get their linger.
	if (RevealAlpha <= 0.f) StopEffect(MaterializeComponent);
	if (PhaseTime >= DisappearSeconds + LingerSeconds) FinishDisappear();
}

void UZooHologramComponent::TickGlitch(const float DeltaTime)
{
	if (GlitchRemaining <= 0.f) return;

	// Fades out over the glitch time.
	GlitchRemaining = FMath::Max(GlitchRemaining - DeltaTime, 0.f);
	if (Material) Material->SetScalarParameterValue(GlitchParameter, GlitchRemaining / Profile->GlitchSeconds);
}

void UZooHologramComponent::TickSpeaking(const float DeltaTime)
{
	const float Goal = bSpeaking ? 1.f : 0.f;
	if (FMath::IsNearlyEqual(SpeakingValue, Goal)) return;

	// Blend toward the goal, snapping when close.
	SpeakingValue = FMath::FInterpTo(SpeakingValue, Goal, DeltaTime, Profile->SpeakingBlendSpeed);
	if (FMath::IsNearlyEqual(SpeakingValue, Goal, BlendTolerance)) SpeakingValue = Goal;
	if (Material) Material->SetScalarParameterValue(SpeakingParameter, SpeakingValue);
}

void UZooHologramComponent::TickAmbient(const float DeltaTime)
{
	if (AmbientRemaining <= 0.f) return;

	// The spawn disc has shown long enough: let it finish (a Disappear / Appear starts a fresh one)
	AmbientRemaining -= DeltaTime;
	if (AmbientRemaining <= 0.f && Phase == EZooHologramPhase::Shown) StopEffect(AmbientComponent);
}

bool UZooHologramComponent::IsAnimating() const
{
	const float Goal = bSpeaking ? 1.f : 0.f;

	return Phase == EZooHologramPhase::Appearing || Phase == EZooHologramPhase::Disappearing || GlitchRemaining > 0.f || AmbientRemaining > 0.f || !FMath::IsNearlyEqual(SpeakingValue, Goal);
}

void UZooHologramComponent::SetRevealHeight(const float Height) const
{
	if (Material) Material->SetScalarParameterValue(RevealHeightParameter, Height);
	if (IsValid(MaterializeComponent)) MaterializeComponent->SetVariableFloat(NiagaraRevealHeight, Height);
}

float UZooHologramComponent::RevealHeightAt(const float Alpha) const
{
	// Eased by the profile's curve, smoothstep when there is none.
	const UCurveFloat* Curve = Profile->RevealCurve;
	const float Eased = Curve ? FMath::Clamp(Curve->GetFloatValue(Alpha), 0.f, 1.f) : FMath::SmoothStep(0.f, 1.f, Alpha);

	return (HoloHeight + Profile->BandOvershoot) * Eased;
}

void UZooHologramComponent::FinishAppear()
{
	// The line leaves the mesh; the particles live out their linger. The hover disc stays only for the profile's seconds.
	SetRevealHeight(RevealVisible);
	StopEffect(MaterializeComponent);
	AmbientRemaining = Profile->AmbientSeconds;

	Phase = EZooHologramPhase::Shown;
	OnAppeared.Broadcast();
}

void UZooHologramComponent::FinishDisappear()
{
	// The ring already ended at the feet; the hover effect is gone too.
	StopEffect(AmbientComponent);

	Phase = EZooHologramPhase::Hidden;
	OnDisappeared.Broadcast();
}

FVector UZooHologramComponent::GetEffectLocation() const
{
	const FVector Pivot = Target->GetComponentLocation();
	if (!Profile->bRingOnGround) return Pivot;

	// Under the body, not the pivot: some animal meshes sit far off their pivot (SK_Wombat by ~4.9 m at scale 1)
	const USkeletalMesh* Asset = Target->GetSkeletalMeshAsset();
	const FVector Center = Asset ? Target->GetComponentTransform().TransformPosition(Asset->GetBounds().Origin) : Pivot;
	const FVector Body { Center.X, Center.Y, Pivot.Z };

	// The ground under the body (the owner and its parts are not the ground)
	FCollisionQueryParams Params { SCENE_QUERY_STAT(ZooHologramGround), false, GetOwner() };
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Body + FVector{ 0.f, 0.f, 60.f }, Body - FVector{ 0.f, 0.f, Profile->GroundSearch }, ECC_Visibility, Params)) return Hit.ImpactPoint + FVector{ 0.f, 0.f, 3.f };

	return Body;
}

UNiagaraComponent* UZooHologramComponent::SpawnEffect(UNiagaraSystem* System) const
{
	if (!System || !Target) return nullptr;

	// Attached at the mesh pivot (or on the ground under it, scanning the terrain), riding along at world scale; activated by the caller.
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Target, NAME_None, GetEffectLocation(), FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition, true, false);
	if (Effect) Effect->SetUsingAbsoluteScale(true);
	if (Effect) Effect->SetWorldScale3D(FVector::OneVector);
	if (!Effect) return nullptr;

	// Shared parameters: the mesh to sample, its size, the accent color.
	Effect->SetVariableObject(NiagaraMesh, Target);
	Effect->SetVariableFloat(NiagaraHoloHeight, HoloHeight);
	Effect->SetVariableFloat(NiagaraRingRadius, RingRadius);
	Effect->SetVariableLinearColor(NiagaraTint, Profile->AccentTint);

	return Effect;
}

void UZooHologramComponent::SpawnMaterialize(const float Direction)
{
	StopEffect(MaterializeComponent);

	MaterializeComponent = SpawnEffect(Profile->MaterializeEffect);
	if (!MaterializeComponent) return;

	// The effect reads the same reveal height as the material, so the ring and the particles can never drift from it.
	MaterializeComponent->SetVariableFloat(NiagaraDirection, Direction);
	MaterializeComponent->SetVariableFloat(NiagaraRevealHeight, RevealHeightAt(RevealAlpha));
	MaterializeComponent->Activate(true);
}

void UZooHologramComponent::StopEffect(TObjectPtr<UNiagaraComponent>& Effect)
{
	// Deactivate, not destroy: the live particles finish, then the component cleans itself up.
	if (IsValid(Effect)) Effect->Deactivate();
	Effect = nullptr;
}

void UZooHologramComponent::PlaySound(USoundBase* Sound) const
{
	if (Sound && Target) UGameplayStatics::SpawnSoundAttached(Sound, Target);
}