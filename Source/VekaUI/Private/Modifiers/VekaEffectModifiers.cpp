// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaEffectModifiers.h"
#include "VekaBackdrop.h"
#include "Colora/VekaTheme.h"
#include "Rendering/DrawElements.h"

namespace
{
	TArray<FVekaBackdropLayer*>& LayerStack()
	{
		static TArray<FVekaBackdropLayer*> Stack;
		return Stack;
	}
}

namespace Veka::Backdrop
{
	FVekaBackdropLayer* CurrentLayer()
	{
		check(IsInGameThread());
		return LayerStack().IsEmpty() ? nullptr : LayerStack().Last();
	}

	FLayerScope::FLayerScope(FVekaBackdropLayer& Layer)
	{
		check(IsInGameThread());
		LayerStack().Push(&Layer);
	}

	FLayerScope::~FLayerScope()
	{
		LayerStack().Pop();
	}
}

FVekaBackdropModifier FVekaBackdropModifier::FrostedGlass(const FVekaPaint& InTint, float Blur, float InNoise)
{
	FVekaBackdropModifier Modifier;
	Modifier.Radius = Blur;
	Modifier.Tint = InTint;
	Modifier.TintAmount = 0.45f;
	Modifier.Saturation = 1.6f;
	Modifier.Noise = InNoise;
	return Modifier;
}

int32 FVekaBackdropModifier::PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	if (Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}
	FLinearColor TintColor = Veka::Colora::Resolve(Tint, Context.Environment).GetAverageColor();
	const float Opacity = Context.WidgetStyle.GetColorAndOpacityTint().A;

	if (Veka::Render::GetBackdropQuality() == EVekaBackdropQuality::Off)
	{
		// No capture: a tinted fill, opaque enough to keep the content readable.
		TintColor.A = FMath::Max(0.85f, TintAmount) * Opacity;
		FallbackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		FallbackBrush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(CornerRadii.TopLeft, CornerRadii.TopRight, CornerRadii.BottomRight, CornerRadii.BottomLeft));
		FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Rect), &FallbackBrush, Context.DrawEffects, TintColor);
		return Context.LayerId + 1;
	}

	// The rect in absolute (window) pixels.
	const FSlateRenderTransform& Transform = Context.Geometry.GetAccumulatedRenderTransform();
	const FVector2f A = Transform.TransformPoint(FVector2f(Rect.Position));
	const FVector2f B = Transform.TransformPoint(FVector2f(Rect.Position + Rect.Size));
	const float Scale = Transform.GetMatrix().GetScale().GetVector().X;
	FVekaBackdropParams Params;
	Params.Min = FVector2f(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
	Params.Max = FVector2f(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
	Params.BlurRadius = Radius * Scale;
	Params.CornerRadii = FVector4f(CornerRadii.TopLeft, CornerRadii.TopRight, CornerRadii.BottomRight, CornerRadii.BottomLeft) * Scale;
	Params.Tint = FLinearColor(TintColor.R, TintColor.G, TintColor.B, FMath::Clamp(TintAmount, 0.f, 1.f) * TintColor.A);
	Params.Saturation = Saturation;
	Params.Noise = Noise;
	Params.Opacity = Opacity;

	// Inside a host: one capture for all of its backdrops; elsewhere, a capture of its own.
	FVekaBackdropLayer* Layer = Veka::Backdrop::CurrentLayer();
	FSlateDrawElement::MakeCustom(Context.DrawElements, Context.LayerId, Layer ? Layer->Add(Params) : Veka::Render::MakeBackdropElement(Params));
	return Context.LayerId + 1;
}
