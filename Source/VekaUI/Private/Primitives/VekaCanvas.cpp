// Copyright TheMiniBoss. All Rights Reserved.

#include "Primitives/VekaCanvas.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"

namespace
{
	struct FCanvasState : public FVekaPrimitiveState
	{
		bool bAnimated = false;
		virtual bool NeedsAnimationFrame(double Now) const override { return bAnimated; }
	};

	/** Round dots: one brush per quarter pixel of radius (Slate does not clamp a corner radius above half the size). */
	const TArray<FSlateRoundedBoxBrush>& DotBrushes()
	{
		static const TArray<FSlateRoundedBoxBrush> Brushes = []
		{
			TArray<FSlateRoundedBoxBrush> Out;
			for (int32 Step = 1; Step <= 256; ++Step)
			{
				Out.Emplace(FLinearColor::White, Step * 0.25f);
			}
			return Out;
		}();
		return Brushes;
	}
}

FVekaCanvasDrawing::FVekaCanvasDrawing(FVekaPaintContext& InContext, const FVekaRect& InBounds, bool bInAdditive)
	: Context(InContext), Bounds(InBounds), bAdditive(bInAdditive)
{
	// Additive: premultiplied color with (almost) no alpha, the light adds to what is under it.
	Effects = bAdditive ? (Context.DrawEffects | ESlateDrawEffect::PreMultipliedAlpha) : Context.DrawEffects;
}

const FVekaEnvironment& FVekaCanvasDrawing::GetEnvironment() const
{
	return Context.Environment;
}

FLinearColor FVekaCanvasDrawing::Final(const FLinearColor& Color) const
{
	const FLinearColor Tinted = Context.Tint(Color);
	if (!bAdditive)
	{
		return Tinted;
	}
	// Alpha 1/255 and not 0: Slate drops boxes with a fully transparent tint; 1/255 darkens what is under by less than 0.4%.
	const float A = FMath::Clamp(Tinted.A, 0.f, 1.f);
	return FLinearColor(Tinted.R * A, Tinted.G * A, Tinted.B * A, 1.f / 255.f);
}

void FVekaCanvasDrawing::Dot(const FVector2D& Center, double Radius, const FLinearColor& Color)
{
	if (Color.A <= 0.002f || Radius <= 0.05)
	{
		return;
	}
	const TArray<FSlateRoundedBoxBrush>& Brushes = DotBrushes();
	const int32 Step = FMath::Clamp(FMath::RoundToInt(Radius * 4.0), 1, Brushes.Num());
	const double Snapped = Step * 0.25;
	const FVekaRect Rect(Bounds.Position + Center - FVector2D(Snapped), FVector2D(Snapped * 2.0));
	FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Rect), &Brushes[Step - 1], Effects, Final(Color));
	++Marks;
}

void FVekaCanvasDrawing::Line(const FVector2D& From, const FVector2D& To, double Width, const FLinearColor& FromColor, const FLinearColor& ToColor)
{
	if ((FromColor.A <= 0.002f && ToColor.A <= 0.002f) || Width <= 0.0 || From.Equals(To, 0.01))
	{
		return;
	}
	TArray<FVector2f> Points = { FVector2f(From), FVector2f(To) };
	TArray<FLinearColor> Colors = { Final(FromColor), Final(ToColor) };
	FSlateDrawElement::MakeLines(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Bounds), MoveTemp(Points), MoveTemp(Colors), Effects,
		FLinearColor::White, true, static_cast<float>(Width));
	++Marks;
}

void FVekaCanvasDrawing::Glow(const FVector2D& Center, double Radius, const FLinearColor& Color)
{
	if (Color.A <= 0.002f || Radius <= 0.1)
	{
		return;
	}
	// A radial falloff from stacked rings: ring K adds ((K+1)/N)^2 - (K/N)^2, so the light at distance D adds up to
	// (1 - D / Radius)^2 of the color, a soft flash with no visible steps.
	constexpr int32 Rings = 24;
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		const double R = Radius * (1.0 - static_cast<double>(Ring) / Rings);
		const float Weight = (FMath::Square(Ring + 1.f) - FMath::Square(static_cast<float>(Ring))) / FMath::Square(static_cast<float>(Rings));
		Dot(Center, R, FLinearColor(Color.R, Color.G, Color.B, Color.A * Weight));
	}
}

FVector2D FVekaCanvas::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	return Proposal.ReplaceUnspecified(FVector2D(100.0, 100.0));
}

TSharedPtr<FVekaPrimitiveState> FVekaCanvas::CreateState() const
{
	TSharedRef<FCanvasState> State = MakeShared<FCanvasState>();
	State->bAnimated = Painter.IsValid() && Painter->IsAnimated();
	return State;
}

int32 FVekaCanvas::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (FCanvasState* State = static_cast<FCanvasState*>(Context.State))
	{
		State->bAnimated = Painter.IsValid() && Painter->IsAnimated();
	}
	if (!Painter.IsValid() || Bounds.Size.X <= 0.0 || Bounds.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}
	FVekaCanvasDrawing Drawing(Context, Bounds, bAdditive);
	Painter->Draw(Drawing);
	return Context.LayerId;
}
