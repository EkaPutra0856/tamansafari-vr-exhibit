// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/WzDirectorGraph.h"

#include "Director/WzNarrativeDirector.h"
#include "Graph/WzDirectorGraphSchema.h"
#include "Graph/WzQuestGraphNode.h"
#include "Quest/WzQuest.h"
#include "Quest/WzQuestLogic.h"

#define LOCTEXT_NAMESPACE "WzDirectorGraph"

namespace
{
	constexpr float ColumnWidth = 350.f;
	constexpr float RowHeight = 160.f;

	/** The longest chain of prerequisites behind each quest (0 = none), for the first layout. */
	TMap<const UWzQuest*, int32> MakeDepths(const UWzNarrativeDirector& Director)
	{
		TMap<const UWzQuest*, int32> Depths;
		for (const auto Quest : Director.Quests) if (Quest) Depths.Add(Quest.Get(), 0);

		// One level per pass; a loop in the data ends with the passes
		for (int32 Pass = 0; Pass < Director.Quests.Num(); ++Pass)
		{
			for (const auto Quest : Director.Quests)
			{
				if (!Quest) continue;

				for (const auto& Tag : Quest->Prerequisites) if (const UWzQuest* Prerequisite = Director.FindQuest(Tag)) Depths[Quest.Get()] = FMath::Max(Depths[Quest.Get()], Depths[Prerequisite] + 1);
			}
		}

		return Depths;
	}
}

// ====== Getters ====== //

UWzDirectorGraph* UWzDirectorGraph::FindOrCreate(UWzNarrativeDirector& Director)
{
	if (UWzDirectorGraph* Existing = Cast<UWzDirectorGraph>(Director.EdGraph)) return Existing;

	UWzDirectorGraph* Graph = NewObject<UWzDirectorGraph>(&Director, NAME_None, RF_Transactional);
	Graph->Schema = UWzDirectorGraphSchema::StaticClass();
	Director.EdGraph = Graph;

	return Graph;
}

UWzNarrativeDirector* UWzDirectorGraph::GetDirector() const
{
	return CastChecked<UWzNarrativeDirector>(GetOuter());
}

TArray<UWzQuestGraphNode*> UWzDirectorGraph::GetQuestNodes() const
{
	TArray<UWzQuestGraphNode*> QuestNodes;
	for (const auto Node : Nodes) if (UWzQuestGraphNode* QuestNode = Cast<UWzQuestGraphNode>(Node)) QuestNodes.Add(QuestNode);

	return QuestNodes;
}

UWzQuestGraphNode* UWzDirectorGraph::FindNode(const UWzQuest* Quest) const
{
	for (const auto Node : GetQuestNodes()) if (Node->Quest == Quest) return Node;

	return nullptr;
}

bool UWzDirectorGraph::HasUniqueTag(const UWzQuest& Quest) const
{
	if (!Quest.QuestTag.IsValid()) return false;

	int32 Count = 0;
	for (const auto Other : GetDirector()->Quests) if (Other && Other->QuestTag == Quest.QuestTag) ++Count;

	return Count == 1;
}

FText UWzDirectorGraph::GetLinkError(const UWzQuestGraphNode& Prerequisite, const UWzQuestGraphNode& Dependent) const
{
	if (&Prerequisite == &Dependent) return LOCTEXT("Self", "A quest cannot require itself");
	if (!HasUniqueTag(*Prerequisite.Quest)) return LOCTEXT("NoTag", "Set a unique Quest Tag on the required quest first");

	// What each quest requires today, by quest and not by tag
	TMap<const UWzQuest*, TArray<const UWzQuest*>> PrerequisitesOf;
	for (const auto Node : GetQuestNodes()) for (const auto Pin : Node->GetInPin()->LinkedTo) PrerequisitesOf.FindOrAdd(Node->Quest.Get()).Add(CastChecked<UWzQuestGraphNode>(Pin->GetOwningNode())->Quest.Get());

	// The dependent already stands behind the prerequisite: requiring it closes a loop
	if (WzQuestLogic::WouldCreateCycle<const UWzQuest*>(PrerequisitesOf, Dependent.Quest.Get(), Prerequisite.Quest.Get())) return LOCTEXT("Cycle", "The quests would wait on each other");

	return FText::GetEmpty();
}

// ====== Sync ====== //

void UWzDirectorGraph::Sync()
{
	UWzNarrativeDirector* Director = GetDirector();

	// Drop the nodes of quests that are gone
	for (const auto Node : GetQuestNodes()) if (!Node->Quest || !Director->Quests.Contains(Node->Quest)) RemoveNode(Node);

	// A node for every quest without one, laid out by prerequisite depth
	const TMap<const UWzQuest*, int32> Depths = MakeDepths(*Director);
	TMap<int32, int32> NextRow;
	for (const auto Quest : Director->Quests)
	{
		if (!Quest) continue;

		Quest->SetFlags(RF_Transactional);
		if (FindNode(Quest)) continue;

		const int32 Depth = Depths[Quest.Get()];
		CreateQuestNode(*Quest, { Depth * ColumnWidth, NextRow.FindOrAdd(Depth)++ * RowHeight }, false);
	}

	// The arrows: a quest's Prerequisites, skipping tags no quest has
	for (const auto Dependent : GetQuestNodes())
	{
		TSet<UEdGraphPin*> Wanted;
		for (const auto& Tag : Dependent->Quest->Prerequisites)
		{
			const UWzQuestGraphNode* Prerequisite = FindNode(Director->FindQuest(Tag));
			if (Prerequisite && Prerequisite != Dependent) Wanted.Add(Prerequisite->GetOutPin());
		}

		// Rebuild only a quest whose arrows differ, so a sync after an undo changes nothing
		UEdGraphPin* InPin = Dependent->GetInPin();
		const TSet<UEdGraphPin*> Linked(InPin->LinkedTo);
		if (Linked.Num() == Wanted.Num() && Linked.Includes(Wanted)) continue;

		Dependent->Modify();
		InPin->BreakAllPinLinks();
		for (const auto OutPin : Wanted)
		{
			OutPin->GetOwningNode()->Modify();
			InPin->MakeLinkTo(OutPin);
		}
	}
}

void UWzDirectorGraph::ApplyTagRenames()
{
	const UWzNarrativeDirector* Director = GetDirector();
	for (const auto Node : GetQuestNodes())
	{
		// A cleared tag waits: the arrows keep the last tag until a new one is set
		const FGameplayTag NewTag = Node->Quest ? Node->Quest->QuestTag : FGameplayTag{};
		if (!NewTag.IsValid() || NewTag == Node->LinkedTag || !Director->Quests.Contains(Node->Quest)) continue;

		// The quests that required the old tag now require the new one
		const FGameplayTag OldTag = Node->LinkedTag;
		if (OldTag.IsValid())
		{
			for (const auto Other : Director->Quests)
			{
				if (!Other || !Other->Prerequisites.Contains(OldTag)) continue;

				Other->Modify();
				Other->Prerequisites.Remove(OldTag);
				Other->Prerequisites.AddUnique(NewTag);
			}
		}

		Node->Modify();
		Node->LinkedTag = NewTag;
	}
}

// ====== Editing ====== //

UWzQuestGraphNode* UWzDirectorGraph::AddQuest(const FVector2f& Location, const bool bSelectNewNode)
{
	UWzNarrativeDirector* Director = GetDirector();
	Director->Modify();
	Modify();

	// The Director owns its quests, or they would not save with it
	UWzQuest* Quest = NewObject<UWzQuest>(Director, NAME_None, RF_Transactional);
	Quest->DisplayName = LOCTEXT("NewQuest", "New Quest");
	Director->Quests.Add(Quest);

	return CreateQuestNode(*Quest, Location, bSelectNewNode);
}

void UWzDirectorGraph::DeleteQuest(UWzQuestGraphNode& Node)
{
	UWzNarrativeDirector* Director = GetDirector();
	Director->Modify();
	Modify();

	// Nothing may keep requiring it, unless another quest still carries the tag
	UWzQuest* Quest = Node.Quest;
	Director->Quests.Remove(Quest);
	for (const auto& Tag : { Quest->QuestTag, Node.LinkedTag })
	{
		if (!Tag.IsValid() || Director->FindQuest(Tag)) continue;

		for (const auto Other : Director->Quests)
		{
			if (!Other || !Other->Prerequisites.Contains(Tag)) continue;

			Other->Modify();
			Other->Prerequisites.Remove(Tag);
		}
	}

	RemoveNode(&Node);
}

void UWzDirectorGraph::AddPrerequisite(const UWzQuestGraphNode& Prerequisite, UWzQuestGraphNode& Dependent)
{
	Dependent.Quest->Modify();
	Dependent.Quest->Prerequisites.AddUnique(Prerequisite.Quest->QuestTag);
}

void UWzDirectorGraph::RemovePrerequisite(const UWzQuestGraphNode& Prerequisite, UWzQuestGraphNode& Dependent)
{
	Dependent.Quest->Modify();
	Dependent.Quest->Prerequisites.Remove(Prerequisite.Quest->QuestTag);
}

// ====== Helpers ====== //

UWzQuestGraphNode* UWzDirectorGraph::CreateQuestNode(UWzQuest& Quest, const FVector2f& Location, const bool bSelectNewNode)
{
	UWzQuestGraphNode* Node = NewObject<UWzQuestGraphNode>(this, NAME_None, RF_Transactional);
	Node->Quest = &Quest;
	Node->LinkedTag = Quest.QuestTag;
	Node->NodePosX = Location.X;
	Node->NodePosY = Location.Y;
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	AddNode(Node, false, bSelectNewNode);

	return Node;
}

#undef LOCTEXT_NAMESPACE