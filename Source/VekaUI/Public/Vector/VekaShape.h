// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaTypes.h"
#include "Vector/VekaPath.h"
#include "VekaShape.generated.h"

UENUM(BlueprintType)
enum class EVekaShapeKind : uint8
{
	Rectangle,
	RoundedRectangle,
	Capsule,
	Circle,
	Ellipse,
	/** Any FVekaPath. */
	Path
};

/**
 * Geometry of a shape inside a rect: gives its outline (path) and an exact hit test (signed distance).
 * The same description drives drawing, hit-testing and (Phase 2) morphing and SDF fusion.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape")
	EVekaShapeKind Kind = EVekaShapeKind::Rectangle;

	/** RoundedRectangle radii. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape")
	FVekaCornerRadii CornerRadii;

	/** Smooth (continuous curvature) corners instead of circular arcs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape")
	bool bContinuousCorners = false;

	/** Kind = Path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape")
	FVekaPath Path;

	/** True: Path is drawn in the unit square and stretched to the rect. False: Path is in local units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape")
	bool bPathInUnitSpace = true;

	static FVekaShape Rectangle();
	static FVekaShape RoundedRectangle(const FVekaCornerRadii& Radii, bool bContinuous = false);
	static FVekaShape Capsule();
	static FVekaShape Circle();
	static FVekaShape Ellipse();
	static FVekaShape FromPath(const FVekaPath& InPath, bool bUnitSpace = true);

	/** Rect actually covered by the shape (a circle uses the centered square). */
	FVekaRect GetShapeRect(const FVekaRect& Bounds) const;

	/** Corner radii in local units for a given shape rect (capsule = half the short side). */
	FVekaCornerRadii ResolveRadii(const FVekaRect& ShapeRect) const;

	/** Outline as a path in local units. */
	FVekaPath MakePath(const FVekaRect& Bounds) const;

	/** True if Slate's native rounded box can draw it (fast path, batched, antialiased). */
	bool CanUseRoundedBox() const;

	/** Negative inside, positive outside (approximate for ellipses and paths). */
	double SignedDistance(const FVector2D& Point, const FVekaRect& Bounds) const;
	bool Contains(const FVector2D& Point, const FVekaRect& Bounds) const { return SignedDistance(Point, Bounds) <= 0.0; }
};
