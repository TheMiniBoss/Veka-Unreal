// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Rendering/SlateRenderTransform.h"
#include "Core/VekaTypes.h"
#include "Core/VekaElement.h"
#include "Core/VekaEnvironment.h"
#include "Core/VekaModifier.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaState.h"
#include "Anima/VekaAnimationSpec.h"
#include "Anima/VekaTransition.h"

class FVekaRuntime;
class FVekaView;
class SVekaNodeWidget;
class SWidget;
class FReferenceCollector;
class FVekaImplicitAnimator;

/**
 * Retained instance of an element: owns @State, primitive state, layout results and (when displayed)
 * one Slate widget. Created and updated by FVekaRuntime (the reconciler). Layout works without Slate,
 * so nodes can be measured in tests or, later, off the game thread.
 */
class VEKAUI_API FVekaNode : public TSharedFromThis<FVekaNode>
{
public:
	FVekaNode(FVekaRuntime& InRuntime, FName InKey, EVekaElementKind InKind, FName InTypeName);
	~FVekaNode();

	FVekaNode(const FVekaNode&) = delete;
	FVekaNode& operator=(const FVekaNode&) = delete;

	// --- Description ---------------------------------------------------------------------------

	FName GetKey() const { return Key; }
	EVekaElementKind GetKind() const { return Kind; }
	FName GetTypeName() const { return TypeName; }
	const FVekaPrimitive& GetPrimitive() const;
	const FInstancedStruct& GetPrimitiveStruct() const { return Primitive; }
	template <typename T>
	const T* GetPrimitiveAs() const { return Primitive.GetPtr<T>(); }
	const TArray<FInstancedStruct>& GetModifiers() const { return Modifiers; }
	const TArray<TSharedRef<FVekaNode>>& GetChildren() const { return Children; }
	FVekaNode* GetParent() const { return Parent; }
	FVekaRuntime& GetRuntime() const { return Runtime; }
	/** Environment seen by this node's content and children (after its own modifiers). */
	const FVekaEnvironment& GetEnvironment() const { return Environment; }
	FVekaPrimitiveState* GetPrimitiveState() const { return PrimitiveState.Get(); }
	const FVekaGestureSet& GetGestures() const { return Gestures; }
	double GetLayoutPriority() const { return LayoutPriority; }
	int32 GetDepth() const;

	/** Values of the last element; the presented modifiers (GetModifiers) may be animating toward them. */
	const TArray<FInstancedStruct>& GetTargetModifiers() const { return TargetModifiers; }

	/** First modifier of type T (presented values), or nullptr. */
	template <typename T>
	const T* FindModifier() const
	{
		for (const FInstancedStruct& Modifier : Modifiers)
		{
			if (const T* Typed = Modifier.GetPtr<T>())
			{
				return Typed;
			}
		}
		return nullptr;
	}

	// --- Anima ---------------------------------------------------------------------------------

	/** Children removed inside an animation transaction, still drawn while their removal transition plays. */
	const TArray<TSharedRef<FVekaNode>>& GetRemovedChildren() const { return RemovedChildren; }
	bool IsRemoving() const { return bRemoving; }
	/** True if this node or an ancestor is being removed (no re-evaluation, no input). */
	bool IsInRemovedSubtree() const;
	/** 1 = identity; insertion goes 0 -> 1, removal 1 -> 0. */
	float GetTransitionProgress() const { return TransitionProgress; }
	float GetTransitionOpacity() const;

	/** Recomputes the environment from the presented modifiers and pushes it down (views are not re-evaluated). */
	void RefreshEnvironment();
	/** Environment seen inside this node: inherited + its modifiers, outermost first. */
	FVekaEnvironment ComputeEnvironment() const;

	// --- Layout --------------------------------------------------------------------------------

	/** Size for a proposal, modifiers included. Cached until the node is invalidated. */
	FVector2D Measure(const FVekaProposal& Proposal);

	/** Computes the modifier rects, the content rect and places the children for a final size. */
	void Arrange(const FVector2D& Size);

	/**
	 * Proposal that made the content (inside the modifiers) measure Size, if it was measured recently. Containers
	 * re-run their distribution with it when placing, so placement matches measurement exactly.
	 */
	TOptional<FVekaProposal> FindContentProposal(const FVector2D& Size) const;

	/** Arrange + arrange every descendant with the rect it was given (no Slate needed). */
	void ArrangeRecursive(const FVector2D& Size);

	/** Sets the rect given by the parent layout (animated when a layout transaction is active). */
	void PlaceAt(const FVekaRect& Rect);

	/** Rect given by the parent, in the parent's local space (presented: may be animating). */
	const FVekaRect& GetArrangedRect() const { return ArrangedRect; }
	/** Rect of the content (inside all modifiers), local space. */
	const FVekaRect& GetContentRect() const { return ContentRect; }
	/** Rect given to modifier Index (local space). */
	const FVekaRect& GetModifierRect(int32 Index) const { return ModifierRects[Index]; }

	bool HitTest(const FVector2D& LocalPoint) const;
	bool HasRenderTransform() const;
	FSlateRenderTransform ComputeRenderTransform(const FVector2D& Size) const;
	/** True if the node needs pointer events (gestures or interactive primitive). */
	bool WantsInput() const;

	// --- Invalidation --------------------------------------------------------------------------

	/** State changed: re-evaluate this view at the end of the frame. */
	void MarkDirty();
	/** Clears measure caches up to the root and asks Slate for a new layout pass. */
	void InvalidateLayout();
	void InvalidatePaint();

	// --- Slate ---------------------------------------------------------------------------------

	TSharedRef<SWidget> GetOrCreateWidget();
	bool HasWidget() const { return Widget.IsValid(); }

	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	friend class FVekaRuntime;
	friend class FVekaContext;
	friend class FVekaLayoutChild;
	friend class FVekaImplicitAnimator;

	FVector2D MeasureLevel(int32 Level, const FVekaProposal& Proposal);
	FVector2D MeasureContent(const FVekaProposal& Proposal);
	TArray<FVekaLayoutChild> MakeChildProxies() const;
	TOptional<EVekaAxis> GetParentStackAxis() const;
	void SyncWidget();

	FVekaRuntime& Runtime;
	FName Key;
	EVekaElementKind Kind;
	FName TypeName;

	FInstancedStruct Primitive;
	TArray<FInstancedStruct> Modifiers;
	FInstancedStruct TargetPrimitive;
	TArray<FInstancedStruct> TargetModifiers;
	TSharedPtr<const FVekaView> View;
	FVekaElement SourceElement;
	FVekaEnvironment InheritedEnvironment;
	FVekaEnvironment Environment;
	TSharedPtr<FVekaPrimitiveState> PrimitiveState;
	TMap<FName, TSharedPtr<FVekaStateBoxBase>> States;
	TArray<TSharedRef<FVekaNode>> Children;
	TArray<TSharedRef<FVekaNode>> RemovedChildren;
	FVekaNode* Parent = nullptr;
	FVekaGestureSet Gestures;
	double LayoutPriority = 0.0;
	bool bDirty = false;
	/** Children were reconciled at least once (insertions after that can play transitions). */
	bool bBuilt = false;
	/** Transaction captured by WithAnimation when this view was marked dirty. */
	TOptional<FVekaAnimationSpec> PendingTransaction;

	TArray<FVekaTransitionEffect> TransitionEffects;
	float TransitionProgress = 1.f;
	bool bRemoving = false;
	FVekaRect LayoutTarget;
	bool bPlaced = false;

	TArray<TPair<FVekaProposal, FVector2D>, TInlineAllocator<4>> MeasureCache;
	TArray<TPair<FVekaProposal, FVector2D>, TInlineAllocator<4>> ContentMeasureCache;
	FVekaRect ArrangedRect;
	FVekaRect ContentRect;
	TArray<FVekaRect> ModifierRects;

	TSharedPtr<SVekaNodeWidget> Widget;
};
