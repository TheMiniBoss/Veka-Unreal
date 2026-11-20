// Copyright TheMiniBoss. All Rights Reserved.

#include "Primitives/VekaShapeView.h"
#include "Core/VekaEnvironment.h"
#include "Vector/VekaVectorRenderer.h"

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

	const FVekaPaint FillPaint = bFillWithForeground ? FVekaPaint(VekaEnv::GetForegroundColor(Context.Environment)) : Fill;
	const bool bDrawFill = bFill && FillPaint.IsVisible();
	const bool bDrawStroke = bStroke && Stroke.IsVisible();

	if (Shape.CanUseRoundedBox() && (!bDrawStroke || (Stroke.IsSimple() && Stroke.Paint.Type == EVekaPaintType::Solid)))
	{
		// Fast path: Slate's rounded box (batched, antialiased, per-corner radii, outline).
		const FVekaRect ShapeRect = Shape.GetShapeRect(Bounds);
		const FVekaCornerRadii Radii = Shape.ResolveRadii(ShapeRect);
		if (bDrawFill)
		{
			FillPaint.Draw(Context, ShapeRect, Radii, State.FillBrush);
		}
		if (bDrawStroke)
		{
			Stroke.Paint.DrawOutline(Context, ShapeRect, Radii, Stroke.Width, State.StrokeBrush);
		}
		return Context.LayerId;
	}

	// Path route: ellipses, continuous corners, custom paths, dashed / trimmed strokes.
	const TArray<FVekaPolyline>& Polylines = GetPolylines(State, Shape, Bounds);
	if (bDrawFill)
	{
		Veka::VectorRender::Fill(Context, Polylines, FillPaint.GetAverageColor());
	}
	if (bDrawStroke)
	{
		Veka::VectorRender::Stroke(Context, Polylines, Stroke);
	}
	return Context.LayerId;
}
