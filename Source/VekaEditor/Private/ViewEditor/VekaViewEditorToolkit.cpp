// Copyright TheMiniBoss. All Rights Reserved.

#include "ViewEditor/VekaViewEditorToolkit.h"
#include "ViewEditor/SVekaViewPreview.h"
#include "CodeEditor/SVekaCodeEditor.h"
#include "View/VekaViewAsset.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Widgets/Docking/SDockTab.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "VekaViewEditor"

const FName FVekaViewEditorToolkit::AppIdentifier(TEXT("VekaViewEditorApp"));
const FName FVekaViewEditorToolkit::ViewportTabId(TEXT("VekaViewEditor_Viewport"));
const FName FVekaViewEditorToolkit::DetailsTabId(TEXT("VekaViewEditor_Details"));
const FName FVekaViewEditorToolkit::CodeTabId(TEXT("VekaViewEditor_Code"));

namespace Veka::Editor
{
	void ApplyDocument(UVekaView* View, const FVekaDocument& Document, const FText& Description, const FString* SourceText)
	{
		if (!View)
		{
			return;
		}
		const FScopedTransaction Transaction(Description);
		View->SetFlags(RF_Transactional);
		View->Modify();
		View->Document = Document;
#if WITH_EDITORONLY_DATA
		View->SourceText = SourceText ? *SourceText : FString();
#endif
		FPropertyChangedEvent Event(FindFProperty<FProperty>(UVekaView::StaticClass(), GET_MEMBER_NAME_CHECKED(UVekaView, Document)), EPropertyChangeType::ValueSet);
		View->PostEditChangeProperty(Event);
	}
}

FVekaViewEditorToolkit::~FVekaViewEditorToolkit()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
}

void FVekaViewEditorToolkit::InitEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVekaView* InView)
{
	View = InView;
	// Edits must be recorded for undo (assets made before the editor existed may lack the flag).
	InView->SetFlags(RF_Transactional);

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bHideSelectionTip = true;
	Details = PropertyEditor.CreateDetailView(DetailsArgs);
	Details->SetObject(InView);

	Preview = SNew(SVekaViewPreview).View(InView);
	CodeEditor = SNew(SVekaCodeEditor).View(InView);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("Standalone_VekaViewEditor_Layout_v2"))
		->AddArea(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.36f)->AddTab(CodeTabId, ETabState::OpenedTab))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.44f)->AddTab(ViewportTabId, ETabState::OpenedTab))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.20f)->AddTab(DetailsTabId, ETabState::OpenedTab)));

	TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension(TEXT("Asset"), EExtensionHook::After, GetToolkitCommands(), FToolBarExtensionDelegate::CreateSP(this, &FVekaViewEditorToolkit::FillToolbar));
	AddToolbarExtender(ToolbarExtender);

	InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, Layout, true, true, InView);

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &FVekaViewEditorToolkit::HandleObjectPropertyChanged);
}

FName FVekaViewEditorToolkit::GetToolkitFName() const
{
	return TEXT("VekaViewEditor");
}

FText FVekaViewEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Veka View Editor");
}

FString FVekaViewEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("Veka View ");
}

FLinearColor FVekaViewEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.35f, 0.25f, 0.95f, 0.5f);
}

void FVekaViewEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu", "Veka View Editor"));
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &FVekaViewEditorToolkit::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FVekaViewEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
	InTabManager->RegisterTabSpawner(CodeTabId, FOnSpawnTab::CreateSP(this, &FVekaViewEditorToolkit::SpawnCodeTab))
		.SetDisplayName(LOCTEXT("CodeTab", "Code"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Code"));
}

void FVekaViewEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(CodeTabId);
}

TSharedRef<SDockTab> FVekaViewEditorToolkit::SpawnViewportTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("ViewportTab", "Viewport"))[Preview.ToSharedRef()];
}

TSharedRef<SDockTab> FVekaViewEditorToolkit::SpawnCodeTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("CodeTab", "Code"))[CodeEditor.ToSharedRef()];
}

TSharedRef<SDockTab> FVekaViewEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("DetailsTab", "Details"))[Details.ToSharedRef()];
}

void FVekaViewEditorToolkit::FillToolbar(FToolBarBuilder& Builder)
{
	Builder.BeginSection(TEXT("Veka"));
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FVekaViewEditorToolkit::RefreshPreview)), NAME_None, LOCTEXT("Refresh", "Refresh"),
		LOCTEXT("RefreshTooltip", "Rebuild the preview from the asset"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"));
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateLambda([this]() { Preview->ResetState(); })), NAME_None, LOCTEXT("ResetState", "Reset state"),
		LOCTEXT("ResetStateTooltip", "Back to the declared initial states"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Reset"));
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateLambda([this]() { Preview->ZoomToFit(); })), NAME_None, LOCTEXT("Fit", "Fit"),
		LOCTEXT("FitTooltip", "Zoom to fit the screen (F)"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Fullscreen"));
	Builder.EndSection();
}

void FVekaViewEditorToolkit::PostUndo(bool bSuccess)
{
	RefreshPreview();
	if (CodeEditor.IsValid())
	{
		CodeEditor->OnDocumentChanged();
	}
	if (Details.IsValid())
	{
		Details->ForceRefresh();
	}
}

void FVekaViewEditorToolkit::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

void FVekaViewEditorToolkit::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	// This view, or a view it uses as a component.
	if (Cast<UVekaView>(Object))
	{
		RefreshPreview();
	}
	// Edits from the Details panel or another tool (the Code tab ignores its own).
	if (Object == View.Get() && CodeEditor.IsValid())
	{
		CodeEditor->OnDocumentChanged();
	}
}

void FVekaViewEditorToolkit::RefreshPreview()
{
	if (Preview.IsValid())
	{
		Preview->Refresh();
	}
}

#undef LOCTEXT_NAMESPACE
