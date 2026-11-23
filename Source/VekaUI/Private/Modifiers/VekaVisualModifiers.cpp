// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaVisualModifiers.h"
#include "Rendering/DrawElements.h"

int32 FVekaBackgroundModifier::PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	return Paint.Draw(Context, Rect, CornerRadii, Brush);
}

int32 FVekaBorderModifier::PaintFront(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	return Paint.DrawOutline(Context, Rect, CornerRadii, Width, Brush);
}

int32 FVekaShadowModifier::PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	if (Color.A <= 0.f || Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}

	// Stacked rounded boxes, outermost (faintest) first, approximating a Gaussian falloff.
	const FVekaRect Base = Rect.Translated(Offset);
	const float LayerAlpha = Color.A / NumLayers * 1.6f;
	for (int32 Layer = NumLayers; Layer >= 1; --Layer)
	{
		const float Spread = Radius * static_cast<float>(Layer) / NumLayers;
		const FVekaRect LayerRect = Base.Inflate(Spread * 0.5f);
		const FVekaCornerRadii Radii(CornerRadii.TopLeft + Spread * 0.5f, CornerRadii.TopRight + Spread * 0.5f,
			CornerRadii.BottomRight + Spread * 0.5f, CornerRadii.BottomLeft + Spread * 0.5f);
		const float Falloff = 1.f - static_cast<float>(Layer - 1) / NumLayers;
		FLinearColor LayerColor = Color;
		LayerColor.A = LayerAlpha * Falloff;
		FVekaPaint(LayerColor).Draw(Context, LayerRect, Radii, Brushes[Layer - 1]);
	}
	return Context.LayerId;
}

void FVekaOpacityModifier::ModifyWidgetStyle(FWidgetStyle& Style) const
{
	Style.BlendColorAndOpacityTint(FLinearColor(1.f, 1.f, 1.f, Opacity));
}

void FVekaOffsetModifier::ModifyRenderTransform(FSlateRenderTransform& Transform, const FVector2D& Size) const
{
	Transform = Concatenate(Transform, FSlateRenderTransform(FVector2f(Offset)));
}

void FVekaScaleModifier::ModifyRenderTransform(FSlateRenderTransform& Transform, const FVector2D& Size) const
{
	Transform = Concatenate(Transform, FSlateRenderTransform(FScale2f(FVector2f(Scale))));
}

void FVekaRotationModifier::ModifyRenderTransform(FSlateRenderTransform& Transform, const FVector2D& Size) const
{
	Transform = Concatenate(Transform, FSlateRenderTransform(FQuat2f(FMath::DegreesToRadians(Degrees))));
}
