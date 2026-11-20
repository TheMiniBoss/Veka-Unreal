// Copyright TheMiniBoss. All Rights Reserved.

#include "Primitives/VekaShapeView.h"
#include "Core/VekaEnvironment.h"
#include "Vector/VekaVectorRenderer.h"
#include "Colora/VekaTheme.h"
#include "Colora/VekaPaintRenderer.h"

namespace
{
	struct FVekaShapeState : public FVekaPrimitiveState
	{
		FSlateBrush FillBrush;
		FSlateBrush StrokeBrush;

		// Flattened outline cache (path route).
		FVekaRect CachedBounds;
		FVekaShape CachedShape;
		bool bHasCache = false;
		TArray<FVekaPolyline> Polylines;
	};

	const TArray<FVekaPolyline>& GetPolylines(FVekaShapeState& State, const FVekaShape& Shape, const FVekaRect& Bounds)
	{
		if (!State.bHasCache || !State.CachedBounds.Equals(Bounds) || !FVekaShape::StaticStruct()->CompareScriptStruct(&State.CachedShape, &Shape, PPF_None))
		{
			State.Polylines.Reset();
			Shape.MakePath(Bounds).Flatten(State.Polylines, 0.2);
			State.CachedBounds = Bounds;
			State.CachedShape = Shape;
			State.bHasCache = true;
		}
		return State.Polylines;
	}
}

TSharedPtr<FVekaPrimitiveState> FVekaShapeView::CreateState() const
{
	return MakeShared<FVekaShapeState>();
}

FVector2D FVekaShapeView::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	// Shapes take what they are offered; 10x10 ideal size like SwiftUI. "Infinite" probes report infinite.
	return Proposal.ReplaceUnspecified(FVector2D(10.0, 10.0));
}

bool FVekaShapeView::HitTest(const FVector2D& LocalPoint, const FVector2D& Size) const
{
	return Shape.Contains(LocalPoint, FVekaRect(FVector2D::ZeroVector, Size));
}

int32 FVekaShapeView::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (!Context.State || Bounds.Size.X <= 0.0 || Bounds.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}
	FVekaShapeState& State = static_cast<FVekaShapeState&>(*Context.State);

	const FVekaPaint FillPaint = Veka::Colora::Resolve(bFillWithForeground ? FVekaPaint(VekaEnv::GetForegroundColor(Context.Environment)) : Fill, Context.Environment);
	FVekaStrokeStyle StrokeStyle = Stroke;
	StrokeStyle.Paint = Veka::Colora::Resolve(Stroke.Paint, Context.Environment);
	const bool bDrawFill = bFill && FillPaint.IsVisible();
	const bool bDrawStroke = bStroke && StrokeStyle.IsVisible();

	if (Shape.CanUseRoundedBox() && (!bDrawStroke || StrokeStyle.IsSimple()))
	{
		// Rect-like shapes: FVekaPaint::Draw (Slate rounded box for solid colors, tessellator for gradients).
		const FVekaRect ShapeRect = Shape.GetShapeRect(Bounds);
		const FVekaCornerRadii Radii = Shape.ResolveRadii(ShapeRect);
		if (bDrawFill)
		{
			FillPaint.Draw(Context, ShapeRect, Radii, State.FillBrush);
		}
		if (bDrawStroke)
		{
			StrokeStyle.Paint.DrawOutline(Context, ShapeRect, Radii, StrokeStyle.Width, State.StrokeBrush);
		}
		return Context.LayerId;
	}

	// Path route: ellipses, continuous corners, custom paths, dashed / trimmed strokes.
	const TArray<FVekaPolyline>& Polylines = GetPolylines(State, Shape, Bounds);
	if (bDrawFill)
	{
		if (FillPaint.Type == EVekaPaintType::Image || FillPaint.Type == EVekaPaintType::Material)
		{
			// Textures on arbitrary paths need a mask (render module): drawn in the bounds for now.
			FillPaint.Draw(Context, Bounds, FVekaCornerRadii(), State.FillBrush);
		}
		else
		{
			Veka::PaintRender::FillPolylines(Context, Polylines, FillPaint, Bounds);
		}
	}
	if (bDrawStroke)
	{
		Veka::VectorRender::Stroke(Context, Polylines, StrokeStyle, Bounds);
	}
	return Context.LayerId;
}
