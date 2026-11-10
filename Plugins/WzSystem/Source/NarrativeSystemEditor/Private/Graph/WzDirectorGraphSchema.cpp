// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/WzDirectorGraphSchema.h"

#include "Graph/WzDirectorGraph.h"
#include "Graph/WzQuestGraphNode.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "WzDirectorGraphSchema"

namespace
{
	/** The "Add Quest" entry of the graph's context menu. */
	struct FWzSchemaAction_AddQuest : public FEdGraphSchemaAction
	{
		FWzSchemaAction_AddQuest() : FEdGraphSchemaAction(FText::GetEmpty(), LOCTEXT("AddQuest", "Add Quest"), LOCTEXT("AddQuestTip", "A new quest on the graph"), 0) {}

		virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2f& Location, const bool bSelectNewNode = true) override
		{
			const FScopedTransaction Transaction(LOCTEXT("AddQuestTransaction", "Add Quest"));

			return CastChecked<UWzDirectorGraph>(ParentGraph)->AddQuest(Location, bSelectNewNode);
		}
	};

	/** The quest nodes at both pins as required (the Out pin's) and dependent (the In pin's), whichever way they were dragged. False when the pins make no arrow. */
	bool GetEnds(const UEdGraphPin& A, const UEdGraphPin& B, UWzQuestGraphNode*& Prerequisite, UWzQuestGraphNode*& Dependent)
	{
		if (A.Direction == B.Direction) return false;

		const UEdGraphPin& OutPin = A.Direction == EGPD_Output ? A : B;
		const UEdGraphPin& InPin = A.Direction == EGPD_Output ? B : A;
		Prerequisite = Cast<UWzQuestGraphNode>(OutPin.GetOwningNode());
		Dependent = Cast<UWzQuestGraphNode>(InPin.GetOwningNode());

		return Prerequisite && Dependent;
	}

	/** Removes the Prerequisite that the arrow between the pins stood for. */
	void RemoveArrow(const UEdGraphPin& A, const UEdGraphPin& B)
	{
		UWzQuestGraphNode* Prerequisite = nullptr;
		UWzQuestGraphNode* Dependent = nullptr;
		if (!GetEnds(A, B, Prerequisite, Dependent)) return;

		CastChecked<UWzDirectorGraph>(Dependent->GetGraph())->RemovePrerequisite(*Prerequisite, *Dependent);
	}
}

// ====== Actions ====== //

void UWzDirectorGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	ContextMenuBuilder.AddAction(MakeShared<FWzSchemaAction_AddQuest>());
}

// ====== Connections ====== //

const FPinConnectionResponse UWzDirectorGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	UWzQuestGraphNode* Prerequisite = nullptr;
	UWzQuestGraphNode* Dependent = nullptr;
	if (!GetEnds(*A, *B, Prerequisite, Dependent)) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Connect an output to an input"));
	if (A->LinkedTo.Contains(B)) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Already connected"));

	const FText Error = CastChecked<UWzDirectorGraph>(Dependent->GetGraph())->GetLinkError(*Prerequisite, *Dependent);
	if (!Error.IsEmpty()) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, Error.ToString());

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT("The first quest must be completed before the second"));
}

bool UWzDirectorGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	UWzQuestGraphNode* Prerequisite = nullptr;
	UWzQuestGraphNode* Dependent = nullptr;
	if (!GetEnds(*A, *B, Prerequisite, Dependent)) return false;

	// The arrow and the Prerequisites change together, so they undo together
	FScopedTransaction Transaction(LOCTEXT("Connect", "Add Quest Prerequisite"));
	A->GetOwningNode()->Modify();
	B->GetOwningNode()->Modify();
	if (!Super::TryCreateConnection(A, B))
	{
		Transaction.Cancel();

		return false;
	}

	CastChecked<UWzDirectorGraph>(Dependent->GetGraph())->AddPrerequisite(*Prerequisite, *Dependent);

	return true;
}

void UWzDirectorGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, const bool bSendsNodeNotification) const
{
	// Every arrow cut removes its Prerequisite
	const FScopedTransaction Transaction(LOCTEXT("Disconnect", "Remove Quest Prerequisite"));
	TargetPin.GetOwningNode()->Modify();
	for (const auto OtherPin : TargetPin.LinkedTo)
	{
		OtherPin->GetOwningNode()->Modify();
		RemoveArrow(TargetPin, *OtherPin);
	}

	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);
}

void UWzDirectorGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(LOCTEXT("DisconnectOne", "Remove Quest Prerequisite"));
	SourcePin->GetOwningNode()->Modify();
	TargetPin->GetOwningNode()->Modify();
	RemoveArrow(*SourcePin, *TargetPin);

	Super::BreakSinglePinLink(SourcePin, TargetPin);
}

// ====== Appearance ====== //

FLinearColor UWzDirectorGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return { 0.9f, 0.7f, 0.2f };
}

#undef LOCTEXT_NAMESPACE