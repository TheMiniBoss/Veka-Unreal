// Copyright TheMiniBoss. All Rights Reserved.

#include "Primitives/VekaImage.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"

namespace
{
	struct FVekaImageState : public FVekaPrimitiveState
	{
		FSlateBrush Brush;
	};

	/** Rect of the image inside Bounds for a content mode. */
	FVekaRect FitRect(const FVekaRect& Bounds, const FVector2D& Natural, EVekaContentMode Mode)
	{
		if (Mode == EVekaContentMode::Stretch || Natural.X <= 0.0 || Natural.Y <= 0.0)
		{
			return Bounds;
		}
		const double ScaleX = Bounds.Size.X / Natural.X;
		const double ScaleY = Bounds.Size.Y / Natural.Y;
		const double Scale = Mode == EVekaContentMode::Fit ? FMath::Min(ScaleX, ScaleY) : FMath::Max(ScaleX, ScaleY);
		const FVector2D Size = Natural * Scale;
		return FVekaRect(Bounds.Position + (Bounds.Size - Size) * 0.5, Size);
	}
}

FVector2D FVekaImage::GetNaturalSize() const
{
	if (NaturalSize.X > 0.0 && NaturalSize.Y > 0.0)
	{
		return NaturalSize;
	}
	if (const UTexture* Texture = Cast<UTexture>(Source.Get()))
	{
		return FVector2D(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight());
	}
	return FVector2D(64.0, 64.0);
}

TSharedPtr<FVekaPrimitiveState> FVekaImage::CreateState() const
{
	return MakeShared<FVekaImageState>();
}

FVector2D FVekaImage::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	const FVector2D Natural = GetNaturalSize();
	if (!bResizable)
	{
		return Natural;
	}

	// Resizable: take the proposal. With one dimension free, keep the aspect ratio (Fit/Fill).
	const double Aspect = Natural.Y > 0.0 ? Natural.X / Natural.Y : 1.0;
	const bool bHasW = Proposal.Width.IsSet() && !Veka::IsInfinite(Proposal.Width.GetValue());
	const bool bHasH = Proposal.Height.IsSet() && !Veka::IsInfinite(Proposal.Height.GetValue());
	if (bHasW && bHasH)
	{
		if (ContentMode == EVekaContentMode::Fit)
		{
			return FitRect(FVekaRect(FVector2D::ZeroVector, FVector2D(Proposal.Width.GetValue(), Proposal.Height.GetValue())), Natural, EVekaContentMode::Fit).Size;
		}
		return FVector2D(Proposal.Width.GetValue(), Proposal.Height.GetValue());
	}
	if (bHasW)
	{
		return FVector2D(Proposal.Width.GetValue(), Proposal.Width.GetValue() / Aspect);
	}
	if (bHasH)
	{
		return FVector2D(Proposal.Height.GetValue() * Aspect, Proposal.Height.GetValue());
	}
	return Natural;
}

int32 FVekaImage::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (!Source || !Context.State)
	{
		return Context.LayerId;
	}

	FSlateBrush& Brush = static_cast<FVekaImageState&>(*Context.State).Brush;
	if (Brush.GetResourceObject() != Source)
	{
		Brush = FSlateBrush();
		Brush.SetResourceObject(Source);
		Brush.DrawAs = ESlateBrushDrawType::Image;
	}
	const FVector2D Natural = GetNaturalSize();
	Brush.SetImageSize(Natural);

	const FVekaRect ImageRect = bResizable ? FitRect(Bounds, Natural, ContentMode) : Bounds;
	const bool bClip = bResizable && ContentMode == EVekaContentMode::Fill;
	if (bClip)
	{
		Context.DrawElements.PushClip(FSlateClippingZone(Context.MakePaintGeometry(Bounds)));
	}
	FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(ImageRect), &Brush, Context.DrawEffects, Context.Tint(Tint));
	if (bClip)
	{
		Context.DrawElements.PopClip();
	}
	return Context.LayerId;
}
