// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "WzDirectorGraph.generated.h"

class UWzNarrativeDirector;
class UWzQuest;
class UWzQuestGraphNode;

/** What the Director editor shows. The Director's quests are the truth: this graph only mirrors them and keeps the node positions. */
UCLASS()
class UWzDirectorGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	// ====== Getters ====== //

	/** The graph of the Director, created on the first open. */
	static UWzDirectorGraph* FindOrCreate(UWzNarrativeDirector& Director);

	UWzNarrativeDirector* GetDirector() const;

	/** Every quest node, in graph order. */
	TArray<UWzQuestGraphNode*> GetQuestNodes() const;

	/** The node of the quest, or null. */
	UWzQuestGraphNode* FindNode(const UWzQuest* Quest) const;

	/** True when the quest has a tag no other quest shares: only then can others require it. */
	bool HasUniqueTag(const UWzQuest& Quest) const;

	/** Why the dependent cannot require the prerequisite, or empty when it can. */
	FText GetLinkError(const UWzQuestGraphNode& Prerequisite, const UWzQuestGraphNode& Dependent) const;

public:
	// ====== Sync ====== //

	/** Makes the graph match the Director: a node per quest, none for removed ones, an arrow per known Prerequisite. */
	void Sync();

	/** Rewrites the Prerequisites that point at a renamed quest tag. Run before Sync, or the arrows are lost. */
	void ApplyTagRenames();

public:
	// ====== Editing ====== //

	/** Adds a quest to the Director and its node to the graph. The caller opens the transaction. */
	UWzQuestGraphNode* AddQuest(const FVector2f& Location, bool bSelectNewNode);

	/** Removes the quest from the Director and from every Prerequisites. The caller opens the transaction. */
	void DeleteQuest(UWzQuestGraphNode& Node);

	/** Writes the arrow into the dependent's Prerequisites. Only that one tag is touched. */
	void AddPrerequisite(const UWzQuestGraphNode& Prerequisite, UWzQuestGraphNode& Dependent);

	void RemovePrerequisite(const UWzQuestGraphNode& Prerequisite, UWzQuestGraphNode& Dependent);

private:
	// ====== Helpers ====== //

	UWzQuestGraphNode* CreateQuestNode(UWzQuest& Quest, const FVector2f& Location, bool bSelectNewNode);
};