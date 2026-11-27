// Copyright TheMiniBoss. All Rights Reserved.

#include "ViewEditor/VekaViewEditorToolkit.h"
#include "ViewEditor/SVekaViewPreview.h"
#include "CodeEditor/SVekaCodeEditor.h"
#include "SimpleMode/VekaViewEditorContext.h"
#include "SimpleMode/SVekaOutliner.h"
#include "SimpleMode/SVekaInspector.h"
#include "TestMode/SVekaTestPanels.h"
#include "Symbols/SVekaSymbolLibrary.h"
#include "Hub/VekaHub.h"
#include "Symbols/VekaSymbol.h"
#include "Colora/VekaPaintAsset.h"
#include "View/VekaViewAsset.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
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
const FName FVekaViewEditorToolkit::TestTabId(TEXT("VekaViewEditor_Test"));
const FName FVekaViewEditorToolkit::TimelineTabId(TEXT("VekaViewEditor_Timeline"));

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

	TestPanel = SNew(SVekaTestPanel, Context.ToSharedRef(), Preview.ToSharedRef());
	TimelinePanel = SNew(SVekaTimelinePanel, Context.ToSharedRef(), Preview.ToSharedRef());
	SidePanel = MakeSidePanel();

	// Three columns, no tab to hunt for: Outliner | viewport over code | one side panel. The Palette, State, Test and
	// Details tabs of v4 now live in the Outliner's "+ Add" button and in the side panel.
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("Standalone_VekaViewEditor_Layout_v5"))
		->AddArea(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.18f)->AddTab(OutlinerTabId, ETabState::OpenedTab)->SetHideTabWell(true))
			->Split(FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.57f)
				->Split(FTabManager::NewStack()->SetSizeCoefficient(0.62f)->AddTab(ViewportTabId, ETabState::OpenedTab)->SetHideTabWell(true))
				->Split(FTabManager::NewStack()->SetSizeCoefficient(0.38f)->AddTab(CodeTabId, ETabState::OpenedTab)->AddTab(TimelineTabId, ETabState::OpenedTab)
					->SetForegroundTab(CodeTabId)))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.25f)->AddTab(InspectorTabId, ETabState::OpenedTab)->SetHideTabWell(true)));

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
	// Still in Window > Veka View Editor for whoever wants the palette docked (the "+ Add" menu has its own).
	Panel(PaletteTabId, LOCTEXT("PaletteTab", "Palette"), Palette, "Kismet.Tabs.Palette");
	Panel(InspectorTabId, LOCTEXT("InspectorTab", "Inspector"), SidePanel, "LevelEditor.Tabs.Details");
	Panel(TimelineTabId, LOCTEXT("TimelineTab", "Timeline"), TimelinePanel, "Sequencer.Tabs.SequencerMain");
}

bool FVekaViewEditorToolkit::IsTesting() const
{
	return TestPanel.IsValid() && TestPanel->IsPlaying();
}

void FVekaViewEditorToolkit::SetTesting(bool bTesting)
{
	if (TestPanel.IsValid())
	{
		TestPanel->SetPlaying(bTesting);
	}
}

TSharedRef<SWidget> FVekaViewEditorToolkit::MakeSidePanel()
{
	// Asset settings (preview model, component library...) under the view's own states, params and events.
	TSharedRef<SWidget> ViewPage = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(10.f, 8.f, 10.f, 2.f))
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("NormalFontBold"))
			.Text(LOCTEXT("ViewPageTitle", "This view"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(10.f, 0.f, 10.f, 4.f))
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("ViewPageHint", "Select a view in the Outliner or the viewport to edit it. Here: the states, params and events of the whole view."))
		]
		+ SVerticalBox::Slot().FillHeight(0.6f)
		[
			StatePanel.ToSharedRef()
		]
		+ SVerticalBox::Slot().FillHeight(0.4f)
		[
			SNew(SExpandableArea)
			.AreaTitle(LOCTEXT("SettingsArea", "Asset settings"))
			.InitiallyCollapsed(true)
			.BodyContent()
			[
				Details.ToSharedRef()
			]
		];

	return SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([this]()
		{
			if (IsTesting())
			{
				return 2;
			}
			return Context.IsValid() && !Context->GetSelectedNodes().IsEmpty() ? 0 : 1;
		})
		+ SWidgetSwitcher::Slot()[Inspector.ToSharedRef()]
		+ SWidgetSwitcher::Slot()[ViewPage]
		+ SWidgetSwitcher::Slot()[TestPanel.ToSharedRef()];
}

void FVekaViewEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(CodeTabId);
	InTabManager->UnregisterTabSpawner(OutlinerTabId);
	InTabManager->UnregisterTabSpawner(PaletteTabId);
	InTabManager->UnregisterTabSpawner(InspectorTabId);
	InTabManager->UnregisterTabSpawner(TimelineTabId);
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
	// Two things to do with a view: design it (clicks select) or test it (clicks use it). The preview refreshes by itself.
	Builder.BeginSection(TEXT("Veka"));
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateLambda([this]() { SetTesting(!IsTesting()); }), FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]() { return IsTesting(); })),
		NAME_None,
		TAttribute<FText>::CreateLambda([this]() { return IsTesting() ? LOCTEXT("StopTest", "Stop") : LOCTEXT("Test", "Test"); }),
		LOCTEXT("TestTooltip", "Test: use the view in the viewport (clicks, hover, focus), watch its states and events on the right. Stop: back to designing (clicks select)."),
		TAttribute<FSlateIcon>::CreateLambda([this]() { return FSlateIcon(FAppStyle::GetAppStyleSetName(), IsTesting() ? "PlayWorld.StopPlaySession" : "Icons.Play"); }),
		EUserInterfaceActionType::ToggleButton);
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateLambda([this]() { Preview->ResetState(); })), NAME_None, LOCTEXT("ResetState", "Reset"),
		LOCTEXT("ResetStateTooltip", "Back to the declared initial states"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"));
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateLambda([this]() { Preview->ZoomToFit(); })), NAME_None, LOCTEXT("Fit", "Fit"),
		LOCTEXT("FitTooltip", "Zoom to fit (F)"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Fullscreen"));
	Builder.AddSeparator();
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(Veka::Editor::Hub::TabId); })), NAME_None, LOCTEXT("Hub", "Hub"),
		LOCTEXT("HubTooltip", "Veka Hub: components, colors, symbols, examples and documentation"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout"));
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
	// This view, a view it uses as a component, a symbol or a paint it may draw.
	if (Cast<UVekaView>(Object) || Cast<UVekaSymbol>(Object) || Cast<UVekaPaintAsset>(Object))
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
