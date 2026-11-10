// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WzDialogueCameraEffect.generated.h"

class AActor;
class APlayerController;

/** A camera effect for one dialogue line. Shared by the dialogue asset, so keep it stateless. */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class NARRATIVESYSTEM_API UWzDialogueCameraEffect : public UObject
{
	GENERATED_BODY()

public:
	// ====== Events ====== //

	/** The line started. Speaker is null for a narrator. */
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	void BeginEffect(APlayerController* PlayerController, AActor* Speaker);

	/** Every frame while the line plays. */
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	void TickEffect(APlayerController* PlayerController, AActor* Speaker, float DeltaTime);

	/** The line (or the whole dialogue) ended. */
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	void EndEffect(APlayerController* PlayerController, AActor* Speaker);
};

/** Turns the view toward the speaker's eyes. Nothing for a narrator. */
UCLASS(meta = (DisplayName = "Look At Speaker"))
class NARRATIVESYSTEM_API UWzLookAtSpeakerCameraEffect : public UWzDialogueCameraEffect
{
	GENERATED_BODY()

public:
	// ====== Look At ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look At", meta = (ClampMin = "0"))
	float InterpSpeed = 4.f;

	/** Added to the eye point, for actors whose eyes are not at their pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look At")
	FVector TargetOffset = FVector::ZeroVector;

public:
	// ====== Events ====== //

	virtual void TickEffect_Implementation(APlayerController* PlayerController, AActor* Speaker, float DeltaTime) override;
};