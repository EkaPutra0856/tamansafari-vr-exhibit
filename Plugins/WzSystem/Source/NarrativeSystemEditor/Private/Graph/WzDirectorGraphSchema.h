// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "WzDirectorGraphSchema.generated.h"

/** The rules of the Director graph: an arrow is a Prerequisite, and every arrow made or cut is written to the quests. */
UCLASS()
class UWzDirectorGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	// ====== Actions ====== //

	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;

public:
	// ====== Connections ====== //

	/** Out pin = the required quest, In pin = the one that requires it, whichever way the user dragged. */
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;

	/** BreakNodeLinks goes through this one, so it needs no override. */
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;

public:
	// ====== Appearance ====== //

	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
};