// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Director/WzNarrativeDirector.h"
#include "GameplayTagContainer.h"
#include "Graph/WzDirectorGraph.h"
#include "Graph/WzDirectorGraphSchema.h"
#include "Graph/WzQuestGraphNode.h"
#include "Quest/WzQuest.h"

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** Parent tags of Narrative.Quest.Zoo.Welcome: real tags, so the test adds none to the project. */
	FGameplayTag Tag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName{ Name });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzDirectorGraphTest, "NarrativeSystemEditor.DirectorGraph", TestFlags)
bool FWzDirectorGraphTest::RunTest(const FString&)
{
	// A transient Director with a tagged quest A, a tagged quest B and an untagged quest C
	const FGameplayTag TagA = Tag(TEXT("Narrative.Quest.Zoo.Welcome"));
	const FGameplayTag TagB = Tag(TEXT("Narrative.Quest.Zoo"));
	const FGameplayTag TagRenamed = Tag(TEXT("Narrative.Quest"));
	UWzNarrativeDirector* Director = NewObject<UWzNarrativeDirector>(GetTransientPackage());
	UWzQuest* A = NewObject<UWzQuest>(Director);
	UWzQuest* B = NewObject<UWzQuest>(Director);
	UWzQuest* C = NewObject<UWzQuest>(Director);
	A->QuestTag = TagA;
	B->QuestTag = TagB;
	Director->Quests = { A, B, C };

	// Sync: a node per quest
	UWzDirectorGraph* Graph = UWzDirectorGraph::FindOrCreate(*Director);
	Graph->Sync();
	TestEqual(TEXT("a node per quest"), Graph->GetQuestNodes().Num(), 3);
	UWzQuestGraphNode* NodeA = Graph->FindNode(A);
	UWzQuestGraphNode* NodeB = Graph->FindNode(B);
	UWzQuestGraphNode* NodeC = Graph->FindNode(C);
	if (!NodeA || !NodeB || !NodeC) return false;

	// Connect A to B writes A's tag into B
	const UEdGraphSchema* Schema = Graph->GetSchema();
	TestTrue(TEXT("A to B is allowed"), Schema->CanCreateConnection(NodeA->GetOutPin(), NodeB->GetInPin()).Response == CONNECT_RESPONSE_MAKE);
	TestTrue(TEXT("A to B connects"), Schema->TryCreateConnection(NodeA->GetOutPin(), NodeB->GetInPin()));
	TestTrue(TEXT("B requires A"), B->Prerequisites == TArray<FGameplayTag>{ TagA });

	// Rejected: the reverse arrow (a loop), the same arrow dragged from the In pin, a quest without a tag, itself
	TestTrue(TEXT("B to A would loop"), Schema->CanCreateConnection(NodeB->GetOutPin(), NodeA->GetInPin()).Response == CONNECT_RESPONSE_DISALLOW);
	TestTrue(TEXT("A to B twice"), Schema->CanCreateConnection(NodeB->GetInPin(), NodeA->GetOutPin()).Response == CONNECT_RESPONSE_DISALLOW);
	TestTrue(TEXT("an untagged quest cannot be required"), Schema->CanCreateConnection(NodeC->GetOutPin(), NodeB->GetInPin()).Response == CONNECT_RESPONSE_DISALLOW);
	TestTrue(TEXT("a quest cannot require itself"), Schema->CanCreateConnection(NodeA->GetOutPin(), NodeA->GetInPin()).Response == CONNECT_RESPONSE_DISALLOW);
	TestTrue(TEXT("two inputs"), Schema->CanCreateConnection(NodeA->GetInPin(), NodeB->GetInPin()).Response == CONNECT_RESPONSE_DISALLOW);

	// A tag that no quest has stays in the Prerequisites through a sync
	C->Prerequisites.Add(TagRenamed);
	Graph->Sync();
	TestTrue(TEXT("an unknown tag is kept"), C->Prerequisites.Contains(TagRenamed));
	C->Prerequisites.Empty();

	// Rename A's tag: the arrow follows
	A->QuestTag = TagRenamed;
	Graph->ApplyTagRenames();
	Graph->Sync();
	TestTrue(TEXT("B requires the new tag"), B->Prerequisites == TArray<FGameplayTag>{ TagRenamed });
	TestEqual(TEXT("the arrow is still there"), NodeB->GetInPin()->LinkedTo.Num(), 1);

	// Cut the arrow: the Prerequisite goes
	Schema->BreakPinLinks(*NodeB->GetInPin(), true);
	TestTrue(TEXT("B requires nothing"), B->Prerequisites.IsEmpty());

	// Delete A: B no longer requires it, and the graph and Director lose it
	TestTrue(TEXT("A to B connects again"), Schema->TryCreateConnection(NodeA->GetOutPin(), NodeB->GetInPin()));
	Graph->DeleteQuest(*NodeA);
	TestTrue(TEXT("B no longer requires A"), B->Prerequisites.IsEmpty());
	TestEqual(TEXT("the Director lost A"), Director->Quests.Num(), 2);
	TestEqual(TEXT("the graph lost A"), Graph->GetQuestNodes().Num(), 2);

	return true;
}

#endif