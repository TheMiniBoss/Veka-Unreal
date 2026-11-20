// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Core/VekaModifier.h"
#include "Core/VekaPaint.h"
#include "VekaVisualModifiers.generated.h"

USTRUCT(BlueprintType, meta = (VekaName = "background", VekaCategory = "Appearance"))
struct VEKAUI_API FVekaBackgroundModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance")
	FVekaPaint Paint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance")
	FVekaCornerRadii CornerRadii;

	virtual int32 PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const override;

private:
	mutable FSlateBrush Brush;
};

/** Outline drawn inside the rect, over the content. */
USTRUCT(BlueprintType, meta = (VekaName = "border", VekaCategory = "Appearance"))
struct VEKAUI_API FVekaBorderModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance")
	FVekaPaint Paint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance", meta = (ClampMin = "0"))
	float Width = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance")
	FVekaCornerRadii CornerRadii;

	virtual int32 PaintFront(FVekaPaintContext& Context, const FVekaRect& Rect) const override;

private:
	mutable FSlateBrush Brush;
};

/**
 * Soft shadow. Phase 1 approximates the blur with stacked rounded boxes (no render target).
 * The real Gaussian shadow comes with the render module.
 */
USTRUCT(BlueprintType, meta = (VekaName = "shadow", VekaCategory = "Appearance"))
struct VEKAUI_API FVekaShadowModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance")
	FLinearColor Color = FLinearColor(0.f, 0.f, 0.f, 0.33f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance", meta = (ClampMin = "0"))
	float Radius = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance")
	FVector2D Offset = FVector2D(0.0, 2.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance")
	FVekaCornerRadii CornerRadii;

	virtual int32 PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const override;

	static constexpr int32 NumLayers = 6;

private:
	mutable FSlateBrush Brushes[NumLayers];
};

USTRUCT(BlueprintType, meta = (VekaName = "opacity", VekaCategory = "Appearance"))
struct VEKAUI_API FVekaOpacityModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Appearance", meta = (ClampMin = "0", ClampMax = "1"))
	float Opacity = 1.f;

	virtual void ModifyWidgetStyle(FWidgetStyle& Style) const override;
};

USTRUCT(BlueprintType, meta = (VekaName = "clip", VekaCategory = "Appearance"))
struct VEKAUI_API FVekaClipModifier : public FVekaModifier
{
	GENERATED_BODY()

	virtual bool ClipsContent() const override { return true; }
};

/** Moves the node without changing layout (render transform). */
USTRUCT(BlueprintType, meta = (VekaName = "offset", VekaCategory = "Transform"))
struct VEKAUI_API FVekaOffsetModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Transform")
	FVector2D Offset = FVector2D::ZeroVector;

	virtual bool HasRenderTransform() const override { return !Offset.IsNearlyZero(); }
	virtual void ModifyRenderTransform(FSlateRenderTransform& Transform, const FVector2D& Size) const override;
};

USTRUCT(BlueprintType, meta = (VekaName = "scale", VekaCategory = "Transform"))
struct VEKAUI_API FVekaScaleModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Transform")
	FVector2D Scale = FVector2D(1.0, 1.0);

	virtual bool HasRenderTransform() const override { return !Scale.Equals(FVector2D(1.0, 1.0)); }
	virtual void ModifyRenderTransform(FSlateRenderTransform& Transform, const FVector2D& Size) const override;
};

USTRUCT(BlueprintType, meta = (VekaName = "rotation", VekaCategory = "Transform"))
struct VEKAUI_API FVekaRotationModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Transform")
	float Degrees = 0.f;

	virtual bool HasRenderTransform() const override { return !FMath::IsNearlyZero(Degrees); }
	virtual void ModifyRenderTransform(FSlateRenderTransform& Transform, const FVector2D& Size) const override;
};
