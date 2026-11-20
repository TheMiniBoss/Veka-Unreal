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

class FVekaRuntime;
class FVekaView;
class SVekaNodeWidget;
class SWidget;
class FReferenceCollector;

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

	// --- Layout --------------------------------------------------------------------------------

	/** Size for a proposal, modifiers included. Cached until the node is invalidated. */
	FVector2D Measure(const FVekaProposal& Proposal);

	/** Computes the modifier rects, the content rect and places the children for a final size. */
	void Arrange(const FVector2D& Size);

	/** Arrange + arrange every descendant with the rect it was given (no Slate needed). */
	void ArrangeRecursive(const FVector2D& Size);

	/** Rect given by the parent, in the parent's local space. */
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
	TSharedPtr<const FVekaView> View;
	FVekaElement SourceElement;
	FVekaEnvironment InheritedEnvironment;
	FVekaEnvironment Environment;
	TSharedPtr<FVekaPrimitiveState> PrimitiveState;
	TMap<FName, TSharedPtr<FVekaStateBoxBase>> States;
	TArray<TSharedRef<FVekaNode>> Children;
	FVekaNode* Parent = nullptr;
	FVekaGestureSet Gestures;
	double LayoutPriority = 0.0;
	bool bDirty = false;

	TArray<TPair<FVekaProposal, FVector2D>, TInlineAllocator<4>> MeasureCache;
	FVekaRect ArrangedRect;
	FVekaRect ContentRect;
	TArray<FVekaRect> ModifierRects;

	TSharedPtr<SVekaNodeWidget> Widget;
};
