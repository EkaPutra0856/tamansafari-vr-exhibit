// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PathGuide/ZooPlaceCardsAsset.h"

#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ZooPlaceCardsAsset"

// ====== Getters ====== //

const FZooPlaceCard* UZooPlaceCardsAsset::FindCard(const FGameplayTag& PlaceTag) const
{
	if (!PlaceTag.IsValid()) return nullptr;

	return Cards.FindByPredicate([&PlaceTag](const FZooPlaceCard& Card) { return Card.PlaceTag == PlaceTag; });
}

#if WITH_EDITOR

// ====== Lifecycle ====== //

EDataValidationResult UZooPlaceCardsAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TSet<FGameplayTag> SeenTags;
	TSet<int32> SeenNumbers;

	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const FZooPlaceCard& Card = Cards[Index];
		auto Fail = [&](const FText& Message)
		{
			Context.AddError(FText::Format(LOCTEXT("CardIssue", "Cards[{0}]: {1}"), Index, Message));
			Result = EDataValidationResult::Invalid;
		};

		// Tag: set and unique
		if (!Card.PlaceTag.IsValid()) Fail(LOCTEXT("NoTag", "PlaceTag is empty."));
		else if (SeenTags.Contains(Card.PlaceTag)) Fail(FText::Format(LOCTEXT("DupTag", "PlaceTag '{0}' is used more than once."), FText::FromName(Card.PlaceTag.GetTagName())));
		SeenTags.Add(Card.PlaceTag);

		// Number: unique when set (0 = unnumbered)
		if (Card.Number > 0)
		{
			if (SeenNumbers.Contains(Card.Number)) Fail(FText::Format(LOCTEXT("DupNumber", "Number {0} is used more than once."), Card.Number));
			SeenNumbers.Add(Card.Number);
		}
	}

	if (Result == EDataValidationResult::NotValidated) Result = EDataValidationResult::Valid;

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE