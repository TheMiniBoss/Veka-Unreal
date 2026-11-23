// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"
#include "Core/VekaPaint.h"

class UVekaPaintAsset;
class SVekaHost;
class SVerticalBox;
class IDetailsView;
class SVekaMeshHandles;

namespace Veka::Editor::Mesh
{
	/** New Width x Height grid on regular positions, colors sampled from the current paint (nothing jumps). */
	void Resample(FVekaPaint& Paint, int32 Width, int32 Height);
	/** Moves a point (unit space, kept in 0..1). */
	void MovePoint(FVekaMeshGradient& Mesh, int32 Index, const FVector2D& Position);
	/** Index of the point nearest to Position within Radius (unit space), or INDEX_NONE. */
	int32 PointAt(const FVekaMeshGradient& Mesh, const FVector2D& Position, double Radius);
}

/**
 * Mesh gradient editor: the paint drawn live, points dragged with the mouse over the lattice, double-click (or the
 * swatch) to pick a point's color, a Colora token per point, rows / columns added or removed (resampled), smooth
 * interpolation and color space. Other paint types are edited in Details (and can be turned into a mesh).
 */
class SVekaPaintEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVekaPaintEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UVekaPaintAsset* InAsset);

	void Rebuild();
	/** One undo step on the asset's paint. */
	void Edit(const FText& Description, TFunctionRef<void(FVekaPaint&)> Change);
	int32 GetSelectedPoint() const { return SelectedPoint; }
	void SelectPoint(int32 Index);
	/** A drag in progress: the canvas shows Preview, the asset is written on release. */
	void PreviewMove(int32 Index, const FVector2D& Position);
	void CommitMove(int32 Index, const FVector2D& Position);
	void PickColor(int32 Index);

	const FVekaPaint& GetDisplayedPaint() const { return Displayed; }

private:
	TSharedRef<SWidget> MakeToolbar();
	void RebuildPointPanel();

	TWeakObjectPtr<UVekaPaintAsset> Asset;
	TSharedPtr<SVekaHost> Canvas;
	TSharedPtr<SVekaMeshHandles> Handles;
	TSharedPtr<SVerticalBox> PointPanel;
	FVekaPaint Displayed;
	int32 SelectedPoint = INDEX_NONE;
};

class FVekaPaintEditorToolkit : public FAssetEditorToolkit, public FSelfRegisteringEditorUndoClient
{
public:
	static const FName EditorTabId;
	static const FName DetailsTabId;

	void InitEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVekaPaintAsset* Paint);
	TSharedPtr<SVekaPaintEditor> GetEditor() const { return Editor; }

	virtual FName GetToolkitFName() const override { return TEXT("VekaPaintEditor"); }
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("Veka Paint "); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.85f, 0.35f, 0.65f, 0.5f); }
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

private:
	TWeakObjectPtr<UVekaPaintAsset> Asset;
	TSharedPtr<SVekaPaintEditor> Editor;
	TSharedPtr<IDetailsView> Details;
	FDelegateHandle PropertyHandle;
};
