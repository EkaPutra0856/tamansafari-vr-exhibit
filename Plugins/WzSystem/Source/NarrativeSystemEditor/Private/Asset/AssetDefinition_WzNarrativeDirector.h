// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "AssetDefinition_WzNarrativeDirector.generated.h"

/** Content Browser entry of the Narrative Director: double-click opens the graph editor. */
UCLASS()
class UAssetDefinition_WzNarrativeDirector : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// ====== Asset ====== //

	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};