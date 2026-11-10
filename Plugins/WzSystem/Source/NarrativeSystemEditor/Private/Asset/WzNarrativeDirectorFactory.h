// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "WzNarrativeDirectorFactory.generated.h"

/** Content Browser > Add > Gameplay > Narrative > Narrative Director. */
UCLASS()
class UWzNarrativeDirectorFactory : public UFactory
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UWzNarrativeDirectorFactory();

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};