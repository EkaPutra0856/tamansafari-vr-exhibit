// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Director/WzNarrativeDirector.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"
#include "Quest/WzNarrativeConditions.h"
#include "Quest/WzQuest.h"
#include "Quest/WzQuestLogic.h"

namespace
{
	constexpr EAutomationTestFlags QuestTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzQuestUnlockTest, "NarrativeSystem.Quest.Unlock", QuestTestFlags)
bool FWzQuestUnlockTest::RunTest(const FString&)
{
	// A quest pointed at by several quests unlocks only when all of them are completed
	const TArray<int32> Prerequisites{ 1, 2 };

	TestTrue(TEXT("no prerequisites"), WzQuestLogic::IsUnlocked(TArray<int32>{}, TSet<int32>{}));
	TestFalse(TEXT("none completed"), WzQuestLogic::IsUnlocked(Prerequisites, TSet<int32>{}));
	TestFalse(TEXT("one of two"), WzQuestLogic::IsUnlocked(Prerequisites, TSet<int32>{ 1 }));
	TestTrue(TEXT("both"), WzQuestLogic::IsUnlocked(Prerequisites, TSet<int32>{ 1, 2, 3 }));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzQuestCycleTest, "NarrativeSystem.Quest.Cycle", QuestTestFlags)
bool FWzQuestCycleTest::RunTest(const FString&)
{
	// 2 requires 1, 3 requires 2
	TMap<int32, TArray<int32>> Requires;
	Requires.Add(2, { 1 });
	Requires.Add(3, { 2 });

	TestTrue(TEXT("a quest cannot require itself"), WzQuestLogic::WouldCreateCycle(Requires, 1, 1));
	TestTrue(TEXT("1 requiring 3 closes the loop"), WzQuestLogic::WouldCreateCycle(Requires, 1, 3));
	TestFalse(TEXT("3 requiring 1 is fine"), WzQuestLogic::WouldCreateCycle(Requires, 3, 1));
	TestFalse(TEXT("a chain has no cycle"), WzQuestLogic::HasCycle(Requires));

	// Close the loop
	Requires.Add(1, { 3 });
	TestTrue(TEXT("a loop is found"), WzQuestLogic::HasCycle(Requires));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzQuestObjectiveDoneTest, "NarrativeSystem.Quest.ObjectiveDone", QuestTestFlags)
bool FWzQuestObjectiveDoneTest::RunTest(const FString&)
{
	// Only required tasks hold the objective back
	TestTrue(TEXT("no tasks"), WzQuestLogic::IsObjectiveDone(TArray<FWzTaskStatus>{}));

	const TArray<FWzTaskStatus> OpenRequired{ { false, false } };
	TestFalse(TEXT("open required"), WzQuestLogic::IsObjectiveDone(OpenRequired));

	const TArray<FWzTaskStatus> DoneRequired{ { false, true } };
	TestTrue(TEXT("done required"), WzQuestLogic::IsObjectiveDone(DoneRequired));

	const TArray<FWzTaskStatus> OpenOptional{ { true, false } };
	TestTrue(TEXT("open optional"), WzQuestLogic::IsObjectiveDone(OpenOptional));

	const TArray<FWzTaskStatus> Mixed{ { true, true }, { false, false } };
	TestFalse(TEXT("optional done, required open"), WzQuestLogic::IsObjectiveDone(Mixed));

	return true;
}

#if WITH_EDITOR
namespace
{
	bool HasDestroyActorsTagIssue(const UWzNarrativeDirector* Director)
	{
		FDataValidationContext Context;
		Director->IsDataValid(Context);

		return Context.GetIssues().ContainsByPredicate([](const FDataValidationContext::FIssue& Issue) { return Issue.Message.ToString().Contains(TEXT("Destroy Actors")); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWzDestroyActorsTagValidationTest, "NarrativeSystem.Quest.DestroyActorsTagValidation", QuestTestFlags)
bool FWzDestroyActorsTagValidationTest::RunTest(const FString&)
{
	// A Destroy Actors task without a tag counts nothing, so the Director must flag it
	auto* Director = NewObject<UWzNarrativeDirector>();
	auto* Quest = NewObject<UWzQuest>(Director);
	auto* Task = NewObject<UWzDestroyActorsCondition>(Quest);
	Quest->Objectives.AddDefaulted_GetRef().Tasks.Add(Task);
	Director->Quests.Add(Quest);

	TestTrue(TEXT("no tag is flagged"), HasDestroyActorsTagIssue(Director));

	// A real tag clears it (skipped when the project has none to borrow)
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Quest"), false);
	if (Tag.IsValid())
	{
		Task->Tag = Tag;
		TestFalse(TEXT("tagged is fine"), HasDestroyActorsTagIssue(Director));
	}

	return true;
}
#endif

#endif