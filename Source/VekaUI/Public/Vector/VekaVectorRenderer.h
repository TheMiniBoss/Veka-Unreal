// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Vector/VekaPath.h"

struct FVekaPaintContext;
struct FVekaStrokeStyle;

/**
 * Vector renderer on top of Slate:
 *  - fills: Colora tessellator (Veka::PaintRender::FillPolylines): triangulation, per-vertex colors, 1px AA fringe;
 *  - strokes: trim + dashes -> antialiased Slate lines, per-point colors for gradient paints.
 * Caps / joins / alignment and variable width come with the full stroke tessellator.
 */
namespace Veka::VectorRender
{
	/** Solid fill (resolved color). */
	VEKAUI_API int32 Fill(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FLinearColor& Color);

	/** Stroke with a resolved paint; Bounds is the rect gradient paints are fitted to. */
	VEKAUI_API int32 Stroke(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaStrokeStyle& Style, const FVekaRect& Bounds);
}
