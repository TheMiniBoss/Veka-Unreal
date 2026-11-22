// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaEffectModifiers.h"
#include "VekaBackdrop.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRendererTypes.h"
#include "Slate/SPostBufferUpdate.h"
#include "Components/PostBufferBlurUpdater.h"
#include "SlateRHIRendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Input/HittestGrid.h"
#include "Types/PaintArgs.h"
#include "Widgets/SWindow.h"

int32 FVekaBackdropModifier::PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	if (Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}
	if (Method == EVekaBackdropMethod::EnginePostBuffer)
	{
		return PaintEnginePostBuffer(Context, Rect);
	}

	// B: the render graph element gets the rect in absolute (window) pixels.
	const FSlateRenderTransform& Transform = Context.Geometry.GetAccumulatedRenderTransform();
	const FVector2f A = Transform.TransformPoint(FVector2f(Rect.Position));
	const FVector2f B = Transform.TransformPoint(FVector2f(Rect.Position + Rect.Size));
	const float Scale = Transform.GetMatrix().GetScale().GetVector().X;
	FVekaBackdropParams Params;
	Params.Min = FVector2f(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
	Params.Max = FVector2f(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
	Params.BlurRadius = Radius * Scale;
	Params.CornerRadii = FVector4f(CornerRadii.TopLeft, CornerRadii.TopRight, CornerRadii.BottomRight, CornerRadii.BottomLeft) * Scale;
	Params.Tint = Tint;
	Params.Saturation = Saturation;
	Params.Opacity = Context.WidgetStyle.GetColorAndOpacityTint().A;
	FSlateDrawElement::MakeCustom(Context.DrawElements, Context.LayerId, Veka::Render::MakeBackdropElement(Params));
	return Context.LayerId + 1;
}

int32 FVekaBackdropModifier::PaintEnginePostBuffer(FVekaPaintContext& Context, const FVekaRect& Rect) const
{
	// A: SPostBufferUpdate copies what was drawn so far into post buffer 0 and runs its processor (Gaussian blur).
	if (!PostBufferUpdate.IsValid())
	{
		PostBufferUpdate = SNew(SPostBufferUpdate).bUsePaintGeometry(true);
		TArray<ESlatePostRT> Buffers = { ESlatePostRT::ESlatePostRT_0 };
		PostBufferUpdate->SetBuffersToUpdate(MakeArrayView(Buffers));
		UPostBufferBlurUpdater* Updater = NewObject<UPostBufferBlurUpdater>();
		Updater->GaussianBlurStrength = Radius;
		TMap<ESlatePostRT, TSharedPtr<FSlatePostProcessorUpdaterProxy>> Updaters;
		Updaters.Add(ESlatePostRT::ESlatePostRT_0, Updater->GetRenderThreadProxy());
		PostBufferUpdate->SetProcessorUpdaters(Updaters);
	}
	static FHittestGrid Grid;
	const FPaintArgs Args(nullptr, Grid, FVector2f::ZeroVector, FPlatformTime::Seconds(), 0.f);
	const FSlateRect Culling = Context.Geometry.GetLayoutBoundingRect();
	const FGeometry Geometry = Context.Geometry.MakeChild(FVector2f(Rect.Size), FSlateLayoutTransform(FVector2f(Rect.Position)));
	int32 Layer = PostBufferUpdate->Paint(Args, Geometry, Culling, Context.DrawElements, Context.LayerId, Context.WidgetStyle, true);

	// The post buffer covers the window: the rect's UVs are its window position over the window size.
	USlateRHIRendererSettings* Settings = USlateRHIRendererSettings::GetMutable();
	UTextureRenderTarget2D* Target = Settings ? Settings->LoadGetPostBufferRT(ESlatePostRT::ESlatePostRT_0) : nullptr;
	const SWindow* Window = Context.DrawElements.GetPaintWindow();
	if (!Target || !Window)
	{
		return Layer;
	}
	const FVector2f WindowPosition = Window->GetPositionInScreen();
	const FVector2f WindowSize = Window->GetSizeInScreen();
	const FSlateRect Absolute = Geometry.GetRenderBoundingRect();
	PostBufferBrush.SetResourceObject(Target);
	PostBufferBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PostBufferBrush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(CornerRadii.TopLeft, CornerRadii.TopRight, CornerRadii.BottomRight, CornerRadii.BottomLeft));
	PostBufferBrush.SetUVRegion(FBox2f(FVector2f((Absolute.Left - WindowPosition.X) / WindowSize.X, (Absolute.Top - WindowPosition.Y) / WindowSize.Y),
		FVector2f((Absolute.Right - WindowPosition.X) / WindowSize.X, (Absolute.Bottom - WindowPosition.Y) / WindowSize.Y)));
	FSlateDrawElement::MakeBox(Context.DrawElements, ++Layer, Geometry.ToPaintGeometry(), &PostBufferBrush, Context.DrawEffects, FLinearColor::White);
	TintBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TintBrush.OutlineSettings = PostBufferBrush.OutlineSettings;
	FSlateDrawElement::MakeBox(Context.DrawElements, ++Layer, Geometry.ToPaintGeometry(), &TintBrush, Context.DrawEffects, Tint);
	return Layer + 1;
}
