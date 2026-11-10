// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/WzNarrativeDirectorFactory.h"

#include "Director/WzNarrativeDirector.h"

// ====== Lifecycle ====== //

UWzNarrativeDirectorFactory::UWzNarrativeDirectorFactory()
{
	SupportedClass = UWzNarrativeDirector::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UWzNarrativeDirectorFactory::FactoryCreateNew(UClass* Class, UObject* Parent, const FName Name, const EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UWzNarrativeDirector>(Parent, Class, Name, Flags | RF_Transactional);
}