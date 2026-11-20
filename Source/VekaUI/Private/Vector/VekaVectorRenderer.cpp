// Copyright TheMiniBoss. All Rights Reserved.

#include "Vector/VekaVectorRenderer.h"
#include "Vector/VekaStroke.h"
#include "Core/VekaPrimitive.h"
#include "Colora/VekaPaintRenderer.h"

namespace Veka::VectorRender
{
	int32 Fill(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FLinearColor& Color)
	{
		FBox2D Box(ForceInit);
		for (const FVekaPolyline& Polyline : Polylines)
		{
			for (const FVector2D& Point : Polyline.Points)
			{
				Box += Point;
			}
		}
		const FVekaRect Bounds = Box.bIsValid ? FVekaRect(Box.Min, Box.GetSize()) : FVekaRect();
		return Veka::PaintRender::FillPolylines(Context, Polylines, FVekaPaint(Color), Bounds);
	}

	int32 Stroke(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaStrokeStyle& Style, const FVekaRect& Bounds)
	{
		if (!Style.IsVisible() || Polylines.IsEmpty())
		{
			return Context.LayerId;
		}

		TArray<FVekaPolyline> Trimmed;
		Veka::PathOps::Trim(Polylines, Style.TrimFrom, Style.TrimTo, Trimmed);

		TArray<FVekaPolyline> Dashed;
		if (Style.Dash.Num() > 0)
		{
			Veka::PathOps::Dash(Trimmed, Style.Dash, Style.DashPhase, Dashed);
		}
		else
		{
			Dashed = MoveTemp(Trimmed);
		}

		return Veka::PaintRender::StrokePolylines(Context, Dashed, Style.Paint, Style.Width, Bounds, Style.Cap, Style.Join, Style.MiterLimit);
	}
}
