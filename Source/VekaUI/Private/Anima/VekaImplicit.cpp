// Copyright TheMiniBoss. All Rights Reserved.

#include "Anima/VekaImplicit.h"
#include "Anima/VekaTransition.h"
#include "Core/VekaNode.h"
#include "Core/VekaRuntime.h"
#include "Colora/VekaColor.h"
#include "Modifiers/VekaAnimaModifiers.h"
#include "Modifiers/VekaLayoutModifiers.h"
#include "Modifiers/VekaInteractionModifiers.h"
#include "UObject/UnrealType.h"

// --- WithAnimation -----------------------------------------------------------------------------------------

namespace
{
	TArray<FVekaAnimationSpec>& GetTransactionStack()
	{
		static thread_local TArray<FVekaAnimationSpec> Stack;
		return Stack;
	}
}

namespace Veka
{
	void WithAnimation(const FVekaAnimationSpec& Spec, TFunctionRef<void()> Body)
	{
		GetTransactionStack().Push(Spec);
		Body();
		GetTransactionStack().Pop();
	}
}

namespace Veka::Anima
{
	const FVekaAnimationSpec* GetActiveTransaction()
	{
		TArray<FVekaAnimationSpec>& Stack = GetTransactionStack();
		return Stack.Num() > 0 ? &Stack.Last() : nullptr;
	}
}

// --- Animator ----------------------------------------------------------------------------------------------

struct FVekaImplicitAnimator::FLeaf
{
	TWeakPtr<FVekaNode> Node;
	const FVekaNode* NodeRaw = nullptr;
	int32 Slot = -1;
	const UScriptStruct* Struct = nullptr;
	TArray<FPathStep> Path;
	ELeafKind Kind = ELeafKind::Double;
	FString Key;

	double Scalar = 0.0;
	FLinearColor Color = FLinearColor::Transparent;
	FLinearColor FromColor = FLinearColor::Transparent;
	FLinearColor ToColor = FLinearColor::Transparent;
};

struct FVekaImplicitAnimator::FRectAnimation
{
	TWeakPtr<FVekaNode> Node;
	FVekaRect From;
	FVekaRect To;
};

struct FVekaImplicitAnimator::FTransitionAnimation
{
	TWeakPtr<FVekaNode> Node;
};

namespace
{
	/** Structs whose values are configuration, not something to animate. */
	bool IsSkippedStruct(const UScriptStruct* Struct)
	{
		return Struct == FVekaAnimationSpec::StaticStruct() || Struct == FVekaTransition::StaticStruct() || Struct == FVekaTransitionEffect::StaticStruct();
	}

	bool IsNonAnimatedSlot(const UScriptStruct* Struct)
	{
		return Struct == FVekaAnimationModifier::StaticStruct() || Struct == FVekaTransitionModifier::StaticStruct()
			|| Struct == FVekaOnTapModifier::StaticStruct() || Struct == FVekaOnHoverModifier::StaticStruct() || Struct == FVekaOnPressModifier::StaticStruct();
	}

	double ReadScalar(const uint8* Ptr, bool bFloat)
	{
		return bFloat ? static_cast<double>(*reinterpret_cast<const float*>(Ptr)) : *reinterpret_cast<const double*>(Ptr);
	}

	void WriteScalar(uint8* Ptr, bool bFloat, double Value)
	{
		if (bFloat)
		{
			*reinterpret_cast<float*>(Ptr) = static_cast<float>(Value);
		}
		else
		{
			*reinterpret_cast<double*>(Ptr) = Value;
		}
	}

}

/** One step of a reflection path (the class's own FPathStep is private). */
struct FVekaImplicitAnimatorPath
{
	const FProperty* Property = nullptr;
	int32 Index = -1;
};

namespace
{
	enum class EKind : uint8 { Float, Double, Color };

	template <typename TVisit, typename TStructural>
	void VisitValue(const FProperty* Property, const uint8* Old, const uint8* New, TArray<FVekaImplicitAnimatorPath>& Path, TVisit& Visit, TStructural& Structural);

	template <typename TVisit, typename TStructural>
	void WalkStruct(const UStruct* Struct, const uint8* Old, const uint8* New, TArray<FVekaImplicitAnimatorPath>& Path, TVisit& Visit, TStructural& Structural)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			if (Property->ArrayDim != 1)
			{
				continue;
			}
			Path.Push({ Property, -1 });
			VisitValue(Property, Property->ContainerPtrToValuePtr<uint8>(Old), Property->ContainerPtrToValuePtr<uint8>(New), Path, Visit, Structural);
			Path.Pop();
		}
	}

	template <typename TVisit, typename TStructural>
	void VisitValue(const FProperty* Property, const uint8* Old, const uint8* New, TArray<FVekaImplicitAnimatorPath>& Path, TVisit& Visit, TStructural& Structural)
	{
		if (Property->IsA<FFloatProperty>())
		{
			Visit(EKind::Float, Old, New);
		}
		else if (Property->IsA<FDoubleProperty>())
		{
			Visit(EKind::Double, Old, New);
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				Visit(EKind::Color, Old, New);
			}
			else if (!IsSkippedStruct(StructProperty->Struct))
			{
				WalkStruct(StructProperty->Struct, Old, New, Path, Visit, Structural);
			}
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper OldArray(ArrayProperty, Old);
			FScriptArrayHelper NewArray(ArrayProperty, New);
			if (OldArray.Num() != NewArray.Num())
			{
				Structural();
				return;
			}
			for (int32 Index = 0; Index < NewArray.Num(); ++Index)
			{
				Path.Last().Index = Index;
				VisitValue(ArrayProperty->Inner, OldArray.GetRawPtr(Index), NewArray.GetRawPtr(Index), Path, Visit, Structural);
			}
			Path.Last().Index = -1;
		}
	}
}

FVekaImplicitAnimator::FVekaImplicitAnimator(FVekaRuntime& InRuntime)
	: Runtime(InRuntime)
{
}

FVekaImplicitAnimator::~FVekaImplicitAnimator() = default;

FString FVekaImplicitAnimator::MakeKey(const FVekaNode& Node, int32 Slot, const TArray<FPathStep>& Path)
{
	FString Key = FString::Printf(TEXT("%p/%d"), &Node, Slot);
	for (const FPathStep& Step : Path)
	{
		Key += TEXT("/");
		Key += Step.Property->GetName();
		if (Step.Index >= 0)
		{
			Key += FString::Printf(TEXT("[%d]"), Step.Index);
		}
	}
	return Key;
}

uint8* FVekaImplicitAnimator::ResolvePath(uint8* Container, TConstArrayView<FPathStep> Path)
{
	uint8* Ptr = Container;
	for (const FPathStep& Step : Path)
	{
		if (!Ptr)
		{
			return nullptr;
		}
		uint8* Value = Step.Property->ContainerPtrToValuePtr<uint8>(Ptr);
		if (Step.Index >= 0)
		{
			const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Step.Property);
			if (!ArrayProperty)
			{
				return nullptr;
			}
			FScriptArrayHelper Array(ArrayProperty, Value);
			if (!Array.IsValidIndex(Step.Index))
			{
				return nullptr;
			}
			Value = Array.GetRawPtr(Step.Index);
		}
		Ptr = Value;
	}
	return Ptr;
}

FInstancedStruct* FVekaImplicitAnimator::GetSlot(FVekaNode& Node, int32 Slot)
{
	if (Slot < 0)
	{
		return &Node.Primitive;
	}
	return Node.Modifiers.IsValidIndex(Slot) ? &Node.Modifiers[Slot] : nullptr;
}

void FVekaImplicitAnimator::UpdateNode(FVekaNode& Node, const FInstancedStruct* NewPrimitive, const TArray<FInstancedStruct>& NewModifiers, const FVekaAnimationSpec* Transaction)
{
	if (NewPrimitive)
	{
		const FInstancedStruct OldPresented = Node.Primitive;
		const FInstancedStruct OldTarget = Node.TargetPrimitive;
		FInstancedStruct Presented;
		ProcessSlot(Node, -1, Presented, OldPresented, OldTarget, *NewPrimitive, Transaction);
		Node.Primitive = MoveTemp(Presented);
		Node.TargetPrimitive = *NewPrimitive;
	}

	const TArray<FInstancedStruct> OldPresented = Node.Modifiers;
	const TArray<FInstancedStruct> OldTarget = Node.TargetModifiers;
	TArray<FInstancedStruct> Presented;
	Presented.SetNum(NewModifiers.Num());
	for (int32 Slot = 0; Slot < NewModifiers.Num(); ++Slot)
	{
		if (OldPresented.IsValidIndex(Slot) && OldTarget.IsValidIndex(Slot))
		{
			ProcessSlot(Node, Slot, Presented[Slot], OldPresented[Slot], OldTarget[Slot], NewModifiers[Slot], Transaction);
		}
		else
		{
			CancelSlot(Node, Slot);
			Presented[Slot] = NewModifiers[Slot];
		}
	}
	for (int32 Slot = NewModifiers.Num(); Slot < OldPresented.Num(); ++Slot)
	{
		CancelSlot(Node, Slot);
	}
	Node.Modifiers = MoveTemp(Presented);
	Node.TargetModifiers = NewModifiers;
}

void FVekaImplicitAnimator::ProcessSlot(FVekaNode& Node, int32 Slot, FInstancedStruct& OutPresented, const FInstancedStruct& OldPresented,
	const FInstancedStruct& OldTarget, const FInstancedStruct& NewTarget, const FVekaAnimationSpec* Spec)
{
	OutPresented = NewTarget;
	const UScriptStruct* Type = NewTarget.GetScriptStruct();
	if (!Type || OldTarget.GetScriptStruct() != Type || OldPresented.GetScriptStruct() != Type)
	{
		CancelSlot(Node, Slot);
		return;
	}
	if (IsNonAnimatedSlot(Type))
	{
		return;
	}

	// 1) Animations already running on this slot keep their presented value.
	for (const TPair<FString, TSharedPtr<FLeaf>>& Pair : Leaves)
	{
		FLeaf& Leaf = *Pair.Value;
		if (Leaf.NodeRaw != &Node || Leaf.Slot != Slot || Leaf.Struct != Type)
		{
			continue;
		}
		if (uint8* Ptr = ResolvePath(OutPresented.GetMutableMemory(), Leaf.Path))
		{
			if (Leaf.Kind == ELeafKind::Color)
			{
				*reinterpret_cast<FLinearColor*>(Ptr) = Leaf.Color;
			}
			else
			{
				WriteScalar(Ptr, Leaf.Kind == ELeafKind::Float, Leaf.Scalar);
			}
		}
	}

	// 2) Changed targets animate (transaction) or snap.
	const bool bFrame = Type == FVekaFrameModifier::StaticStruct();
	TArray<FVekaImplicitAnimatorPath> WalkPath;
	auto ToSteps = [&WalkPath]()
	{
		TArray<FPathStep> Steps;
		Steps.Reserve(WalkPath.Num());
		for (const FVekaImplicitAnimatorPath& Step : WalkPath)
		{
			Steps.Add({ Step.Property, Step.Index });
		}
		return Steps;
	};

	auto Visit = [&](EKind Kind, const uint8* OldValue, const uint8* NewValue)
	{
		const bool bColor = Kind == EKind::Color;
		const bool bFloat = Kind == EKind::Float;
		const bool bEqual = bColor
			? *reinterpret_cast<const FLinearColor*>(OldValue) == *reinterpret_cast<const FLinearColor*>(NewValue)
			: ReadScalar(OldValue, bFloat) == ReadScalar(NewValue, bFloat);
		if (bEqual)
		{
			return;
		}

		const TArray<FPathStep> Steps = ToSteps();
		uint8* PresentedPtr = ResolvePath(OutPresented.GetMutableMemory(), Steps);
		const uint8* OldPresentedPtr = ResolvePath(const_cast<uint8*>(static_cast<const uint8*>(OldPresented.GetMemory())), Steps);
		if (!PresentedPtr)
		{
			return;
		}

		bool bAnimate = Spec != nullptr && OldPresentedPtr != nullptr;
		if (bAnimate && !bColor)
		{
			const double From = ReadScalar(OldPresentedPtr, bFloat);
			const double To = ReadScalar(NewValue, bFloat);
			// "Unset" (-1) and "infinite" sizes are not values to interpolate.
			bAnimate = !Veka::IsInfinite(FMath::Abs(From)) && !Veka::IsInfinite(FMath::Abs(To)) && !(bFrame && (From < 0.0 || To < 0.0));
		}

		const ELeafKind LeafKind = bColor ? ELeafKind::Color : (bFloat ? ELeafKind::Float : ELeafKind::Double);
		if (bAnimate)
		{
			StartLeaf(Node, Slot, Type, Steps, LeafKind, OldPresentedPtr, NewValue, *Spec);
			// Start drawing from where it was.
			if (bColor)
			{
				*reinterpret_cast<FLinearColor*>(PresentedPtr) = Leaves.FindChecked(MakeKey(Node, Slot, Steps))->Color;
			}
			else
			{
				WriteScalar(PresentedPtr, bFloat, Leaves.FindChecked(MakeKey(Node, Slot, Steps))->Scalar);
			}
		}
		else
		{
			CancelLeaf(MakeKey(Node, Slot, Steps));
			if (bColor)
			{
				*reinterpret_cast<FLinearColor*>(PresentedPtr) = *reinterpret_cast<const FLinearColor*>(NewValue);
			}
			else
			{
				WriteScalar(PresentedPtr, bFloat, ReadScalar(NewValue, bFloat));
			}
		}
	};

	auto Structural = [&]()
	{
		// An array changed size: its values snap.
		const TArray<FPathStep> Steps = ToSteps();
		CancelSlot(Node, Slot, &Steps);
		const FProperty* ArrayProperty = Steps.Last().Property;
		TArray<FPathStep> Container = Steps;
		Container.Pop();
		uint8* PresentedOwner = ResolvePath(OutPresented.GetMutableMemory(), Container);
		const uint8* TargetOwner = ResolvePath(const_cast<uint8*>(static_cast<const uint8*>(NewTarget.GetMemory())), Container);
		if (PresentedOwner && TargetOwner)
		{
			ArrayProperty->CopyCompleteValue(ArrayProperty->ContainerPtrToValuePtr<void>(PresentedOwner), ArrayProperty->ContainerPtrToValuePtr<void>(TargetOwner));
		}
	};

	WalkStruct(Type, static_cast<const uint8*>(OldTarget.GetMemory()), static_cast<const uint8*>(NewTarget.GetMemory()), WalkPath, Visit, Structural);
}

void FVekaImplicitAnimator::StartLeaf(FVekaNode& Node, int32 Slot, const UScriptStruct* Struct, const TArray<FPathStep>& Path, ELeafKind Kind,
	const uint8* From, const uint8* To, const FVekaAnimationSpec& Spec)
{
	const FString Key = MakeKey(Node, Slot, Path);
	TSharedPtr<FLeaf>& Slotted = Leaves.FindOrAdd(Key);
	const bool bExisting = Slotted.IsValid();
	if (!bExisting)
	{
		Slotted = MakeShared<FLeaf>();
		Slotted->Node = Node.AsShared();
		Slotted->NodeRaw = &Node;
		Slotted->Slot = Slot;
		Slotted->Struct = Struct;
		Slotted->Path = Path;
		Slotted->Kind = Kind;
		Slotted->Key = Key;
	}
	TSharedPtr<FLeaf> Leaf = Slotted;
	TWeakPtr<FLeaf> WeakLeaf = Leaf;
	FVekaAnimaScheduler& Scheduler = Runtime.GetScheduler();

	auto OnComplete = [this, Key, WeakLeaf]()
	{
		const TSharedPtr<FLeaf>* Found = Leaves.Find(Key);
		if (Found && *Found == WeakLeaf.Pin())
		{
			Leaves.Remove(Key);
		}
	};

	if (Kind == ELeafKind::Color)
	{
		// Colors interpolate in Oklab; a retarget restarts from the presented color.
		Leaf->FromColor = bExisting ? Leaf->Color : *reinterpret_cast<const FLinearColor*>(From);
		Leaf->ToColor = *reinterpret_cast<const FLinearColor*>(To);
		Leaf->Color = Leaf->FromColor;
		Scheduler.CancelChannel(Leaf.Get());
		Scheduler.Animate(Spec, 0.0, 1.0, [this, WeakLeaf](double T)
		{
			if (TSharedPtr<FLeaf> Pinned = WeakLeaf.Pin())
			{
				Pinned->Color = Veka::Colora::Lerp(Pinned->FromColor, Pinned->ToColor, static_cast<float>(T), EVekaColorSpace::OkLab);
				ApplyLeaf(*Pinned);
			}
		}, OnComplete, Leaf.Get());
	}
	else
	{
		const bool bFloat = Kind == ELeafKind::Float;
		if (!bExisting)
		{
			Leaf->Scalar = ReadScalar(From, bFloat);
		}
		// Same channel: the scheduler retargets from the presented value (springs keep their velocity).
		Scheduler.Animate(Spec, Leaf->Scalar, ReadScalar(To, bFloat), [this, WeakLeaf](double Value)
		{
			if (TSharedPtr<FLeaf> Pinned = WeakLeaf.Pin())
			{
				Pinned->Scalar = Value;
				ApplyLeaf(*Pinned);
			}
		}, OnComplete, Leaf.Get());
	}
	Runtime.RequestUpdate();
}

void FVekaImplicitAnimator::ApplyLeaf(FLeaf& Leaf)
{
	TSharedPtr<FVekaNode> Node = Leaf.Node.Pin();
	if (!Node.IsValid())
	{
		return;
	}
	FInstancedStruct* Slot = GetSlot(*Node, Leaf.Slot);
	if (!Slot || Slot->GetScriptStruct() != Leaf.Struct)
	{
		return;
	}
	if (uint8* Ptr = ResolvePath(Slot->GetMutableMemory(), Leaf.Path))
	{
		if (Leaf.Kind == ELeafKind::Color)
		{
			*reinterpret_cast<FLinearColor*>(Ptr) = Leaf.Color;
		}
		else
		{
			WriteScalar(Ptr, Leaf.Kind == ELeafKind::Float, Leaf.Scalar);
		}
		// Only an environment modifier (or the primitive, to be safe) can change what the subtree inherits, and only a
		// layout modifier (or the primitive) a size.
		const FVekaModifier* Modifier = Leaf.Slot >= 0 ? Slot->GetPtr<FVekaModifier>() : nullptr;
		Touch(*Node, Leaf.Slot < 0 || !Modifier || Modifier->AffectsLayout());
		if (Leaf.Slot < 0 || !Modifier || Modifier->ModifiesEnvironment())
		{
			TouchedEnvironment.Add(Node.Get());
		}
	}
}

void FVekaImplicitAnimator::CancelLeaf(const FString& Key)
{
	if (TSharedPtr<FLeaf> Leaf = Leaves.FindRef(Key))
	{
		Runtime.GetScheduler().CancelChannel(Leaf.Get());
		Leaves.Remove(Key);
	}
}

void FVekaImplicitAnimator::CancelSlot(const FVekaNode& Node, int32 Slot, const TArray<FPathStep>* Prefix)
{
	TArray<FString> ToCancel;
	for (const TPair<FString, TSharedPtr<FLeaf>>& Pair : Leaves)
	{
		const FLeaf& Leaf = *Pair.Value;
		if (Leaf.NodeRaw != &Node || Leaf.Slot != Slot)
		{
			continue;
		}
		if (Prefix)
		{
			bool bMatches = Leaf.Path.Num() >= Prefix->Num();
			for (int32 Index = 0; bMatches && Index < Prefix->Num(); ++Index)
			{
				// The prefix ends on the array itself (Index -1): any element below it matches.
				const FPathStep& P = (*Prefix)[Index];
				const FPathStep& L = Leaf.Path[Index];
				bMatches = P.Property == L.Property && (P.Index < 0 || P.Index == L.Index);
			}
			if (!bMatches)
			{
				continue;
			}
		}
		ToCancel.Add(Pair.Key);
	}
	for (const FString& Key : ToCancel)
	{
		CancelLeaf(Key);
	}
}

void FVekaImplicitAnimator::Touch(FVekaNode& Node, bool bLayout)
{
	if (bLayout)
	{
		TouchedLayout.Add(&Node);
	}
	bool bAlready = false;
	TouchedSet.Add(&Node, &bAlready);
	if (!bAlready)
	{
		Touched.Add(Node.AsShared());
	}
}

// --- Layout ------------------------------------------------------------------------------------------------

void FVekaImplicitAnimator::AnimateRect(FVekaNode& Node, const FVekaRect& Target, const FVekaAnimationSpec& Spec)
{
	TSharedPtr<FRectAnimation>& Animation = Rects.FindOrAdd(&Node);
	if (!Animation.IsValid())
	{
		Animation = MakeShared<FRectAnimation>();
		Animation->Node = Node.AsShared();
	}
	Animation->From = Node.ArrangedRect;
	Animation->To = Target;

	const FVekaNode* Raw = &Node;
	TWeakPtr<FRectAnimation> Weak = Animation;
	FVekaAnimaScheduler& Scheduler = Runtime.GetScheduler();
	Scheduler.CancelChannel(Animation.Get());
	Scheduler.Animate(Spec, 0.0, 1.0, [this, Weak](double T)
	{
		TSharedPtr<FRectAnimation> Pinned = Weak.Pin();
		TSharedPtr<FVekaNode> Target = Pinned.IsValid() ? Pinned->Node.Pin() : nullptr;
		if (Target.IsValid())
		{
			Target->ArrangedRect = FVekaRect(FMath::Lerp(Pinned->From.Position, Pinned->To.Position, T), FMath::Lerp(Pinned->From.Size, Pinned->To.Size, T));
			Touch(*Target);
		}
	}, [this, Raw, Weak]()
	{
		const TSharedPtr<FRectAnimation>* Found = Rects.Find(Raw);
		if (Found && *Found == Weak.Pin())
		{
			Rects.Remove(Raw);
		}
	}, Animation.Get());
	Runtime.RequestUpdate();
}

void FVekaImplicitAnimator::CancelRect(FVekaNode& Node)
{
	if (TSharedPtr<FRectAnimation> Animation = Rects.FindRef(&Node))
	{
		Runtime.GetScheduler().CancelChannel(Animation.Get());
		Rects.Remove(&Node);
	}
}

bool FVekaImplicitAnimator::IsRectAnimating(const FVekaNode& Node) const
{
	return Rects.Contains(&Node);
}

// --- Transitions -------------------------------------------------------------------------------------------

void FVekaImplicitAnimator::StartTransition(FVekaNode& Node, bool bInsertion, const FVekaAnimationSpec& Spec, TFunction<void()> OnDone)
{
	FVekaTransition Transition = FVekaTransition::Opacity();
	for (const FInstancedStruct& Modifier : Node.TargetModifiers)
	{
		if (const FVekaTransitionModifier* Typed = Modifier.GetPtr<FVekaTransitionModifier>())
		{
			Transition = Typed->Transition;
		}
	}
	Node.TransitionEffects = bInsertion ? Transition.Insertion : Transition.Removal;
	Node.bRemoving = !bInsertion;
	if (bInsertion && !Transitions.Contains(&Node))
	{
		Node.TransitionProgress = 0.f;
	}

	TSharedPtr<FTransitionAnimation>& Animation = Transitions.FindOrAdd(&Node);
	if (!Animation.IsValid())
	{
		Animation = MakeShared<FTransitionAnimation>();
		Animation->Node = Node.AsShared();
	}

	const FVekaNode* Raw = &Node;
	TWeakPtr<FTransitionAnimation> Weak = Animation;
	FVekaAnimaScheduler& Scheduler = Runtime.GetScheduler();
	Scheduler.CancelChannel(Animation.Get());
	Scheduler.Animate(Spec, Node.TransitionProgress, bInsertion ? 1.0 : 0.0, [this, Weak](double Progress)
	{
		TSharedPtr<FTransitionAnimation> Pinned = Weak.Pin();
		TSharedPtr<FVekaNode> Target = Pinned.IsValid() ? Pinned->Node.Pin() : nullptr;
		if (Target.IsValid())
		{
			Target->TransitionProgress = static_cast<float>(Progress);
			Touch(*Target);
		}
	}, [this, Raw, Weak, bInsertion, OnDone]()
	{
		const TSharedPtr<FTransitionAnimation>* Found = Transitions.Find(Raw);
		if (!Found || *Found != Weak.Pin())
		{
			return;
		}
		if (TSharedPtr<FVekaNode> Target = (*Found)->Node.Pin())
		{
			Target->TransitionProgress = bInsertion ? 1.f : 0.f;
			if (bInsertion)
			{
				Target->TransitionEffects.Reset();
			}
		}
		Transitions.Remove(Raw);
		if (!bInsertion && OnDone)
		{
			OnDone();
		}
	}, Animation.Get());
	Touch(Node);
	Runtime.RequestUpdate();
}

void FVekaImplicitAnimator::ForgetNode(const FVekaNode& Node)
{
	TArray<FString> ToCancel;
	for (const TPair<FString, TSharedPtr<FLeaf>>& Pair : Leaves)
	{
		if (Pair.Value->NodeRaw == &Node)
		{
			ToCancel.Add(Pair.Key);
		}
	}
	for (const FString& Key : ToCancel)
	{
		CancelLeaf(Key);
	}
	if (TSharedPtr<FRectAnimation> Rect = Rects.FindRef(&Node))
	{
		Runtime.GetScheduler().CancelChannel(Rect.Get());
		Rects.Remove(&Node);
	}
	if (TSharedPtr<FTransitionAnimation> Transition = Transitions.FindRef(&Node))
	{
		Runtime.GetScheduler().CancelChannel(Transition.Get());
		Transitions.Remove(&Node);
	}
}

void FVekaImplicitAnimator::FlushTouched()
{
	TArray<TWeakPtr<FVekaNode>> Nodes = MoveTemp(Touched);
	Touched.Reset();
	TouchedSet.Reset();
	const TSet<const FVekaNode*> Environments = MoveTemp(TouchedEnvironment);
	TouchedEnvironment.Reset();
	const TSet<const FVekaNode*> Layouts = MoveTemp(TouchedLayout);
	TouchedLayout.Reset();
	for (const TWeakPtr<FVekaNode>& Weak : Nodes)
	{
		if (TSharedPtr<FVekaNode> Node = Weak.Pin())
		{
			const bool bEnvironment = Environments.Contains(Node.Get());
			if (bEnvironment)
			{
				Node->RefreshEnvironment();
			}
			if (bEnvironment || Layouts.Contains(Node.Get()))
			{
				Node->InvalidateLayout();
				Node->SyncWidget();
			}
			else
			{
				Node->SyncPresentation();
			}
		}
	}
}
