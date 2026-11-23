// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "VekaViewAssetDefinition.generated.h"

class UTextureRenderTarget2D;
class FWidgetRenderer;

/** Veka View in the Content Browser: name, color, User Interface category; double-click opens the Veka View editor. */
UCLASS()
class UAssetDefinition_VekaView : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

/** Content Browser thumbnail: the view itself, drawn with its preview model in the dark scheme. */
UCLASS()
class UVekaViewThumbnailRenderer : public UThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual bool CanVisualizeAsset(UObject* Object) override;
	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Viewport, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	virtual void BeginDestroy() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	TSharedPtr<FWidgetRenderer> Renderer;
};
