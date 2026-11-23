// Copyright TheMiniBoss. All Rights Reserved.

#include "ViewEditor/VekaViewAssetDefinition.h"
#include "ViewEditor/VekaViewEditorToolkit.h"
#include "View/VekaViewAsset.h"
#include "Slate/SVekaHost.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "VekaViewEditor"

FText UAssetDefinition_VekaView::GetAssetDisplayName() const
{
	return LOCTEXT("AssetName", "Veka View");
}

FLinearColor UAssetDefinition_VekaView::GetAssetColor() const
{
	return FLinearColor(0.35f, 0.25f, 0.95f);
}

TSoftClassPtr<UObject> UAssetDefinition_VekaView::GetAssetClass() const
{
	return UVekaView::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_VekaView::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::UI };
	return Categories;
}

EAssetCommandResult UAssetDefinition_VekaView::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UVekaView* View : OpenArgs.LoadObjects<UVekaView>())
	{
		const TSharedRef<FVekaViewEditorToolkit> Toolkit = MakeShared<FVekaViewEditorToolkit>();
		Toolkit->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, View);
	}
	return EAssetCommandResult::Handled;
}

bool UVekaViewThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return Cast<UVekaView>(Object) != nullptr;
}

void UVekaViewThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	OutWidth = FMath::TruncToInt(256.f * Zoom);
	OutHeight = FMath::TruncToInt(256.f * Zoom);
}

void UVekaViewThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Viewport, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UVekaView* View = Cast<UVekaView>(Object);
	if (!View || Width == 0 || Height == 0 || !FSlateApplication::IsInitialized())
	{
		return;
	}

	// The view with its preview model, laid out at its own size, then scaled to the thumbnail.
	TStrongObjectPtr<UVekaViewModel> Model;
#if WITH_EDITORONLY_DATA
	if (UClass* ModelClass = View->PreviewModelClass.Get())
	{
		Model.Reset(NewObject<UVekaViewModel>(GetTransientPackage(), ModelClass, NAME_None, RF_Transient));
	}
#endif
	const TSharedRef<FVekaViewStore> Store = MakeShared<FVekaViewStore>();
	Store->SetModel(Model.Get());
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	FVekaEnvironment Environment = VekaEnv::MakeDefault();
	Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Veka::Colora::ResolveColor(TEXT("Foreground"), Environment));
	Host->SetEnvironment(Environment);
	Host->SetRoot(View->BuildRoot(Store));
	Host->SlatePrepass(1.f);
	FVector2D Natural = Host->GetDesiredSize();
	Natural.X = FMath::Clamp(Natural.X, 16.0, 2048.0);
	Natural.Y = FMath::Clamp(Natural.Y, 16.0, 2048.0);

	const FLinearColor Backdrop = Veka::Colora::ResolveColor(TEXT("Background"), Environment);
	const TSharedRef<SWidget> Content = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(Backdrop)
		.Padding(FMargin(Width * 0.06f))
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SNew(SBox)
				.WidthOverride(Natural.X)
				.HeightOverride(Natural.Y)
				[
					Host
				]
			]
		];

	const FVector2D DrawSize(Width, Height);
	if (!RenderTarget || RenderTarget->SizeX != static_cast<int32>(Width) || RenderTarget->SizeY != static_cast<int32>(Height))
	{
		RenderTarget = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, true);
	}
	if (!Renderer.IsValid())
	{
		Renderer = MakeShared<FWidgetRenderer>(true, false);
	}
	// Two passes: the first one lays the views out, the second draws the settled layout.
	Renderer->DrawWidget(RenderTarget, Content, DrawSize, 0.f);
	Renderer->DrawWidget(RenderTarget, Content, DrawSize, 0.f);

	FCanvasTileItem Tile(FVector2D(X, Y), RenderTarget->GameThread_GetRenderTargetResource(), DrawSize, FLinearColor::White);
	Tile.BlendMode = SE_BLEND_Opaque;
	Canvas->DrawItem(Tile);
}

void UVekaViewThumbnailRenderer::BeginDestroy()
{
	Renderer.Reset();
	RenderTarget = nullptr;
	Super::BeginDestroy();
}

#undef LOCTEXT_NAMESPACE
