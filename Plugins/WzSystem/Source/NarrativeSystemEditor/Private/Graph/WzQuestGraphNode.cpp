// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/WzQuestGraphNode.h"

#include "Quest/WzNarrativeCondition.h"
#include "Quest/WzQuest.h"

#define LOCTEXT_NAMESPACE "WzQuestGraphNode"

namespace
{
	const FName QuestPinCategory{ TEXT("Quest") };
	const FName InPinName{ TEXT("Requires") };
	const FName OutPinName{ TEXT("Unlocks") };
}

// ====== References ====== //

UEdGraphPin* UWzQuestGraphNode::GetInPin() const
{
	return FindPinChecked(InPinName, EGPD_Input);
}

UEdGraphPin* UWzQuestGraphNode::GetOutPin() const
{
	return FindPinChecked(OutPinName, EGPD_Output);
}

// ====== Lifecycle ====== //

void UWzQuestGraphNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, QuestPinCategory, InPinName);
	CreatePin(EGPD_Output, QuestPinCategory, OutPinName);
}

bool UWzQuestGraphNode::CanDuplicateNode() const
{
	return false;
}

// ====== Appearance ====== //

FText UWzQuestGraphNode::GetNodeTitle(const ENodeTitleType::Type TitleType) const
{
	if (!Quest) return LOCTEXT("Missing", "Missing Quest");

	const FText Name = Quest->DisplayName.IsEmpty() ? LOCTEXT("Unnamed", "Unnamed Quest") : Quest->DisplayName;
	if (TitleType != ENodeTitleType::FullTitle) return Name;

	// Second line: what starts the quest
	FText Start = LOCTEXT("StartsImmediately", "Starts: Immediately");
	if (Quest->StartCondition)
	{
		const FText Condition = Quest->StartCondition->Description.IsEmpty() ? Quest->StartCondition->GetClass()->GetDisplayNameText() : Quest->StartCondition->Description;
		Start = FText::Format(LOCTEXT("StartsWith", "Starts: {0}"), Condition);
	}

	// Third line: a quest cannot be required before it has a tag
	if (!Quest->QuestTag.IsValid()) return FText::Format(LOCTEXT("TitleUntagged", "{0}\n{1}\nSet the Quest Tag"), Name, Start);

	return FText::Format(LOCTEXT("Title", "{0}\n{1}"), Name, Start);
}

FLinearColor UWzQuestGraphNode::GetNodeTitleColor() const
{
	// The main story is gold, the side quests blue
	return Quest && Quest->Type == EWzQuestType::Optional ? FLinearColor{ 0.1f, 0.3f, 0.45f } : FLinearColor{ 0.55f, 0.32f, 0.05f };
}

FText UWzQuestGraphNode::GetTooltipText() const
{
	if (!Quest) return FText::GetEmpty();

	return Quest->QuestTag.IsValid() ? FText::FromString(Quest->QuestTag.ToString()) : LOCTEXT("NoTag", "No Quest Tag yet");
}

#undef LOCTEXT_NAMESPACE