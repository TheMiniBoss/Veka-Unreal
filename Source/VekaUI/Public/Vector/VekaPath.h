// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaTypes.h"
#include "VekaPath.generated.h"

UENUM(BlueprintType)
enum class EVekaPathVerb : uint8
{
	MoveTo,
	LineTo,
	/** P0 = control, P1 = end. */
	QuadTo,
	/** P0 = control 1, P1 = control 2, P2 = end. */
	CubicTo,
	Close
};

UENUM(BlueprintType)
enum class EVekaFillRule : uint8
{
	NonZero,
	EvenOdd
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaPathCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Path")
	EVekaPathVerb Verb = EVekaPathVerb::MoveTo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Path")
	FVector2D P0 = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Path")
	FVector2D P1 = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Path")
	FVector2D P2 = FVector2D::ZeroVector;

	bool operator==(const FVekaPathCommand& Other) const
	{
		return Verb == Other.Verb && P0 == Other.P0 && P1 == Other.P1 && P2 == Other.P2;
	}
};

/** A flattened sub-path. */
struct VEKAUI_API FVekaPolyline
{
	TArray<FVector2D> Points;
	bool bClosed = false;
};

/**
 * Vector path: the single representation used for shapes, strokes, symbols (Phase 2) and vector text.
 * Arcs are stored as cubic Béziers.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaPath
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Path")
	TArray<FVekaPathCommand> Commands;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Path")
	EVekaFillRule FillRule = EVekaFillRule::NonZero;

	// --- Building ------------------------------------------------------------------------------

	FVekaPath& MoveTo(const FVector2D& Point);
	FVekaPath& LineTo(const FVector2D& Point);
	FVekaPath& QuadTo(const FVector2D& Control, const FVector2D& End);
	FVekaPath& CubicTo(const FVector2D& Control1, const FVector2D& Control2, const FVector2D& End);
	/** SVG elliptical arc from the current point to End. */
	FVekaPath& ArcTo(double RadiusX, double RadiusY, double XAxisRotationDegrees, bool bLargeArc, bool bSweep, const FVector2D& End);
	FVekaPath& Close();

	FVekaPath& AddRect(const FVekaRect& Rect);
	/** Rounded rectangle; bContinuous = smooth "squircle-like" corners (curvature ramps in, like iOS). */
	FVekaPath& AddRoundedRect(const FVekaRect& Rect, const FVekaCornerRadii& Radii, bool bContinuous = false);
	FVekaPath& AddEllipse(const FVekaRect& Rect);
	FVekaPath& AddCircle(const FVector2D& Center, double Radius);
	FVekaPath& AddPolygon(TConstArrayView<FVector2D> Points, bool bClosed = true);

	// --- Queries -------------------------------------------------------------------------------

	bool IsEmpty() const { return Commands.IsEmpty(); }
	/** Bounds of the flattened path. */
	FBox2D GetBounds() const;
	FVekaPath Transformed(const FVector2D& Scale, const FVector2D& Translation) const;
	/** Maps a path drawn in the unit square (0..1) into Rect. */
	FVekaPath FittedToRect(const FVekaRect& Rect) const { return Transformed(Rect.Size, Rect.Position); }
	void Flatten(TArray<FVekaPolyline>& OutPolylines, double Tolerance = 0.25) const;
	double GetLength(double Tolerance = 0.25) const;
	bool ContainsPoint(const FVector2D& Point, double Tolerance = 0.25) const;

	/** Parses SVG path data (the `d` attribute): M L H V C S Q T A Z, absolute and relative. */
	static bool FromSvg(const FString& PathData, FVekaPath& OutPath, FString* OutError = nullptr);

	bool operator==(const FVekaPath& Other) const { return FillRule == Other.FillRule && Commands == Other.Commands; }

private:
	FVector2D GetCurrentPoint() const;
	FVector2D GetSubpathStart() const;
};

namespace Veka::PathOps
{
	VEKAUI_API double PolylineLength(const FVekaPolyline& Polyline);
	/** Keeps the part between From and To (0..1 of the total length, over all polylines). */
	VEKAUI_API void Trim(const TArray<FVekaPolyline>& In, double From, double To, TArray<FVekaPolyline>& Out);
	/** Splits polylines with an on/off dash pattern (lengths in local units). */
	VEKAUI_API void Dash(const TArray<FVekaPolyline>& In, TConstArrayView<float> Pattern, double Phase, TArray<FVekaPolyline>& Out);
	/** Ear-clipping triangulation of a simple polygon. Returns false if it could not triangulate everything. */
	VEKAUI_API bool Triangulate(TConstArrayView<FVector2D> Polygon, TArray<int32>& OutIndices);
	VEKAUI_API double SignedArea(TConstArrayView<FVector2D> Polygon);
}
