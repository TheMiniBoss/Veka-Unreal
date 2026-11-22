// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Core/VekaModifier.h"
#include "Core/VekaPaint.h"
#include "VekaEffectModifiers.generated.h"

class SPostBufferUpdate;

/** How a backdrop reads what is behind it (Phase 4 spike: both are kept until the decision). */
UENUM(BlueprintType)
enum class EVekaBackdropMethod : uint8
{
	/** B: our Slate custom element, render graph copy + dual Kawase blur + composite (VekaUIRender). */
	RenderGraph,
	/** A: the engine's Slate post buffer 0 (SPostBufferUpdate + its Gaussian blur processor), sampled as a texture. */
	EnginePostBuffer
};

/** Frosted backdrop: what is drawn behind the node, blurred, tinted and clipped to the rounded rect. */
USTRUCT(BlueprintType, meta = (VekaName = "backdropBlur", VekaCategory = "Effects"))
struct VEKAUI_API FVekaBackdropModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0"))
	float Radius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	FVekaCornerRadii CornerRadii;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	FLinearColor Tint = FLinearColor(1.f, 1.f, 1.f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects", meta = (ClampMin = "0"))
	float Saturation = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Effects")
	EVekaBackdropMethod Method = EVekaBackdropMethod::RenderGraph;

	virtual int32 PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const override;

private:
	int32 PaintEnginePostBuffer(FVekaPaintContext& Context, const FVekaRect& Rect) const;

	mutable TSharedPtr<SPostBufferUpdate> PostBufferUpdate;
	mutable FSlateBrush PostBufferBrush;
	mutable FSlateBrush TintBrush;
};
