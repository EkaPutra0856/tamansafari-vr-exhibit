// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathGuide/Data/PathGuidePlacesAsset.h"

#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "PathGuidePlacesAsset"

// ====== Getters ====== //

TArray<FPathGuidePlace> UWzPathGuidePlacesAsset::GetPlaces() const
{
	// Tagged places only, in array order
	TArray<FPathGuidePlace> Result;
	for (const auto& Place : Places) if (Place.PlaceTag.IsValid()) Result.Add(Place);

	return Result;
}

const FPathGuidePlace* UWzPathGuidePlacesAsset::FindPlacePtr(const FGameplayTag& PlaceTag) const
{
	if (!PlaceTag.IsValid()) return nullptr;

	return Places.FindByPredicate([&PlaceTag](const FPathGuidePlace& Place) { return Place.PlaceTag == PlaceTag; });
}

#if WITH_EDITOR

// ====== Lifecycle ====== //

EDataValidationResult UWzPathGuidePlacesAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TSet<FGameplayTag> SeenTags;

	for (int32 Index = 0; Index < Places.Num(); ++Index)
	{
		const FPathGuidePlace& Place = Places[Index];
		auto Fail = [&](const FText& Message)
		{
			Context.AddError(FText::Format(LOCTEXT("PlaceIssue", "Places[{0}]: {1}"), Index, Message));
			Result = EDataValidationResult::Invalid;
		};

		// Tag: set and unique.
		if (!Place.PlaceTag.IsValid()) Fail(LOCTEXT("NoTag", "PlaceTag is empty."));
		else if (SeenTags.Contains(Place.PlaceTag)) Fail(FText::Format(LOCTEXT("DupTag", "PlaceTag '{0}' is used more than once."), FText::FromName(Place.PlaceTag.GetTagName())));
		SeenTags.Add(Place.PlaceTag);

		// Name
		if (Place.DisplayName.IsEmptyOrWhitespace()) Fail(LOCTEXT("NoName", "DisplayName is empty."));
	}

	if (Result == EDataValidationResult::NotValidated) Result = EDataValidationResult::Valid;

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
