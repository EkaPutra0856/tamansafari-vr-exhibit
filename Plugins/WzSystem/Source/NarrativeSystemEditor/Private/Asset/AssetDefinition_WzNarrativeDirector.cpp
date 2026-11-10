// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/AssetDefinition_WzNarrativeDirector.h"

#include "Director/WzNarrativeDirector.h"
#include "Editor/WzDirectorEditorToolkit.h"

#define LOCTEXT_NAMESPACE "WzNarrativeDirectorAsset"

// ====== Asset ====== //

FText UAssetDefinition_WzNarrativeDirector::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Narrative Director");
}

FLinearColor UAssetDefinition_WzNarrativeDirector::GetAssetColor() const
{
	return FColor(222, 160, 40);
}

TSoftClassPtr<UObject> UAssetDefinition_WzNarrativeDirector::GetAssetClass() const
{
	return UWzNarrativeDirector::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_WzNarrativeDirector::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(EAssetCategoryPaths::Gameplay, LOCTEXT("NarrativeSubMenu", "Narrative")) };

	return Categories;
}

EAssetCommandResult UAssetDefinition_WzNarrativeDirector::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (const auto Director : OpenArgs.LoadObjects<UWzNarrativeDirector>())
	{
		const TSharedRef<FWzDirectorEditorToolkit> Editor = MakeShared<FWzDirectorEditorToolkit>();
		Editor->Init(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Director);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE