// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Core/VekaModifier.h"
#include "Core/VekaPaint.h"
#include "VekaEffectModifiers.generated.h"

class FVekaBackdropLayer;

/**
 * Backdrop: what is drawn behind the node (the 3D scene, the UI under it), blurred, tinted and clipped to the rounded
 * rect. Every backdrop of a Veka host shares one capture and one blur pyramid per frame (r.Veka.Backdrop.Quality;
 * at 0 the node gets a tinted fill instead). `.backdropBlur(radius)` and `.frostedGlass()` (grain, stronger tint).
 */
USTRUCT(BlueprintType, meta = (VekaName = "backdropBlur", VekaCategory = "Effects"))
struct VEKAUI_API FVekaBackdropModifier : public FVekaModifier
{
	GENERATED_BODY()

	/** Blur radius (Slate units); any value, animatable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0"))
	float Radius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	FVekaCornerRadii CornerRadii;

	/** Color mixed over the blur (a Colora token by default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	FVekaPaint Tint = FVekaPaint::FromToken(TEXT("WindowBackground"));

	/** How much of the tint (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float TintAmount = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0"))
	float Saturation = 1.4f;

	/** Grain (frosted glass), 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float Noise = 0.f;

	virtual int32 PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const override;

	/** Frosted glass: a strong blur, more tint and a little grain. */
	static FVekaBackdropModifier FrostedGlass(const FVekaPaint& Tint = FVekaPaint::FromToken(TEXT("WindowBackground")), float Blur = 30.f, float Noise = 0.035f);

private:
	mutable FSlateBrush FallbackBrush;
};

/**
 * Liquid distortion: what is behind the node flows in animated waves (heat haze, water), inside its rounded rect.
 * Shares its host's backdrop capture.
 */
USTRUCT(BlueprintType, meta = (VekaName = "liquidDistortion", VekaCategory = "Effects"))
struct VEKAUI_API FVekaLiquidDistortionModifier : public FVekaModifier
{
	GENERATED_BODY()

	/** Displacement of the background (Slate units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0"))
	float Strength = 8.f;

	/** Size of the waves (Slate units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "1"))
	float Scale = 120.f;

	/** Speed of the flow (1 = normal, 0 = frozen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	float Speed = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	FVekaCornerRadii CornerRadii;

	virtual int32 PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const override;
};

/** Material of a liquid glass (shared by .glass and .glassGroup). */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaGlassMaterial
{
	GENERATED_BODY()

	/** Light blur of what is seen through the glass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0"))
	float Blur = 3.f;

	/** How far the background is displaced at the very edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0"))
	float Refraction = 18.f;

	/** Width of the curved edge (where refraction and the rim happen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "1"))
	float Bezel = 18.f;

	/** Color split of the refraction (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float Aberration = 0.35f;

	/** Specular rim, lit from the pointer (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float Highlight = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	FVekaPaint Tint = FVekaPaint(FLinearColor::White);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float TintAmount = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0"))
	float Saturation = 1.2f;
};

/**
 * Liquid glass: the background seen through the node's rounded rect, bent at the edge (refraction from the shape's
 * signed distance field), split in colors, with a rim lit from the pointer. Inside a .glassGroup, the node gives its
 * shape to the group instead (shapes then melt into each other). Shares its host's backdrop capture.
 */
USTRUCT(BlueprintType, meta = (VekaName = "glass", VekaCategory = "Effects"))
struct VEKAUI_API FVekaGlassModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	FVekaCornerRadii CornerRadii;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	FVekaGlassMaterial Material;

	virtual int32 PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const override;

private:
	mutable FSlateBrush FallbackBrush;
};

/**
 * One glass for every .glass in the subtree (up to 8 shapes): their outlines merge with a smooth minimum when they come
 * closer than Smoothness, like drops (Liquid Glass tab bars, morphing buttons). The group's material is used.
 */
USTRUCT(BlueprintType, meta = (VekaName = "glassGroup", VekaCategory = "Effects"))
struct VEKAUI_API FVekaGlassGroupModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0"))
	float Smoothness = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	FVekaGlassMaterial Material;

	virtual int32 PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const override;
	virtual int32 PaintFront(FVekaPaintContext& Context, const FVekaRect& Rect) const override;
};

namespace Veka::Backdrop
{
	/** The backdrop layer of the Veka host being painted (game thread), nullptr outside of a host. */
	VEKAUI_API FVekaBackdropLayer* CurrentLayer();

	/** Makes Layer the current one while a host paints its tree (scopes nest). */
	struct VEKAUI_API FLayerScope
	{
		explicit FLayerScope(FVekaBackdropLayer& Layer);
		~FLayerScope();
		FLayerScope(const FLayerScope&) = delete;
		FLayerScope& operator=(const FLayerScope&) = delete;
	};
}
