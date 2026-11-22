// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Core/VekaTypes.h"
#include "Anima/VekaAnimationSpec.h"

class FVekaNode;
class FVekaRuntime;
class FProperty;

namespace Veka
{
	/**
	 * Runs Body; the views re-evaluated because of state changed inside it animate their changes with Spec
	 * (SwiftUI's withAnimation): animatable values, layout positions, insertion / removal transitions.
	 *
	 *   .OnTap([Expanded]() { Veka::WithAnimation(FVekaAnimationSpec::Bouncy(), [&] { Expanded.Set(!Expanded.Get()); }); })
	 */
	VEKAUI_API void WithAnimation(const FVekaAnimationSpec& Spec, TFunctionRef<void()> Body);
}

namespace Veka::Anima
{
	/** Spec of the innermost WithAnimation running on this thread, or nullptr. */
	VEKAUI_API const FVekaAnimationSpec* GetActiveTransaction();
}

/**
 * Implicit animations of a runtime. A node keeps the values of its last element (targets) and the values it
 * draws (presented). When an animation transaction is active, every changed float / double / FLinearColor
 * UPROPERTY of the primitive and modifiers (found by reflection, nested structs and same-size arrays included)
 * animates from its presented value to the new target; otherwise it snaps. Also drives layout (rect) animations
 * and insertion / removal transitions.
 */
class VEKAUI_API FVekaImplicitAnimator
{
public:
	explicit FVekaImplicitAnimator(FVekaRuntime& InRuntime);
	~FVekaImplicitAnimator();

	/** Sets the node's targets and presented structs from a new element (NewPrimitive is null for views). */
	void UpdateNode(FVekaNode& Node, const FInstancedStruct* NewPrimitive, const TArray<FInstancedStruct>& NewModifiers, const FVekaAnimationSpec* Transaction);

	void AnimateRect(FVekaNode& Node, const FVekaRect& Target, const FVekaAnimationSpec& Spec);
	void CancelRect(FVekaNode& Node);
	bool IsRectAnimating(const FVekaNode& Node) const;

	/** Plays the insertion (progress -> 1) or removal (progress -> 0) transition; OnDone runs at the end of a removal. */
	void StartTransition(FVekaNode& Node, bool bInsertion, const FVekaAnimationSpec& Spec, TFunction<void()> OnDone = nullptr);

	/** Cancels everything that animates this node. */
	void ForgetNode(const FVekaNode& Node);

	/** Number of running implicit value animations (tests). */
	int32 NumValueAnimations() const { return Leaves.Num(); }

	/** After the scheduler ticked: relayout / repaint / re-sync the nodes whose presented values moved. */
	void FlushTouched();

private:
	struct FPathStep
	{
		const FProperty* Property = nullptr;
		int32 Index = -1;
	};
	enum class ELeafKind : uint8 { Float, Double, Color };
	struct FLeaf;
	struct FRectAnimation;
	struct FTransitionAnimation;

	void ProcessSlot(FVekaNode& Node, int32 Slot, FInstancedStruct& OutPresented, const FInstancedStruct& OldPresented,
		const FInstancedStruct& OldTarget, const FInstancedStruct& NewTarget, const FVekaAnimationSpec* Spec);
	void StartLeaf(FVekaNode& Node, int32 Slot, const UScriptStruct* Struct, const TArray<FPathStep>& Path, ELeafKind Kind,
		const uint8* From, const uint8* To, const FVekaAnimationSpec& Spec);
	void CancelLeaf(const FString& Key);
	void CancelSlot(const FVekaNode& Node, int32 Slot, const TArray<FPathStep>* Prefix = nullptr);
	void ApplyLeaf(FLeaf& Leaf);
	/** Node changed this frame; bLayout: its size may have changed too (else it only repaints). */
	void Touch(FVekaNode& Node, bool bLayout = true);

	static FString MakeKey(const FVekaNode& Node, int32 Slot, const TArray<FPathStep>& Path);
	static uint8* ResolvePath(uint8* Container, TConstArrayView<FPathStep> Path);
	static FInstancedStruct* GetSlot(FVekaNode& Node, int32 Slot);

	FVekaRuntime& Runtime;
	TMap<FString, TSharedPtr<FLeaf>> Leaves;
	TMap<const FVekaNode*, TSharedPtr<FRectAnimation>> Rects;
	TMap<const FVekaNode*, TSharedPtr<FTransitionAnimation>> Transitions;
	TArray<TWeakPtr<FVekaNode>> Touched;
	/** The nodes of Touched (dedup without a linear search). */
	TSet<const FVekaNode*> TouchedSet;
	/** Touched nodes whose animated value can change their environment (the others skip RefreshEnvironment). */
	TSet<const FVekaNode*> TouchedEnvironment;
	/** Touched nodes whose layout may have changed (the others get SyncPresentation). */
	TSet<const FVekaNode*> TouchedLayout;
};
