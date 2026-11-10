// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "GraphEditor.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/GCObject.h"

class IDetailsView;
class UWzDirectorGraph;
class UWzNarrativeDirector;

/** The Director editor: the quest graph on the left, the details of the selected quest (or of the Director) on the right. */
class FWzDirectorEditorToolkit : public FAssetEditorToolkit, public FGCObject, public FEditorUndoClient
{
public:
	// ====== Lifecycle ====== //

	virtual ~FWzDirectorEditorToolkit() override;

	void Init(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InToolkitHost, UWzNarrativeDirector* InDirector);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** The toolkit is no UObject: this keeps the Director, and so its graph, alive. */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	// ====== References ====== //

	TObjectPtr<UWzNarrativeDirector> Director;

	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<IDetailsView> DetailsView;

	/** The graph's commands: only Delete. Copy, cut, paste and duplicate stay unbound, since two nodes cannot share a quest. */
	TSharedPtr<FUICommandList> GraphCommands;

	UWzDirectorGraph* GetGraph() const;

private:
	// ====== Tabs ====== //

	TSharedRef<SDockTab> SpawnGraphTab(const FSpawnTabArgs& Args) const;
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args) const;

private:
	// ====== Events ====== //

	void HandleSelectionChanged(const FGraphPanelSelectionSet& Selection);

	/** A quest was edited in the details: follow its tag, and redraw its node. */
	void HandlePropertiesChanged(const FPropertyChangedEvent& Event);

	void DeleteSelectedNodes();
	bool CanDeleteSelectedNodes() const;

private:
	// ====== Helpers ====== //

	/** Re-reads the Director into the graph and redraws it. */
	void Refresh();
};