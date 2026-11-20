// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Vector/VekaPath.h"

struct FVekaPaintContext;
struct FVekaStrokeStyle;

/**
 * Phase 1 vector renderer on top of Slate:
 *  - fills: ear-clipping triangulation -> FSlateDrawElement::MakeCustomVerts, plus a 1px antialiased edge line;
 *  - strokes: trim + dashes -> FSlateDrawElement::MakeLines (antialiased).
 * Phase 2 replaces it with a full tessellator (holes, caps, joins, analytic AA, gradient strokes) with caching.
 */
namespace Veka::VectorRender
{
	VEKAUI_API int32 Fill(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FLinearColor& Color);
	VEKAUI_API int32 Stroke(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaStrokeStyle& Style);
}
