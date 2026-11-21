// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "EditorUndoClient.h"

class UVekaView;
class SVekaViewPreview;
class IDetailsView;
class FToolBarBuilder;
struct FVekaDocument;
struct FPropertyChangedEvent;

namespace Veka::Editor
{
	/**
	 * Replaces the document of a view as one undoable step (Edit > Undo, Ctrl+Z). Every editing tool of the Veka Editor
	 * goes through here; open editors refresh.
	 */
	void ApplyDocument(UVekaView* View, const FVekaDocument& Document, const FText& Description);
}

/**
 * The Veka View asset editor (double-click a Veka View): Viewport and Details tabs, toolbar, undo / redo.
 * 3c adds the Code tab, 3d the Simple mode.
 */
class FVekaViewEditorToolkit : public FAssetEditorToolkit, public FSelfRegisteringEditorUndoClient
{
public:
	static const FName AppIdentifier;
	static const FName ViewportTabId;
	static const FName DetailsTabId;

	virtual ~FVekaViewEditorToolkit() override;

	void InitEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVekaView* View);

	UVekaView* GetView() const { return View.Get(); }
	TSharedPtr<SVekaViewPreview> GetPreview() const { return Preview; }

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
	void FillToolbar(FToolBarBuilder& Builder);
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);
	void RefreshPreview();

	TWeakObjectPtr<UVekaView> View;
	TSharedPtr<SVekaViewPreview> Preview;
	TSharedPtr<IDetailsView> Details;
	FDelegateHandle PropertyChangedHandle;
};
