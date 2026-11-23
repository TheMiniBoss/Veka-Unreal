// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPaint.h"
#include "Vector/VekaStroke.h"

struct FVekaPaintContext;
struct FVekaPolyline;

namespace Veka::Colora
{
	/**
	 * Evaluates a resolved paint (no tokens) at many points: gradient stops are converted to the paint's color space
	 * once, mesh gradients are inverted per point. Colors are linear (FLinearColor).
	 */
	class VEKAUI_API FPaintSampler
	{
	public:
		FPaintSampler(const FVekaPaint& InPaint, const FVekaRect& InBounds);

		FLinearColor Sample(const FVector2D& LocalPoint) const;

		/** Gradient parameter (0..1, unclamped) of a point, for linear / radial / elliptical / angular paints. */
		float GetGradientT(const FVector2D& LocalPoint) const;

		/** Distance between color samples (in pixels) that keeps the piecewise-linear error invisible. */
		double GetSampleSpacingPixels() const;

	private:
		FLinearColor SampleStops(float T) const;
		FLinearColor SampleMesh(const FVector2D& LocalPoint) const;
		FVector2D MeshPosition(int32 CellX, int32 CellY, double U, double V) const;
		FVector4f MeshValue(int32 CellX, int32 CellY, double U, double V) const;
		FVector4f MeshPoint(int32 X, int32 Y) const;
		bool FindMeshUV(const FVector2D& Point, int32& OutCellX, int32& OutCellY, double& OutU, double& OutV) const;

		FVekaPaint Paint;
		FVekaRect Bounds;
		EVekaColorSpace Space = EVekaColorSpace::OkLab;

		TArray<float> StopLocations;
		TArray<FVector4f> StopValues;

		int32 MeshWidth = 0;
		int32 MeshHeight = 0;
		bool bMeshSmooth = false;
		bool bMeshRegular = false;
		TArray<FVector2D> MeshPositions;
		TArray<FVector4f> MeshValues;
	};
}

/**
 * Colora renderer on top of Slate custom vertices. Fills are triangulated, then (for gradients) cut along a grid
 * of GetSampleSpacingPixels() so per-vertex colors follow the paint; edges get a 1px antialiasing fringe.
 * Blend mode Add draws premultiplied colors with zero alpha (additive with Slate's premultiplied blend state).
 */
/**
 * A fill or stroke tessellated in local space (coverage in alpha, 1px antialiasing fringe included). Tessellation
 * is the expensive part: primitives that draw the same geometry every frame build it once and only draw it.
 */
struct VEKAUI_API FVekaTessellation
{
	TArray<FVector2D> Positions;
	TArray<float> Coverage;
	TArray<int32> Indices;

	bool IsEmpty() const { return Indices.IsEmpty(); }
	void Reset() { Positions.Reset(); Coverage.Reset(); Indices.Reset(); }
};

namespace Veka::PaintRender
{
	/** Fills closed polylines (local space) with a resolved paint fitted to Bounds. */
	VEKAUI_API int32 FillPolylines(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, const FVekaRect& Bounds);

	/**
	 * Tessellated, antialiased strokes along polylines (caps, joins, miter limit; gradient paints colored per vertex).
	 * Joins only fill the outer side of each turn, so translucent strokes do not darken at their joints.
	 */
	VEKAUI_API int32 StrokePolylines(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, float Width, const FVekaRect& Bounds,
		EVekaLineCap Cap = EVekaLineCap::Butt, EVekaLineJoin Join = EVekaLineJoin::Miter, float MiterLimit = 4.f);

	/** FillPolylines without drawing: the result depends on the polylines, the paint (gradient grid) and the render scale. */
	VEKAUI_API void TessellateFill(const FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, const FVekaRect& Bounds,
		FVekaTessellation& Out);

	/** StrokePolylines without drawing. */
	VEKAUI_API void TessellateStroke(const FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, float Width, const FVekaRect& Bounds,
		EVekaLineCap Cap, EVekaLineJoin Join, float MiterLimit, FVekaTessellation& Out);

	/** Colors and draws a tessellation (cheap: no geometry work). */
	VEKAUI_API int32 DrawTessellation(FVekaPaintContext& Context, const FVekaTessellation& Mesh, const FVekaPaint& Paint, const FVekaRect& Bounds);

	/** Color matrix of the environment, then the widget style tint (opacity). */
	VEKAUI_API FLinearColor ApplyTint(const FVekaPaintContext& Context, const FLinearColor& Color);
}
