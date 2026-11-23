// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "EditorUndoClient.h"

class UVekaView;
class SVekaViewPreview;
class SVekaCodeEditor;
class SVekaOutliner;
class SVekaPalette;
class SVekaInspector;
class SVekaStatePanel;
class FVekaViewEditorContext;
class SVekaTestPanel;
class SVekaTimelinePanel;
class IDetailsView;
class FToolBarBuilder;
struct FVekaDocument;
struct FPropertyChangedEvent;

namespace Veka::Editor
{
	/**
	 * Replaces the document of a view as one undoable step (Edit > Undo, Ctrl+Z). Every editing tool of the Veka Editor
	 * goes through here; open editors refresh. SourceText: the code the document came from (Code tab), kept in the same
	 * step; other tools pass none and the stored code is dropped (the Code tab then prints the document).
	 */
	void ApplyDocument(UVekaView* View, const FVekaDocument& Document, const FText& Description, const FString* SourceText = nullptr);
}

/**
 * The Veka View asset editor (double-click a Veka View): Simple mode (Outliner, Palette, Inspector, State: 3d), Code
 * (3c), Viewport and Details tabs, toolbar, undo / redo. Selection is shared by the viewport, the outliner and the code.
 */
class FVekaViewEditorToolkit : public FAssetEditorToolkit, public FSelfRegisteringEditorUndoClient
{
public:
	static const FName AppIdentifier;
	static const FName ViewportTabId;
	static const FName DetailsTabId;
	static const FName CodeTabId;
	static const FName OutlinerTabId;
	static const FName PaletteTabId;
	static const FName InspectorTabId;
	static const FName StateTabId;
	static const FName TestTabId;
	static const FName TimelineTabId;

	virtual ~FVekaViewEditorToolkit() override;

	void InitEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVekaView* View);

	UVekaView* GetView() const { return View.Get(); }
	TSharedPtr<SVekaViewPreview> GetPreview() const { return Preview; }
	TSharedPtr<SVekaCodeEditor> GetCodeEditor() const { return CodeEditor; }
	TSharedPtr<FVekaViewEditorContext> GetContext() const { return Context; }
	TSharedPtr<SVekaOutliner> GetOutliner() const { return Outliner; }
	TSharedPtr<SVekaInspector> GetInspector() const { return Inspector; }
	TSharedPtr<SVekaTestPanel> GetTestPanel() const { return TestPanel; }
	TSharedPtr<SVekaTimelinePanel> GetTimelinePanel() const { return TimelinePanel; }

	// FAssetEditorToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnCodeTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnPanelTab(const FSpawnTabArgs& Args, FText Label, TSharedPtr<SWidget> Content);
	void DocumentChanged();
	void FillToolbar(FToolBarBuilder& Builder);
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);
	void RefreshPreview();

	TWeakObjectPtr<UVekaView> View;
	TSharedPtr<SVekaViewPreview> Preview;
	TSharedPtr<IDetailsView> Details;
	TSharedPtr<SVekaCodeEditor> CodeEditor;
	TSharedPtr<FVekaViewEditorContext> Context;
	TSharedPtr<SVekaOutliner> Outliner;
	TSharedPtr<SVekaPalette> Palette;
	TSharedPtr<SVekaInspector> Inspector;
	TSharedPtr<SVekaStatePanel> StatePanel;
	TSharedPtr<SVekaTestPanel> TestPanel;
	TSharedPtr<SVekaTimelinePanel> TimelinePanel;
	FDelegateHandle PropertyChangedHandle;
};
