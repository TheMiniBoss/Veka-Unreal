// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/SlateRenderTransform.h"
#include "Core/VekaPrimitive.h"
#include "VekaModifier.generated.h"

class FVekaEnvironment;

/** Gesture callbacks collected from the modifiers of a node. */
struct VEKAUI_API FVekaGestureSet
{
	TArray<TFunction<void()>> OnTap;
	TArray<TFunction<void(bool)>> OnHover;
	TArray<TFunction<void(bool)>> OnPress;

	bool IsEmpty() const { return OnTap.IsEmpty() && OnHover.IsEmpty() && OnPress.IsEmpty(); }
	void Reset() { OnTap.Reset(); OnHover.Reset(); OnPress.Reset(); }
};

/** Layout hints collected from the modifiers of a node. */
struct VEKAUI_API FVekaLayoutTraits
{
	double LayoutPriority = 0.0;
};

/**
 * Base of every modifier (padding, frame, background, opacity...). THIS IS THE EXTENSION API, like FVekaPrimitive.
 *
 * Modifiers are stored in call order: index 0 is the innermost (closest to the content).
 * `Text("Hi").Padding(8).Background(Red)` = [Padding, Background]: the background covers the padded area.
 *
 * Every visual value must be a UPROPERTY (the reconciler diffs UPROPERTYs). Callbacks (gestures) may be
 * plain C++ members: they are always refreshed from the latest element.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaModifier
{
	GENERATED_BODY()

	virtual ~FVekaModifier() = default;

	using FMeasureFn = TFunctionRef<FVector2D(const FVekaProposal&)>;

	/** Size for a proposal, given the measure function of everything inside this modifier. */
	virtual FVector2D Measure(const FVekaProposal& Proposal, FMeasureFn Inner) const { return Inner(Proposal); }

	/** Rect given to the inside, from the rect given to this modifier. */
	virtual FVekaRect Place(const FVekaRect& Outer, FMeasureFn Inner) const { return Outer; }

	/** Changes the environment seen by the content and the children. */
	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const {}

	/** Changes the widget style (opacity, tint) used for everything inside this modifier. */
	virtual void ModifyWidgetStyle(FWidgetStyle& Style) const {}

	/** Render transform (no relayout), applied around the center of the node. */
	virtual void ModifyRenderTransform(FSlateRenderTransform& Transform, const FVector2D& Size) const {}
	virtual bool HasRenderTransform() const { return false; }

	virtual void CollectGestures(FVekaGestureSet& Gestures) const {}
	virtual void ModifyLayoutTraits(FVekaLayoutTraits& Traits) const {}

	/** Drawn under the inside of this modifier, in Rect (the rect given to this modifier). */
	virtual int32 PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const { return Context.LayerId; }

	/** Drawn over the inside of this modifier. */
	virtual int32 PaintFront(FVekaPaintContext& Context, const FVekaRect& Rect) const { return Context.LayerId; }

	/** Everything inside this modifier is clipped to its rect. */
	virtual bool ClipsContent() const { return false; }
};
