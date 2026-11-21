// Copyright TheMiniBoss. All Rights Reserved.

#include "Symbols/VekaSymbolAssets.h"
#include "Symbols/VekaSvgImport.h"
#include "Symbols/VekaSymbolEditor.h"
#include "Paint/VekaPaintEditor.h"
#include "Symbols/VekaSymbol.h"
#include "Colora/VekaPaintAsset.h"
#include "Colora/VekaTheme.h"
#include "Core/VekaEnvironment.h"
#include "Slate/SVekaHost.h"
#include "VekaBuilders.h"
#include "AssetTypeCategories.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Misc/FeedbackContext.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "VekaSymbolAssets"

// --- Factories -----------------------------------------------------------------------------------------------------

UVekaSymbolImportFactory::UVekaSymbolImportFactory()
{
	SupportedClass = UVekaSymbol::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	bText = true;
	Formats.Add(TEXT("svg;SVG image (as a Veka Symbol)"));
}

UObject* UVekaSymbolImportFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type,
	const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	const FString Text(UE_PTRDIFF_TO_INT32(BufferEnd - Buffer), Buffer);
	FVekaSymbolDefinition Definition;
	TArray<FString> Warnings;
	FString Error;
	if (!Veka::Editor::Svg::ImportSymbol(Text, Definition, Warnings, &Error))
	{
		Warn->Log(ELogVerbosity::Error, FString::Printf(TEXT("%s: %s"), *GetCurrentFilename(), *Error));
		return nullptr;
	}
	for (const FString& Warning : Warnings)
	{
		Warn->Log(ELogVerbosity::Warning, FString::Printf(TEXT("%s: %s"), *GetCurrentFilename(), *Warning));
	}
	Definition.Name = InName;
	Definition.Category = TEXT("Imported");
	UVekaSymbol* Symbol = NewObject<UVekaSymbol>(InParent, InClass, InName, Flags | RF_Transactional);
	Symbol->Definition = MoveTemp(Definition);
	Veka::Symbols::RegisterSymbol(Symbol);
	Veka::Symbols::NotifyChanged(Symbol->Definition);
	Buffer = BufferEnd;
	return Symbol;
}

FText UVekaSymbolImportFactory::GetDisplayName() const
{
	return LOCTEXT("SymbolImportName", "Veka Symbol (from SVG)");
}

UVekaSymbolFactory::UVekaSymbolFactory()
{
	SupportedClass = UVekaSymbol::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UVekaSymbolFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UVekaSymbol* Symbol = NewObject<UVekaSymbol>(InParent, InClass, InName, Flags | RF_Transactional);
	Symbol->Definition.Name = InName;
	Symbol->Definition.Category = TEXT("Custom");
	// A circle to start from (the keyline grid's circle).
	FVekaSymbolShape& Shape = Symbol->Definition.Shapes.AddDefaulted_GetRef();
	Shape.Path.AddCircle(FVector2D(12.0, 12.0), 9.0);
	Shape.bFillable = true;
	Veka::Symbols::NotifyChanged(Symbol->Definition);
	return Symbol;
}

FText UVekaSymbolFactory::GetDisplayName() const
{
	return LOCTEXT("SymbolFactoryName", "Veka Symbol");
}

uint32 UVekaSymbolFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::UI;
}

UVekaPaintFactory::UVekaPaintFactory()
{
	SupportedClass = UVekaPaintAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UVekaPaintFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UVekaPaintAsset* Asset = NewObject<UVekaPaintAsset>(InParent, InClass, InName, Flags | RF_Transactional);
	const FLinearColor Colors[] = {
		FLinearColor(0.10f, 0.05f, 0.40f), FLinearColor(0.35f, 0.10f, 0.60f), FLinearColor(0.85f, 0.25f, 0.55f),
		FLinearColor(0.05f, 0.20f, 0.55f), FLinearColor(0.45f, 0.35f, 0.85f), FLinearColor(0.95f, 0.55f, 0.45f),
		FLinearColor(0.02f, 0.35f, 0.50f), FLinearColor(0.20f, 0.65f, 0.75f), FLinearColor(0.98f, 0.85f, 0.55f),
	};
	Asset->Paint = FVekaPaint::FromMesh(FVekaMeshGradient::Grid(3, 3, Colors));
	return Asset;
}

FText UVekaPaintFactory::GetDisplayName() const
{
	return LOCTEXT("PaintFactoryName", "Veka Paint");
}

uint32 UVekaPaintFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::UI;
}

// --- Asset definitions ---------------------------------------------------------------------------------------------

FText UAssetDefinition_VekaSymbol::GetAssetDisplayName() const
{
	return LOCTEXT("SymbolAsset", "Veka Symbol");
}

FLinearColor UAssetDefinition_VekaSymbol::GetAssetColor() const
{
	return FLinearColor(0.35f, 0.82f, 0.78f);
}

TSoftClassPtr<UObject> UAssetDefinition_VekaSymbol::GetAssetClass() const
{
	return UVekaSymbol::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_VekaSymbol::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::UI };
	return Categories;
}

EAssetCommandResult UAssetDefinition_VekaSymbol::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UVekaSymbol* Symbol : OpenArgs.LoadObjects<UVekaSymbol>())
	{
		MakeShared<FVekaSymbolEditorToolkit>()->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Symbol);
	}
	return EAssetCommandResult::Handled;
}

FText UAssetDefinition_VekaPaint::GetAssetDisplayName() const
{
	return LOCTEXT("PaintAsset", "Veka Paint");
}

FLinearColor UAssetDefinition_VekaPaint::GetAssetColor() const
{
	return FLinearColor(0.85f, 0.35f, 0.65f);
}

TSoftClassPtr<UObject> UAssetDefinition_VekaPaint::GetAssetClass() const
{
	return UVekaPaintAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_VekaPaint::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::UI };
	return Categories;
}

EAssetCommandResult UAssetDefinition_VekaPaint::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UVekaPaintAsset* Paint : OpenArgs.LoadObjects<UVekaPaintAsset>())
	{
		MakeShared<FVekaPaintEditorToolkit>()->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Paint);
	}
	return EAssetCommandResult::Handled;
}

// --- Thumbnails ----------------------------------------------------------------------------------------------------

void UVekaElementThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	OutWidth = FMath::TruncToInt(256.f * Zoom);
	OutHeight = FMath::TruncToInt(256.f * Zoom);
}

void UVekaElementThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Viewport, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	if (!Object || Width == 0 || Height == 0 || !FSlateApplication::IsInitialized())
	{
		return;
	}
	const TSharedRef<SVekaHost> Host = SNew(SVekaHost);
	FVekaEnvironment Environment = VekaEnv::MakeDefault();
	Environment.Set<FLinearColor>(VekaEnv::ForegroundColor(), Veka::Colora::ResolveColor(TEXT("Foreground"), Environment));
	Host->SetEnvironment(Environment);
	Host->SetRoot(MakeElement(Object));
	const FVector2D DrawSize(Width, Height);
	TSharedPtr<SWidget> Content;
	if (FillsThumbnail())
	{
		Content = Host;
	}
	else
	{
		Host->SlatePrepass(1.f);
		FVector2D Natural = Host->GetDesiredSize();
		Natural.X = FMath::Clamp(Natural.X, 8.0, 1024.0);
		Natural.Y = FMath::Clamp(Natural.Y, 8.0, 1024.0);
		Content = SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f))
			.Padding(FMargin(Width * 0.16f))
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
				[
					SNew(SBox).WidthOverride(Natural.X).HeightOverride(Natural.Y)[Host]
				]
			];
	}
	if (!RenderTarget || RenderTarget->SizeX != static_cast<int32>(Width) || RenderTarget->SizeY != static_cast<int32>(Height))
	{
		RenderTarget = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, true);
	}
	if (!Renderer.IsValid())
	{
		Renderer = MakeShared<FWidgetRenderer>(true, false);
	}
	Renderer->DrawWidget(RenderTarget, Content.ToSharedRef(), DrawSize, 0.f);
	Renderer->DrawWidget(RenderTarget, Content.ToSharedRef(), DrawSize, 0.f);
	FCanvasTileItem Tile(FVector2D(X, Y), RenderTarget->GameThread_GetRenderTargetResource(), DrawSize, FLinearColor::White);
	Tile.BlendMode = SE_BLEND_Opaque;
	Canvas->DrawItem(Tile);
}

void UVekaElementThumbnailRenderer::BeginDestroy()
{
	Renderer.Reset();
	RenderTarget = nullptr;
	Super::BeginDestroy();
}

bool UVekaSymbolThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return Cast<UVekaSymbol>(Object) != nullptr;
}

FVekaElement UVekaSymbolThumbnailRenderer::MakeElement(UObject* Object) const
{
	const UVekaSymbol* Symbol = Cast<UVekaSymbol>(Object);
	return Symbol ? Veka::Symbol(Symbol->GetSymbolName()).FontSize(48.f) : FVekaElement();
}

bool UVekaPaintThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return Cast<UVekaPaintAsset>(Object) != nullptr;
}

FVekaElement UVekaPaintThumbnailRenderer::MakeElement(UObject* Object) const
{
	const UVekaPaintAsset* Paint = Cast<UVekaPaintAsset>(Object);
	return Paint ? Veka::Rectangle().Fill(Paint->Paint) : FVekaElement();
}

#undef LOCTEXT_NAMESPACE
