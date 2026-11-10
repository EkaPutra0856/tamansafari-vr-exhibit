// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Done / optional flags of one task, for the objective rule. */
struct FWzTaskStatus
{
	bool bOptional = false;
	bool bDone = false;
};

/** The quest rules as pure functions, shared by the subsystem, the data validation and the graph editor. */
namespace WzQuestLogic
{
	/** True when every prerequisite is completed. */
	template <typename TKey>
	bool IsUnlocked(const TArray<TKey>& Prerequisites, const TSet<TKey>& Completed)
	{
		for (const auto& Prerequisite : Prerequisites) if (!Completed.Contains(Prerequisite)) return false;

		return true;
	}

	/** True when Quest is already (indirectly) required by Prerequisite, so requiring Prerequisite for Quest closes a loop. */
	template <typename TKey>
	bool WouldCreateCycle(const TMap<TKey, TArray<TKey>>& PrerequisitesOf, const TKey& Quest, const TKey& Prerequisite)
	{
		// Walk what Prerequisite requires, looking for Quest
		TSet<TKey> Seen;
		TArray<TKey> Pending{ Prerequisite };
		while (!Pending.IsEmpty())
		{
			const TKey Next = Pending.Pop();
			if (Next == Quest) return true;

			bool bAlreadySeen = false;
			Seen.Add(Next, &bAlreadySeen);
			if (bAlreadySeen) continue;

			if (const TArray<TKey>* Requires = PrerequisitesOf.Find(Next)) Pending.Append(*Requires);
		}

		return false;
	}

	/** True when some quest (indirectly) requires itself. */
	template <typename TKey>
	bool HasCycle(const TMap<TKey, TArray<TKey>>& PrerequisitesOf)
	{
		for (const auto& [Quest, Requires] : PrerequisitesOf) for (const auto& Prerequisite : Requires) if (WouldCreateCycle(PrerequisitesOf, Quest, Prerequisite)) return true;

		return false;
	}

	/** True when no required task is still open. */
	inline bool IsObjectiveDone(const TArray<FWzTaskStatus>& Tasks)
	{
		for (const auto& Task : Tasks) if (!Task.bOptional && !Task.bDone) return false;

		return true;
	}
}