// Copyright Epic Games, Inc. All Rights Reserved.

#include "Director/WzNarrativeDirector.h"

#include "Misc/DataValidation.h"
#include "Quest/WzNarrativeConditions.h"
#include "Quest/WzQuest.h"
#include "Quest/WzQuestLogic.h"

// ====== Lifecycle ====== //

#if WITH_EDITOR
EDataValidationResult UWzNarrativeDirector::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Every quest needs a unique tag
	TSet<FGameplayTag> Tags;
	for (const auto& Quest : Quests)
	{
		if (!Quest)
		{
			Context.AddError(FText::FromString(TEXT("A quest slot is empty.")));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		bool bDuplicate = false;
		if (Quest->QuestTag.IsValid()) Tags.Add(Quest->QuestTag, &bDuplicate);
		if (!Quest->QuestTag.IsValid() || bDuplicate)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Quest '%s' has no tag or shares it with another quest."), *Quest->DisplayName.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}

	// Prerequisites must be quests of this director, and never loop
	TMap<FGameplayTag, TArray<FGameplayTag>> PrerequisitesOf;
	for (const auto& Quest : Quests)
	{
		if (!Quest) continue;

		PrerequisitesOf.Add(Quest->QuestTag, Quest->Prerequisites);
		for (const auto& Prerequisite : Quest->Prerequisites)
		{
			if (Tags.Contains(Prerequisite)) continue;

			Context.AddError(FText::FromString(FString::Printf(TEXT("Quest '%s' requires %s, which is not a quest here."), *Quest->DisplayName.ToString(), *Prerequisite.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}
	if (WzQuestLogic::HasCycle(PrerequisitesOf))
	{
		Context.AddError(FText::FromString(TEXT("The quest arrows form a loop.")));
		Result = EDataValidationResult::Invalid;
	}

	// An empty script or task slot would shift the task indices the tracker shows
	for (const auto& Quest : Quests)
	{
		if (!Quest) continue;

		bool bEmptySlot = Quest->Scripts.Contains(nullptr);
		for (const auto& Objective : Quest->Objectives) bEmptySlot |= Objective.Scripts.Contains(nullptr) || Objective.Tasks.Contains(nullptr);
		if (bEmptySlot)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Quest '%s' has an empty script or task slot."), *Quest->DisplayName.ToString())));
			Result = EDataValidationResult::Invalid;
		}

		// A Destroy Actors task without a tag counts nothing, so its objective never ends
		const auto* Start = Cast<UWzDestroyActorsCondition>(Quest->StartCondition);
		bool bUntagged = Start && !Start->Tag.IsValid();
		for (const auto& Objective : Quest->Objectives)
		{
			for (const auto& Task : Objective.Tasks)
			{
				const auto* Destroy = Cast<UWzDestroyActorsCondition>(Task);
				bUntagged |= Destroy && !Destroy->Tag.IsValid();
			}
		}
		if (!bUntagged) continue;

		Context.AddError(FText::FromString(FString::Printf(TEXT("Quest '%s' has a Destroy Actors task without a Tag."), *Quest->DisplayName.ToString())));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

// ====== Getters ====== //

UWzQuest* UWzNarrativeDirector::FindQuest(const FGameplayTag& QuestTag) const
{
	const auto* Found = Quests.FindByPredicate([&QuestTag](const TObjectPtr<UWzQuest>& Quest)
	{
		return Quest && Quest->QuestTag == QuestTag;
	});

	return Found ? Found->Get() : nullptr;
}