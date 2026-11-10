// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/WzDirectorEditorToolkit.h"

#include "Director/WzNarrativeDirector.h"
#include "Editor.h"
#include "Framework/Commands/GenericCommands.h"
#include "Graph/WzDirectorGraph.h"
#include "Graph/WzQuestGraphNode.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Quest/WzQuest.h"
#include "ScopedTransaction.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "WzDirectorEditor"

namespace
{
	const FName AppIdentifier{ TEXT("WzDirectorEditorApp") };
	const FName GraphTabId{ TEXT("WzDirectorEditor_Graph") };
	const FName DetailsTabId{ TEXT("WzDirectorEditor_Details") };
}

// ====== Lifecycle ====== //

FWzDirectorEditorToolkit::~FWzDirectorEditorToolkit()
{
	if (GEditor) GEditor->UnregisterForUndo(this);
}

void FWzDirectorEditorToolkit::Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InToolkitHost, UWzNarrativeDirector* InDirector)
{
	Director = InDirector;
	Director->SetFlags(RF_Transactional);

	// The graph is made on the first open and always re-read from the quests
	UWzDirectorGraph* Graph = UWzDirectorGraph::FindOrCreate(*Director);
	Graph->Sync();

	// Delete is the only command: two nodes must never share a quest
	GraphCommands = MakeShared<FUICommandList>();
	GraphCommands->MapAction(FGenericCommands::Get().Delete, FExecuteAction::CreateSP(this, &FWzDirectorEditorToolkit::DeleteSelectedNodes), FCanExecuteAction::CreateSP(this, &FWzDirectorEditorToolkit::CanDeleteSelectedNodes));

	// Graph
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FWzDirectorEditorToolkit::HandleSelectionChanged);
	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("GraphCorner", "DIRECTOR");
	GraphEditor = SNew(SGraphEditor)
		.AdditionalCommands(GraphCommands)
		.GraphToEdit(Graph)
		.GraphEvents(Events)
		.Appearance(Appearance);

	// Details
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bHideSelectionTip = true;
	DetailsView = PropertyModule.CreateDetailView(DetailsArgs);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FWzDirectorEditorToolkit::HandlePropertiesChanged);
	DetailsView->SetObject(Director.Get());

	// Layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("WzDirectorEditor_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.72f)->SetHideTabWell(true)->AddTab(GraphTabId, ETabState::OpenedTab))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.28f)->AddTab(DetailsTabId, ETabState::OpenedTab))
		);
	InitAssetEditor(Mode, InToolkitHost, AppIdentifier, Layout, true, true, Director.Get());
	RegenerateMenusAndToolbars();

	// Undo and redo bring the graph back in line
	GEditor->RegisterForUndo(this);
}

void FWzDirectorEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu", "Director"));
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FWzDirectorEditorToolkit::SpawnGraphTab))
		.SetDisplayName(LOCTEXT("GraphTab", "Graph"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FWzDirectorEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FWzDirectorEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

FName FWzDirectorEditorToolkit::GetToolkitFName() const
{
	return AppIdentifier;
}

FText FWzDirectorEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Narrative Director");
}

FString FWzDirectorEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Director ").ToString();
}

FLinearColor FWzDirectorEditorToolkit::GetWorldCentricTabColorScale() const
{
	return { 0.55f, 0.32f, 0.05f, 0.5f };
}

void FWzDirectorEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Director);
}

FString FWzDirectorEditorToolkit::GetReferencerName() const
{
	return TEXT("FWzDirectorEditorToolkit");
}

void FWzDirectorEditorToolkit::PostUndo(const bool bSuccess)
{
	// The quest shown may be gone or back: show what the selection stands for now
	Refresh();
	HandleSelectionChanged(GraphEditor->GetSelectedNodes());
}

void FWzDirectorEditorToolkit::PostRedo(const bool bSuccess)
{
	PostUndo(bSuccess);
}

// ====== References ====== //

UWzDirectorGraph* FWzDirectorEditorToolkit::GetGraph() const
{
	return UWzDirectorGraph::FindOrCreate(*Director);
}

// ====== Tabs ====== //

TSharedRef<SDockTab> FWzDirectorEditorToolkit::SpawnGraphTab(const FSpawnTabArgs& Args) const
{
	return SNew(SDockTab)
		.Label(LOCTEXT("GraphTabLabel", "Quests"))
		[
			GraphEditor.ToSharedRef()
		];
}

TSharedRef<SDockTab> FWzDirectorEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args) const
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabLabel", "Details"))
		[
			DetailsView.ToSharedRef()
		];
}

// ====== Events ====== //

void FWzDirectorEditorToolkit::HandleSelectionChanged(const FGraphPanelSelectionSet& Selection)
{
	// One quest selected shows that quest, anything else the Director
	const UWzQuestGraphNode* Node = Selection.Num() == 1 ? Cast<UWzQuestGraphNode>(*Selection.CreateConstIterator()) : nullptr;
	UObject* Shown = Director.Get();
	if (Node && Node->Quest && Director->Quests.Contains(Node->Quest)) Shown = Node->Quest.Get();

	DetailsView->SetObject(Shown);
}

void FWzDirectorEditorToolkit::HandlePropertiesChanged(const FPropertyChangedEvent& Event)
{
	Refresh();
}

void FWzDirectorEditorToolkit::DeleteSelectedNodes()
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteQuests", "Delete Quests"));

	// Copy the selection first: deleting changes it
	const FGraphPanelSelectionSet Selected = GraphEditor->GetSelectedNodes();
	GraphEditor->ClearSelectionSet();
	for (const auto Object : Selected) if (UWzQuestGraphNode* Node = Cast<UWzQuestGraphNode>(Object)) GetGraph()->DeleteQuest(*Node);

	GraphEditor->NotifyGraphChanged();
}

bool FWzDirectorEditorToolkit::CanDeleteSelectedNodes() const
{
	return GraphEditor->GetSelectedNodes().Num() > 0;
}

// ====== Helpers ====== //

void FWzDirectorEditorToolkit::Refresh()
{
	// Tags first: the arrows still exist and map by quest to the new tag
	UWzDirectorGraph* Graph = GetGraph();
	Graph->ApplyTagRenames();
	Graph->Sync();
	GraphEditor->NotifyGraphChanged();
}

#undef LOCTEXT_NAMESPACE