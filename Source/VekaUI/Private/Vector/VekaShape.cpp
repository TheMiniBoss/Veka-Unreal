// Copyright TheMiniBoss. All Rights Reserved.

#include "Vector/VekaShape.h"

namespace
{
	/** Signed distance to a box with per-corner radii, centered at the origin (Inigo Quilez' sdRoundBox). */
	double RoundedBoxSDF(const FVector2D& P, const FVector2D& HalfSize, const FVekaCornerRadii& Radii)
	{
		// Pick the radius of the quadrant (y-down: top = negative y).
		double R;
		if (P.X >= 0.0)
		{
			R = P.Y < 0.0 ? Radii.TopRight : Radii.BottomRight;
		}
		else
		{
			R = P.Y < 0.0 ? Radii.TopLeft : Radii.BottomLeft;
		}
		const FVector2D Q = P.GetAbs() - HalfSize + FVector2D(R, R);
		const FVector2D QMax(FMath::Max(Q.X, 0.0), FMath::Max(Q.Y, 0.0));
		return FMath::Min(FMath::Max(Q.X, Q.Y), 0.0) + QMax.Size() - R;
	}

	/** Approximate signed distance to an axis-aligned ellipse (good near the boundary). */
	double EllipseSDF(const FVector2D& P, const FVector2D& Radii)
	{
		if (Radii.X <= 0.0 || Radii.Y <= 0.0)
		{
			return P.Size();
		}
		const double K0 = FVector2D(P.X / Radii.X, P.Y / Radii.Y).Size();
		const double K1 = FVector2D(P.X / (Radii.X * Radii.X), P.Y / (Radii.Y * Radii.Y)).Size();
		if (K1 <= 0.0)
		{
			return -FMath::Min(Radii.X, Radii.Y);
		}
		return K0 * (K0 - 1.0) / K1;
	}
}

FVekaShape FVekaShape::Rectangle()
{
	FVekaShape Shape;
	Shape.Kind = EVekaShapeKind::Rectangle;
	return Shape;
}

FVekaShape FVekaShape::RoundedRectangle(const FVekaCornerRadii& Radii, bool bContinuous)
{
	FVekaShape Shape;
	Shape.Kind = EVekaShapeKind::RoundedRectangle;
	Shape.CornerRadii = Radii;
	Shape.bContinuousCorners = bContinuous;
	return Shape;
}

FVekaShape FVekaShape::Capsule()
{
	FVekaShape Shape;
	Shape.Kind = EVekaShapeKind::Capsule;
	return Shape;
}

FVekaShape FVekaShape::Circle()
{
	FVekaShape Shape;
	Shape.Kind = EVekaShapeKind::Circle;
	return Shape;
}

FVekaShape FVekaShape::Ellipse()
{
	FVekaShape Shape;
	Shape.Kind = EVekaShapeKind::Ellipse;
	return Shape;
}

FVekaShape FVekaShape::FromPath(const FVekaPath& InPath, bool bUnitSpace)
{
	FVekaShape Shape;
	Shape.Kind = EVekaShapeKind::Path;
	Shape.Path = InPath;
	Shape.bPathInUnitSpace = bUnitSpace;
	return Shape;
}

FVekaRect FVekaShape::GetShapeRect(const FVekaRect& Bounds) const
{
	if (Kind == EVekaShapeKind::Circle)
	{
		const double Side = FMath::Min(Bounds.Size.X, Bounds.Size.Y);
		return FVekaRect(Bounds.Position + (Bounds.Size - FVector2D(Side, Side)) * 0.5, FVector2D(Side, Side));
	}
	return Bounds;
}

FVekaCornerRadii FVekaShape::ResolveRadii(const FVekaRect& ShapeRect) const
{
	switch (Kind)
	{
	case EVekaShapeKind::RoundedRectangle:
		return CornerRadii.ClampedTo(ShapeRect.Size);
	case EVekaShapeKind::Capsule:
	case EVekaShapeKind::Circle:
		return FVekaCornerRadii(static_cast<float>(FMath::Min(ShapeRect.Size.X, ShapeRect.Size.Y) * 0.5));
	default:
		return FVekaCornerRadii();
	}
}

FVekaPath FVekaShape::MakePath(const FVekaRect& Bounds) const
{
	const FVekaRect ShapeRect = GetShapeRect(Bounds);
	FVekaPath Result;
	switch (Kind)
	{
	case EVekaShapeKind::Rectangle:
		Result.AddRect(ShapeRect);
		break;
	case EVekaShapeKind::RoundedRectangle:
		Result.AddRoundedRect(ShapeRect, ResolveRadii(ShapeRect), bContinuousCorners);
		break;
	case EVekaShapeKind::Capsule:
		Result.AddRoundedRect(ShapeRect, ResolveRadii(ShapeRect), false);
		break;
	case EVekaShapeKind::Circle:
	case EVekaShapeKind::Ellipse:
		Result.AddEllipse(ShapeRect);
		break;
	case EVekaShapeKind::Path:
		Result = bPathInUnitSpace ? Path.FittedToRect(ShapeRect) : Path.Transformed(FVector2D(1.0, 1.0), ShapeRect.Position);
		break;
	}
	return Result;
}

bool FVekaShape::CanUseRoundedBox() const
{
	switch (Kind)
	{
	case EVekaShapeKind::Rectangle:
	case EVekaShapeKind::Capsule:
	case EVekaShapeKind::Circle:
		return true;
	case EVekaShapeKind::RoundedRectangle:
		return !bContinuousCorners;
	default:
		return false;
	}
}

double FVekaShape::SignedDistance(const FVector2D& Point, const FVekaRect& Bounds) const
{
	const FVekaRect ShapeRect = GetShapeRect(Bounds);
	const FVector2D Local = Point - ShapeRect.Center();
	const FVector2D Half = ShapeRect.Size * 0.5;

	switch (Kind)
	{
	case EVekaShapeKind::Rectangle:
		return RoundedBoxSDF(Local, Half, FVekaCornerRadii());
	case EVekaShapeKind::RoundedRectangle:
	case EVekaShapeKind::Capsule:
	case EVekaShapeKind::Circle:
		// Continuous corners are within ~0.5 unit of the circular ones: good enough for hit-testing.
		return RoundedBoxSDF(Local, Half, ResolveRadii(ShapeRect));
	case EVekaShapeKind::Ellipse:
		return EllipseSDF(Local, Half);
	case EVekaShapeKind::Path:
	{
		const FVekaPath Outline = MakePath(Bounds);
		return Outline.ContainsPoint(Point) ? -1.0 : 1.0;
	}
	}
	return 1.0;
}
