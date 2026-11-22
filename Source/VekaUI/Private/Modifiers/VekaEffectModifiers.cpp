// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaEffectModifiers.h"
#include "VekaBackdrop.h"
#include "Colora/VekaTheme.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	TArray<FVekaBackdropLayer*>& LayerStack()
	{
		static TArray<FVekaBackdropLayer*> Stack;
		return Stack;
	}

	/**
	 * Glass groups being painted: their children give them their shapes. A null entry is a group that draws nothing
	 * (no capture), whose children draw on their own.
	 */
	TArray<TSharedPtr<FVekaGlassParams, ESPMode::ThreadSafe>>& GroupStack()
	{
		static TArray<TSharedPtr<FVekaGlassParams, ESPMode::ThreadSafe>> Stack;
		return Stack;
	}

	/** A local rect in absolute (window) pixels, and the pixels per Slate unit. */
	void ToAbsolute(const FVekaPaintContext& Context, const FVekaRect& Rect, FVector2f& OutMin, FVector2f& OutMax, float& OutScale)
	{
		const FSlateRenderTransform& Transform = Context.Geometry.GetAccumulatedRenderTransform();
		const FVector2f A = Transform.TransformPoint(FVector2f(Rect.Position));
		const FVector2f B = Transform.TransformPoint(FVector2f(Rect.Position + Rect.Size));
		OutMin = FVector2f(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		OutMax = FVector2f(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		OutScale = Transform.GetMatrix().GetScale().GetVector().X;
	}

	TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe> MakeGlassParams(const FVekaGlassMaterial& Material, const FVekaPaintContext& Context, float Scale)
	{
		TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe> Params = MakeShared<FVekaGlassParams, ESPMode::ThreadSafe>();
		Params->BlurRadius = Material.Blur * Scale;
		Params->Refraction = Material.Refraction * Scale;
		Params->Bezel = Material.Bezel * Scale;
		Params->Aberration = Material.Aberration;
		Params->Highlight = Material.Highlight;
		const FLinearColor Tint = Veka::Colora::Resolve(Material.Tint, Context.Environment).GetAverageColor();
		Params->Tint = FLinearColor(Tint.R, Tint.G, Tint.B, FMath::Clamp(Material.TintAmount, 0.f, 1.f) * Tint.A);
		Params->Saturation = Material.Saturation;
		Params->Opacity = Context.WidgetStyle.GetColorAndOpacityTint().A;
		if (FSlateApplication::IsInitialized())
		{
			Params->LightPosition = FVector2f(FSlateApplication::Get().GetCursorPos());
		}
		return Params;
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

int32 FVekaLiquidDistortionModifier::PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	FVekaBackdropLayer* Layer = Veka::Backdrop::CurrentLayer();
	if (!Layer || Strength <= 0.f || Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0 || Veka::Render::GetBackdropQuality() == EVekaBackdropQuality::Off)
	{
		return Context.LayerId;
	}
	FVekaBackdropParams Params;
	float Scale_ = 1.f;
	ToAbsolute(Context, Rect, Params.Min, Params.Max, Scale_);
	Params.BlurRadius = 0.f;
	Params.CornerRadii = FVector4f(CornerRadii.TopLeft, CornerRadii.TopRight, CornerRadii.BottomRight, CornerRadii.BottomLeft) * Scale_;
	Params.Tint = FLinearColor::Transparent;
	Params.Saturation = 1.f;
	Params.DistortionStrength = Strength * Scale_;
	Params.DistortionScale = Scale * Scale_;
	Params.DistortionTime = static_cast<float>(FMath::Fmod(FPlatformTime::Seconds() * Speed, 3600.0));
	Params.Opacity = Context.WidgetStyle.GetColorAndOpacityTint().A;
	FSlateDrawElement::MakeCustom(Context.DrawElements, Context.LayerId, Layer->Add(Params));
	return Context.LayerId + 1;
}

int32 FVekaGlassModifier::PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	if (Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}
	FVekaGlassShape Shape;
	float Scale = 1.f;
	ToAbsolute(Context, Rect, Shape.Min, Shape.Max, Scale);
	Shape.CornerRadii = FVector4f(CornerRadii.TopLeft, CornerRadii.TopRight, CornerRadii.BottomRight, CornerRadii.BottomLeft) * Scale;

	// Inside a group that draws: the shape joins the group's glass.
	if (!GroupStack().IsEmpty() && GroupStack().Last().IsValid())
	{
		FVekaGlassParams& Group = *GroupStack().Last();
		if (Group.Shapes.Num() < FVekaGlassParams::MaxShapes)
		{
			Group.Shapes.Add(Shape);
		}
		return Context.LayerId;
	}

	FVekaBackdropLayer* Layer = Veka::Backdrop::CurrentLayer();
	if (!Layer || Veka::Render::GetBackdropQuality() == EVekaBackdropQuality::Off)
	{
		// No capture: a light tinted fill.
		FLinearColor Fill = Veka::Colora::Resolve(Material.Tint, Context.Environment).GetAverageColor();
		Fill.A = 0.35f * Context.WidgetStyle.GetColorAndOpacityTint().A;
		FallbackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		FallbackBrush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(CornerRadii.TopLeft, CornerRadii.TopRight, CornerRadii.BottomRight, CornerRadii.BottomLeft));
		FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Rect), &FallbackBrush, Context.DrawEffects, Fill);
		return Context.LayerId + 1;
	}
	const TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe> Params = MakeGlassParams(Material, Context, Scale);
	Params->Shapes.Add(Shape);
	FSlateDrawElement::MakeCustom(Context.DrawElements, Context.LayerId, Layer->AddGlass(Params));
	return Context.LayerId + 1;
}

int32 FVekaGlassGroupModifier::PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	FVekaBackdropLayer* Layer = Veka::Backdrop::CurrentLayer();
	if (!Layer || Veka::Render::GetBackdropQuality() == EVekaBackdropQuality::Off)
	{
		GroupStack().Push(nullptr);
		return Context.LayerId;
	}
	const float Scale = Context.Geometry.GetAccumulatedRenderTransform().GetMatrix().GetScale().GetVector().X;
	const TSharedRef<FVekaGlassParams, ESPMode::ThreadSafe> Params = MakeGlassParams(Material, Context, Scale);
	Params->Smoothness = Smoothness * Scale;
	// Drawn before the children; they add their shapes during this frame's paint.
	FSlateDrawElement::MakeCustom(Context.DrawElements, Context.LayerId, Layer->AddGlass(Params));
	GroupStack().Push(Params);
	return Context.LayerId + 1;
}

int32 FVekaGlassGroupModifier::PaintFront(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	if (!GroupStack().IsEmpty())
	{
		GroupStack().Pop();
	}
	return Context.LayerId;
}
