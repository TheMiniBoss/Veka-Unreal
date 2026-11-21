// Copyright TheMiniBoss. All Rights Reserved.

#include "ViewEditor/VekaViewEditorToolkit.h"
#include "ViewEditor/SVekaViewPreview.h"
#include "CodeEditor/SVekaCodeEditor.h"
#include "SimpleMode/VekaViewEditorContext.h"
#include "SimpleMode/SVekaOutliner.h"
#include "SimpleMode/SVekaInspector.h"
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
const FName FVekaViewEditorToolkit::OutlinerTabId(TEXT("VekaViewEditor_Outliner"));
const FName FVekaViewEditorToolkit::PaletteTabId(TEXT("VekaViewEditor_Palette"));
const FName FVekaViewEditorToolkit::InspectorTabId(TEXT("VekaViewEditor_Inspector"));
const FName FVekaViewEditorToolkit::StateTabId(TEXT("VekaViewEditor_State"));

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

	// Simple mode: one context shared by every panel (selection, edits, read-only while the code has errors).
	Context = MakeShared<FVekaViewEditorContext>(InView);
	TWeakPtr<SVekaCodeEditor> WeakCode = CodeEditor;
	Context->ReadOnly = [WeakCode]()
	{
		const TSharedPtr<SVekaCodeEditor> Code = WeakCode.Pin();
		return Code.IsValid() && (Code->HasErrors() || Code->HasPendingExternalChange());
	};
	Preview->SetContext(Context);
	CodeEditor->SetContext(Context);
	Outliner = SNew(SVekaOutliner, Context.ToSharedRef());
	Palette = SNew(SVekaPalette, Context.ToSharedRef());
	Inspector = SNew(SVekaInspector, Context.ToSharedRef());
	TWeakPtr<SVekaViewPreview> WeakPreview = Preview;
	StatePanel = SNew(SVekaStatePanel, Context.ToSharedRef(), [WeakPreview]() -> UObject*
	{
		const TSharedPtr<SVekaViewPreview> Pinned = WeakPreview.Pin();
		return Pinned.IsValid() ? Pinned->GetModel() : nullptr;
	});

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("Standalone_VekaViewEditor_Layout_v3"))
		->AddArea(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.2f)
				->Split(FTabManager::NewStack()->SetSizeCoefficient(0.6f)->AddTab(OutlinerTabId, ETabState::OpenedTab))
				->Split(FTabManager::NewStack()->SetSizeCoefficient(0.4f)->AddTab(PaletteTabId, ETabState::OpenedTab)))
			->Split(FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.55f)
				->Split(FTabManager::NewStack()->SetSizeCoefficient(0.6f)->AddTab(ViewportTabId, ETabState::OpenedTab)->SetHideTabWell(true))
				->Split(FTabManager::NewStack()->SetSizeCoefficient(0.4f)->AddTab(CodeTabId, ETabState::OpenedTab)))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.25f)
				->AddTab(InspectorTabId, ETabState::OpenedTab)
				->AddTab(StateTabId, ETabState::OpenedTab)
				->AddTab(DetailsTabId, ETabState::OpenedTab)
				->SetForegroundTab(InspectorTabId)));

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
	auto Panel = [this, &InTabManager](FName Id, FText Label, TSharedPtr<SWidget> Content, const char* Icon)
	{
		InTabManager->RegisterTabSpawner(Id, FOnSpawnTab::CreateSP(this, &FVekaViewEditorToolkit::SpawnPanelTab, Label, Content))
			.SetDisplayName(Label)
			.SetGroup(WorkspaceMenuCategory.ToSharedRef())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), Icon));
	};
	Panel(OutlinerTabId, LOCTEXT("OutlinerTab", "Outliner"), Outliner, "LevelEditor.Tabs.Outliner");
	Panel(PaletteTabId, LOCTEXT("PaletteTab", "Palette"), Palette, "Kismet.Tabs.Palette");
	Panel(InspectorTabId, LOCTEXT("InspectorTab", "Inspector"), Inspector, "LevelEditor.Tabs.Details");
	Panel(StateTabId, LOCTEXT("StateTab", "State"), StatePanel, "Kismet.Tabs.Variables");
}

void FVekaViewEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(CodeTabId);
	InTabManager->UnregisterTabSpawner(OutlinerTabId);
	InTabManager->UnregisterTabSpawner(PaletteTabId);
	InTabManager->UnregisterTabSpawner(InspectorTabId);
	InTabManager->UnregisterTabSpawner(StateTabId);
}

TSharedRef<SDockTab> FVekaViewEditorToolkit::SpawnPanelTab(const FSpawnTabArgs& Args, FText Label, TSharedPtr<SWidget> Content)
{
	return SNew(SDockTab).Label(Label)[Content.ToSharedRef()];
}

void FVekaViewEditorToolkit::DocumentChanged()
{
	if (Context.IsValid())
	{
		Context->NotifyDocumentChanged();
	}
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
	DocumentChanged();
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
	if (Object == View.Get())
	{
		DocumentChanged();
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
