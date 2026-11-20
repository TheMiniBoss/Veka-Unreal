// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"
#include "Core/VekaTypes.h"
#include "VekaPrimitive.generated.h"

class FVekaNode;
class FVekaEnvironment;
class SWidget;

/** Mutable runtime data owned by a node for its primitive (scroll offset, text layout cache, brushes...). */
struct VEKAUI_API FVekaPrimitiveState
{
	virtual ~FVekaPrimitiveState() = default;

	/** True while the primitive animates on its own (text effects, content transitions): the widget repaints every frame. */
	virtual bool NeedsAnimationFrame(double Now) const { return false; }
};

/** What a layout sees of one child: it can ask for a size and place it. */
class VEKAUI_API FVekaLayoutChild
{
public:
	explicit FVekaLayoutChild(FVekaNode& InNode) : Node(&InNode) {}

	/** Size the child wants for this proposal (includes the child's own modifiers). */
	FVector2D SizeThatFits(const FVekaProposal& Proposal) const;

	/** Gives the child its final rect, in the parent's local space. */
	void Place(const FVekaRect& Rect) const;

	/** Higher priority children are offered space first by stacks (see LayoutPriority modifier). */
	double GetLayoutPriority() const;

	FVekaNode& GetNode() const { return *Node; }

private:
	FVekaNode* Node;
};

/** Passed to layout functions. */
struct VEKAUI_API FVekaLayoutContext
{
	FVekaNode& Node;
	const FVekaEnvironment& Environment;
	FVekaPrimitiveState* State;
	/** Axis of the enclosing Stack, if the node is a direct child of one (used by Spacer). */
	TOptional<EVekaAxis> ParentStackAxis;
};

/** Passed to paint functions. LayerId is the layer to draw on; return the highest layer used. */
struct VEKAUI_API FVekaPaintContext
{
	const FGeometry& Geometry;
	FSlateWindowElementList& DrawElements;
	int32 LayerId;
	const FWidgetStyle& WidgetStyle;
	ESlateDrawEffect DrawEffects;
	const FVekaEnvironment& Environment;
	FVekaPrimitiveState* State;

	/** Paint geometry of a local rect. */
	FPaintGeometry MakePaintGeometry(const FVekaRect& Rect) const;

	/** Color with the environment's color adjustments, then the inherited opacity / tint. */
	FLinearColor Tint(const FLinearColor& Color) const;
};

/** Passed to input handlers. */
struct VEKAUI_API FVekaInputContext
{
	FVekaNode& Node;
	FVekaPrimitiveState* State;
	const FGeometry& Geometry;
};

/**
 * Base of every Veka component (leaf or container). THIS IS THE EXTENSION API:
 * Veka's own Text, Shape, Stack, Grid, ScrollView... are written exactly like a user primitive would be.
 *
 * Rules:
 *  - Derive a USTRUCT from FVekaPrimitive. Every value that changes how the component looks or lays out
 *    must be a UPROPERTY: the reconciler compares UPROPERTYs to decide what to invalidate.
 *  - Behaviour is in the virtual functions below. Runtime data goes into a FVekaPrimitiveState (CreateState).
 *  - The struct is registered automatically in FVekaSchema (DSL, inspector, docs). Optional metadata:
 *    meta = (VekaName = "Text", VekaCategory = "Primitives").
 *
 * The base implementation behaves like a ZStack: children are layered and centered.
 */
USTRUCT(BlueprintType, meta = (VekaName = "Group", VekaCategory = "Layout"))
struct VEKAUI_API FVekaPrimitive
{
	GENERATED_BODY()

	virtual ~FVekaPrimitive() = default;

	/** Size this component wants for the proposal. Children are measured through the proxies. */
	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const;

	/** Places every child inside Bounds (local space of this node). */
	virtual void PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const;

	/** Draws the component under its children. */
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const { return Context.LayerId; }

	/** Draws the component over its children (scroll indicators, focus rings...). */
	virtual int32 PaintOverlay(FVekaPaintContext& Context, const FVekaRect& Bounds) const { return Context.LayerId; }

	/** True if the local point (relative to Bounds origin) hits the component. Default: the whole rect. */
	virtual bool HitTest(const FVector2D& LocalPoint, const FVector2D& Size) const;

	/** Children are clipped to the content rect. */
	virtual bool ClipsChildren() const { return false; }

	/** Set for stacks so Spacers know which axis to expand along. */
	virtual TOptional<EVekaAxis> GetStackAxis() const { return TOptional<EVekaAxis>(); }

	/** False for leaves; children of a leaf are ignored. */
	virtual bool AcceptsChildren() const { return true; }

	/** Runtime state created once per node (kept while the node keeps the same primitive type). */
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const { return nullptr; }

	/** True if the primitive handles input itself (keeps the node widget hit-testable). */
	virtual bool WantsInput() const { return false; }

	virtual FReply OnMouseWheel(FVekaInputContext& Context, const FPointerEvent& Event) const { return FReply::Unhandled(); }

	/**
	 * A native Slate widget placed over the content rect (text input...), owned by the state so it survives
	 * re-evaluations. nullptr for drawn primitives.
	 */
	virtual TSharedPtr<SWidget> GetNativeWidget(FVekaPrimitiveState* State) const { return nullptr; }
};
