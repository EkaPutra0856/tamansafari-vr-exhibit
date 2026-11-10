// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "GameplayTagContainer.h"
#include "WzQuestGraphNode.generated.h"

class UWzQuest;

/** A quest on the Director graph. An arrow into its In pin is one of its Prerequisites. */
UCLASS()
class UWzQuestGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	// ====== References ====== //

	/** The quest this node shows. The Director owns it, so it is never instanced here. */
	UPROPERTY()
	TObjectPtr<UWzQuest> Quest;

	/** The quest tag the arrows out of this node were last written with, to follow a tag rename. */
	UPROPERTY()
	FGameplayTag LinkedTag;

	/** Where this quest's arrows come in: the quests it requires. */
	UEdGraphPin* GetInPin() const;

	/** Where this quest's arrows go out: the quests that require it. */
	UEdGraphPin* GetOutPin() const;

public:
	// ====== Lifecycle ====== //

	virtual void AllocateDefaultPins() override;

	/** A pasted node would share the quest of the original. */
	virtual bool CanDuplicateNode() const override;

public:
	// ====== Appearance ====== //

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
};