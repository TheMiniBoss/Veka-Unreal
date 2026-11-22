// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/SlateRenderTransform.h"
#include "Core/VekaPrimitive.h"
#include "VekaModifier.generated.h"

class FVekaEnvironment;

enum class EVekaDragPhase : uint8
{
	Began,
	Changed,
	Ended
};

/** One step of a drag (SwiftUI DragGesture.Value). Positions are in the node's local space. */
struct FVekaDragValue
{
	EVekaDragPhase Phase = EVekaDragPhase::Began;
	FVector2D Location = FVector2D::ZeroVector;
	FVector2D StartLocation = FVector2D::ZeroVector;
	FVector2D Translation = FVector2D::ZeroVector;
	/** Size of the node (to turn a location into a fraction). */
	FVector2D Size = FVector2D::ZeroVector;
};

/** Gesture callbacks collected from the modifiers of a node. */
struct VEKAUI_API FVekaGestureSet
{
	TArray<TFunction<void()>> OnTap;
	TArray<TFunction<void(bool)>> OnHover;
	TArray<TFunction<void(bool)>> OnPress;
	/** Right click (context menus); screen position of the click. */
	TArray<TFunction<void(const FVector2D&)>> OnSecondaryTap;
	TArray<TFunction<void(const FVekaDragValue&)>> OnDrag;
	/** Keys while focused; return true when handled. Unhandled Enter / Space activate OnTap. */
	TArray<TFunction<bool(const FKeyEvent&)>> OnKey;
	/** Keyboard focus shown (true only for keyboard navigation, so a click shows no focus ring). */
	TArray<TFunction<void(bool)>> OnFocus;
	/** Takes keyboard focus (Tab navigation, click). */
	bool bFocusable = false;

	bool IsEmpty() const
	{
		return OnTap.IsEmpty() && OnHover.IsEmpty() && OnPress.IsEmpty() && OnSecondaryTap.IsEmpty() && OnDrag.IsEmpty()
			&& OnKey.IsEmpty() && OnFocus.IsEmpty() && !bFocusable;
	}
	void Reset()
	{
		OnTap.Reset(); OnHover.Reset(); OnPress.Reset(); OnSecondaryTap.Reset(); OnDrag.Reset(); OnKey.Reset(); OnFocus.Reset();
		bFocusable = false;
	}
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
	/** True for the modifiers that override ModifyEnvironment (an animated value of the others never changes the environment). */
	virtual bool ModifiesEnvironment() const { return false; }
	/** False for purely visual modifiers (offset, opacity, background...): an animated value of theirs only repaints. */
	virtual bool AffectsLayout() const { return true; }

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

	/** The inside of this modifier is painted twice, with PaintBetween in between (blend modes against the backdrop). */
	virtual bool PaintsContentTwice() const { return false; }
	virtual int32 PaintBetween(FVekaPaintContext& Context, const FVekaRect& Rect) const { return Context.LayerId; }
};
