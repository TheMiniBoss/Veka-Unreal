// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaModifier.h"
#include "Core/VekaElement.h"
#include "Core/VekaPaint.h"
#include "Vector/VekaShape.h"
#include "Vector/VekaPath.h"
#include "VekaMorph.generated.h"

class FVekaEnvironment;

/** One shape of a ShapeGroup as seen by the group (captured at layout, in the group's local space). */
struct VEKAUI_API FVekaMorphShape
{
	FVekaShape Shape;
	/** Rect the shape is drawn in, in its own node's local space. */
	FVekaRect Bounds;
	/** Position of its node in the group's local space. */
	FVector2D Origin = FVector2D::ZeroVector;
	/** Offset / scale / rotation modifiers of its node (around the node's center). */
	bool bTransformed = false;
	FSlateRenderTransform InverseTransform;
	FVector2D Pivot = FVector2D::ZeroVector;
	double DistanceScale = 1.0;
	/** Paths: outline in the node's local space (signed distance to it). */
	TArray<FVector2D> Polygon;

	/** Signed distance at a point of the group's local space (negative inside). */
	double SignedDistance(const FVector2D& Point) const;
	/** Approximate bounds in the group's local space. */
	FVekaRect GetGroupBounds() const;
};

/**
 * Shapes that melt into each other (SwiftUI-like "liquid" morph): the children shapes are not drawn one by one,
 * their signed distance fields are merged with a smooth minimum and the contour is filled as one shape. Move the
 * children (offset, frame, layout) and the outline flows; hit-testing follows the merged outline.
 *
 *   Veka::ShapeGroup(18)(Circle().Frame(80, 80), Circle().Frame(36, 36).Offset({ X, 0 }))
 *
 * Shapes anywhere in the subtree are merged; other views (text, symbols) draw normally on top.
 */
USTRUCT(BlueprintType, meta = (VekaName = "ShapeGroup", VekaCategory = "Morph"))
struct VEKAUI_API FVekaShapeGroup : public FVekaPrimitive
{
	GENERATED_BODY()

	/** Blend radius: how far apart two shapes start to melt together (0 = plain union). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Morph", meta = (ClampMin = "0"))
	float Smoothness = 16.f;

	/** Fill of the merged shape; otherwise the first child's fill (or the foreground color). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Morph", meta = (InlineEditConditionToggle))
	bool bOverrideFill = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Morph", meta = (EditCondition = "bOverrideFill"))
	FVekaPaint Fill;

	/** Shapes captured at the last layout (written during PlaceChildren, read by Paint and HitTest). */
	mutable TArray<FVekaMorphShape> CapturedShapes;
	mutable FVekaPaint CapturedFill;
	mutable bool bCapturedForeground = true;
	mutable FVector2D CapturedContentOffset = FVector2D::ZeroVector;

	virtual void PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool HitTest(const FVector2D& LocalPoint, const FVector2D& Size) const override;
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;
};

/** Marks the subtree of a ShapeGroup: its shapes leave the drawing to the group. Added by Veka::ShapeGroup. */
USTRUCT(meta = (VekaName = "shapeGroupScope", VekaCategory = "Morph"))
struct VEKAUI_API FVekaShapeGroupScopeModifier : public FVekaModifier
{
	GENERATED_BODY()

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
	virtual bool ModifiesEnvironment() const override { return true; }
};

/**
 * SwiftUI matchedGeometryEffect: when a view with the same id appears while another leaves (in an animation),
 * it starts from the other's frame and animates to its own; the leaving view flies to the new frame as it fades.
 */
USTRUCT(BlueprintType, meta = (VekaName = "matchedGeometry", VekaCategory = "Morph"))
struct VEKAUI_API FVekaMatchedGeometryModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Morph")
	FName Id;

	/** Ids are matched inside a namespace (two lists can both use "card"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Morph")
	FName Namespace;
};

namespace VekaEnv
{
	inline FName InShapeGroup() { static const FName Name(TEXT("Veka.InShapeGroup")); return Name; }

	VEKAUI_API bool IsInShapeGroup(const FVekaEnvironment& Environment);
}

namespace Veka
{
	/** Builder of a ShapeGroup: `ShapeGroup(16)(children...)`. */
	class VEKAUI_API FShapeGroupBuilder
	{
	public:
		explicit FShapeGroupBuilder(const FVekaShapeGroup& InGroup) : Group(InGroup) {}

		template <typename... TChildren>
		FVekaElement operator()(TChildren&&... InChildren) const
		{
			TArray<FVekaElement> ChildArray;
			ChildArray.Reserve(sizeof...(TChildren));
			(ChildArray.Add(FVekaElement(Forward<TChildren>(InChildren))), ...);
			return Children(MoveTemp(ChildArray));
		}

		FVekaElement Children(TArray<FVekaElement> InChildren) const;

	private:
		FVekaShapeGroup Group;
	};

	VEKAUI_API FShapeGroupBuilder ShapeGroup(float Smoothness = 16.f);
	VEKAUI_API FShapeGroupBuilder ShapeGroup(float Smoothness, const FVekaPaint& Fill);
}

namespace Veka::Morph
{
	/** Polynomial smooth minimum (k = blend radius); equals min(A, B) when they are more than k apart. */
	VEKAUI_API double SmoothMin(double A, double B, double K);

	/** Merged field of shapes at a point. */
	VEKAUI_API double Field(TConstArrayView<FVekaMorphShape> Shapes, double Smoothness, const FVector2D& Point);

	/**
	 * Closed contour(s) of Field = 0 over Area (marching squares, CellSize apart, edge crossings interpolated).
	 * Each loop is a closed polyline.
	 */
	VEKAUI_API void ExtractContour(TFunctionRef<double(const FVector2D&)> InField, const FVekaRect& Area, double CellSize, TArray<FVekaPolyline>& OutLoops);
}
