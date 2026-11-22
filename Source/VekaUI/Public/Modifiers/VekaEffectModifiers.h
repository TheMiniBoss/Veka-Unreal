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
