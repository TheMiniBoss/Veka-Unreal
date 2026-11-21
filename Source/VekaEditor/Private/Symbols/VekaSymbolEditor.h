// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"
#include "Core/VekaElement.h"

class UVekaSymbol;
class SVekaHost;
class SVerticalBox;
class IDetailsView;
struct FVekaSymbolDefinition;

/**
 * Symbol Editor: the symbol on the 24 x 24 keyline grid at any weight (layers tinted by role, the selected one in
 * orange), its layers (kind, role, variable layer, fillable, stroke scale, color; reorder, delete), and previews in
 * every weight, rendering mode, scale and variant, with the variable value.
 */
class SVekaSymbolEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaSymbolEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UVekaSymbol* InSymbol);

	void Rebuild();
	/** One undo step on the symbol's definition. */
	void Edit(const FText& Description, TFunctionRef<void(FVekaSymbolDefinition&)> Change);
	void SelectLayer(int32 Layer);
	int32 GetSelectedLayer() const { return SelectedLayer; }

	/** The canvas element (grid + layers) for a weight, used by tests. */
	static FVekaElement MakeCanvas(const FVekaSymbolDefinition& Definition, float Weight, int32 SelectedLayer, float Size);

private:
	TSharedRef<SWidget> MakeLayerList();
	TSharedRef<SWidget> MakeLayerProperties();
	FVekaElement MakePreviews() const;

	TWeakObjectPtr<UVekaSymbol> Symbol;
	TSharedPtr<SVekaHost> Canvas;
	TSharedPtr<SVekaHost> Previews;
	TSharedPtr<SVerticalBox> Layers;
	TSharedPtr<SVerticalBox> Properties;
	int32 SelectedLayer = 0;
	float Weight = 0.4f;
	float VariableValue = 1.f;
};

class FVekaSymbolEditorToolkit : public FAssetEditorToolkit, public FSelfRegisteringEditorUndoClient
{
public:
	static const FName EditorTabId;
	static const FName DetailsTabId;

	void InitEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVekaSymbol* Symbol);
	TSharedPtr<SVekaSymbolEditor> GetEditor() const { return Editor; }

	virtual FName GetToolkitFName() const override { return TEXT("VekaSymbolEditor"); }
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("Veka Symbol "); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.35f, 0.82f, 0.78f, 0.5f); }
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

private:
	TWeakObjectPtr<UVekaSymbol> Symbol;
	TSharedPtr<SVekaSymbolEditor> Editor;
	TSharedPtr<IDetailsView> Details;
	FDelegateHandle PropertyHandle;
};
