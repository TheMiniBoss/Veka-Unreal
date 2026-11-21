// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "AssetDefinitionDefault.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "Core/VekaElement.h"
#include "VekaSymbolAssets.generated.h"

class UTextureRenderTarget2D;
class FWidgetRenderer;

/** Import an .svg as a Veka Symbol (layers, roles, variable layers: see Veka::Editor::Svg::ImportSymbol). */
UCLASS()
class UVekaSymbolImportFactory : public UFactory
{
	GENERATED_BODY()

public:
	UVekaSymbolImportFactory();
	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer,
		const TCHAR* BufferEnd, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
};

/** Content Browser > Add > User Interface > Veka Symbol. */
UCLASS()
class UVekaSymbolFactory : public UFactory
{
	GENERATED_BODY()

public:
	UVekaSymbolFactory();
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
};

/** Content Browser > Add > User Interface > Veka Paint (starts as a 3 x 3 mesh gradient). */
UCLASS()
class UVekaPaintFactory : public UFactory
{
	GENERATED_BODY()

public:
	UVekaPaintFactory();
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
};

/** Veka Symbol: double-click opens the Symbol Editor. */
UCLASS()
class UAssetDefinition_VekaSymbol : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

/** Veka Paint: double-click opens the paint editor (mesh gradient points and colors). */
UCLASS()
class UAssetDefinition_VekaPaint : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

/** Thumbnail drawn by a Veka element (rendered with FWidgetRenderer, scaled to fit). */
UCLASS(Abstract)
class UVekaElementThumbnailRenderer : public UThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Viewport, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	virtual void BeginDestroy() override;

protected:
	virtual FVekaElement MakeElement(UObject* Object) const { return FVekaElement(); }
	/** Laid out at the thumbnail size (paints) instead of the element's own size (symbols). */
	virtual bool FillsThumbnail() const { return false; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	TSharedPtr<FWidgetRenderer> Renderer;
};

UCLASS()
class UVekaSymbolThumbnailRenderer : public UVekaElementThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual bool CanVisualizeAsset(UObject* Object) override;

protected:
	virtual FVekaElement MakeElement(UObject* Object) const override;
};

UCLASS()
class UVekaPaintThumbnailRenderer : public UVekaElementThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual bool CanVisualizeAsset(UObject* Object) override;

protected:
	virtual FVekaElement MakeElement(UObject* Object) const override;
	virtual bool FillsThumbnail() const override { return true; }
};
