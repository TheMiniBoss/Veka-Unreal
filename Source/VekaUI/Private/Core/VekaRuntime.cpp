// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaRuntime.h"
#include "Core/VekaNode.h"
#include "Core/VekaView.h"
#include "VekaUILog.h"
#include "Anima/VekaImplicit.h"
#include "Anima/VekaTimeline.h"
#include "Modifiers/VekaAnimaModifiers.h"

namespace
{
	struct FKeyedElement
	{
		FName Key;
		FVekaElement Element;
	};

	/** Assigns sibling keys and flattens fragments. Slot index is part of the key so If() keeps identity. */
	void FlattenChildren(const TArray<FVekaElement>& Elements, const FString& Prefix, TArray<FKeyedElement>& Out, TSet<FName>& UsedKeys)
	{
		for (int32 Index = 0; Index < Elements.Num(); ++Index)
		{
			const FVekaElement& Element = Elements[Index];
			if (Element.IsEmpty())
			{
				continue;
			}

			const FName Id = Element.GetId();
			const FString Base = Id.IsNone() ? FString::Printf(TEXT("#%d"), Index) : Id.ToString();
			const FString KeyString = Prefix.IsEmpty() ? Base : Prefix + TEXT("/") + Base;

			if (Element.GetKind() == EVekaElementKind::Fragment)
			{
				FlattenChildren(Element.GetChildren(), KeyString, Out, UsedKeys);
				continue;
			}

			FName Key(*KeyString);
			if (UsedKeys.Contains(Key))
			{
				UE_LOG(LogVekaUI, Warning, TEXT("Duplicate Veka id '%s' among siblings; identity falls back to position."), *KeyString);
				Key = FName(*FString::Printf(TEXT("%s~%d"), *KeyString, Index));
			}
			UsedKeys.Add(Key);
			Out.Add({ Key, Element });
		}
	}

	bool ModifiersEqual(const TArray<FInstancedStruct>& A, const TArray<FInstancedStruct>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!(A[Index] == B[Index]))
			{
				return false;
			}
		}
		return true;
	}

	const FVekaAnimationModifier* FindAnimationModifier(const TArray<FInstancedStruct>& Modifiers)
	{
		for (const FInstancedStruct& Modifier : Modifiers)
		{
			if (const FVekaAnimationModifier* Typed = Modifier.GetPtr<FVekaAnimationModifier>())
			{
				return Typed;
			}
		}
		return nullptr;
	}

	bool IsAttachedTo(const FVekaNode& Node, const FVekaNode* Root)
	{
		const FVekaNode* Current = &Node;
		while (Current->GetParent())
		{
			Current = Current->GetParent();
		}
		return Current == Root;
	}
}

FVekaRuntime::FVekaRuntime()
	: RootEnvironment(VekaEnv::MakeDefault())
	, Implicit(MakeUnique<FVekaImplicitAnimator>(*this))
{
}

FVekaRuntime::~FVekaRuntime()
{
	// Scheduler callbacks reference the implicit animator: stop them first.
	Scheduler.CancelAll();
}

const FVekaAnimationSpec* FVekaRuntime::GetLayoutTransaction() const
{
	// Layout happens in the paint that follows the flush (same or next frame).
	return LayoutTransaction.IsSet() && GFrameCounter <= LayoutTransactionFrame + 1 ? &LayoutTransaction.GetValue() : nullptr;
}

void FVekaRuntime::BeginLayoutTransaction(const FVekaAnimationSpec& Spec)
{
	LayoutTransaction = Spec;
	LayoutTransactionFrame = GFrameCounter;
}

void FVekaRuntime::RegisterTimeline(const TSharedRef<FVekaTimelinePlayer>& Player)
{
	const TWeakPtr<FVekaTimelinePlayer> Weak = Player;
	if (!Timelines.Contains(Weak))
	{
		Timelines.Add(Weak);
	}
	RequestUpdate();
}

void FVekaRuntime::TickTimelines(double DeltaSeconds)
{
	for (int32 Index = Timelines.Num() - 1; Index >= 0; --Index)
	{
		TSharedPtr<FVekaTimelinePlayer> Player = Timelines[Index].Pin();
		if (!Player.IsValid() || !Player->IsPlaying())
		{
			Timelines.RemoveAt(Index);
			continue;
		}
		Player->Advance(DeltaSeconds);
	}
}

void FVekaRuntime::SetRoot(const FVekaElement& InRoot)
{
	RootElement = InRoot;
	const TSharedPtr<FVekaNode> Previous = Root;
	Root = Reconcile(nullptr, Root, RootElement, RootEnvironment, TEXT("root"));
	if (Root != Previous)
	{
		OnRootChanged.Broadcast();
	}
}

void FVekaRuntime::SetEnvironment(const FVekaEnvironment& Environment)
{
	RootEnvironment = Environment;
	SetRoot(RootElement);
}

TSharedPtr<FVekaNode> FVekaRuntime::Reconcile(FVekaNode* Parent, const TSharedPtr<FVekaNode>& Existing, const FVekaElement& Element,
	const FVekaEnvironment& InheritedEnvironment, FName Key)
{
	if (Element.IsEmpty() || Element.GetKind() == EVekaElementKind::Fragment)
	{
		return nullptr;
	}

	const EVekaElementKind Kind = Element.GetKind();
	const FName TypeName = Element.GetTypeName();

	TSharedPtr<FVekaNode> Node = Existing;
	bool bChanged = false;
	TOptional<FVekaAnimationSpec> NodeTransaction = CurrentTransaction;

	if (!Node.IsValid() || Node->Kind != Kind || Node->TypeName != TypeName)
	{
		Node = MakeShared<FVekaNode>(*this, Key, Kind, TypeName);
		Node->Primitive = Kind == EVekaElementKind::Primitive ? Element.GetPrimitive() : FInstancedStruct::Make(FVekaPrimitive());
		Node->TargetPrimitive = Node->Primitive;
		Node->PrimitiveState = Node->GetPrimitive().CreateState();
		Node->Modifiers = Element.GetModifiers();
		Node->TargetModifiers = Node->Modifiers;
		bChanged = true;
	}
	else
	{
		// Compare with the last targets (presented values may be animating); callbacks are not compared.
		const FInstancedStruct* NewPrimitive = Kind == EVekaElementKind::Primitive ? &Element.GetPrimitive() : nullptr;
		if (NewPrimitive)
		{
			bChanged |= !(Node->TargetPrimitive == *NewPrimitive);
		}
		bChanged |= !ModifiersEqual(Node->TargetModifiers, Element.GetModifiers());
		bChanged |= Node->InheritedEnvironment != InheritedEnvironment;

		// `.Animation(spec, value)`: a new value opens a transaction for this node and its subtree.
		const FVekaAnimationModifier* NewAnimation = FindAnimationModifier(Element.GetModifiers());
		const FVekaAnimationModifier* OldAnimation = FindAnimationModifier(Node->TargetModifiers);
		if (NewAnimation && OldAnimation && NewAnimation->ValueKey != OldAnimation->ValueKey)
		{
			NodeTransaction = NewAnimation->Spec;
			BeginLayoutTransaction(NewAnimation->Spec);
		}

		Implicit->UpdateNode(*Node, NewPrimitive, Element.GetModifiers(), NodeTransaction.IsSet() ? &NodeTransaction.GetValue() : nullptr);
	}
	TGuardValue<TOptional<FVekaAnimationSpec>> TransactionGuard(CurrentTransaction, NodeTransaction);

	Node->Key = Key;
	Node->Parent = Parent;
	Node->View = Element.GetView();
	Node->SourceElement = Element;
	Node->InheritedEnvironment = InheritedEnvironment;

	EvaluateNode(*Node, bChanged);
	return Node;
}

void FVekaRuntime::EvaluateNode(FVekaNode& Node, bool bDescriptionChanged)
{
	// Environment, gestures and layout traits from this node's modifiers.
	Node.Environment = Node.ComputeEnvironment();
	Node.Gestures.Reset();
	FVekaLayoutTraits Traits;
	for (const FInstancedStruct& Modifier : Node.Modifiers)
	{
		if (const FVekaModifier* Typed = Modifier.GetPtr<FVekaModifier>())
		{
			Typed->CollectGestures(Node.Gestures);
			Typed->ModifyLayoutTraits(Traits);
		}
	}
	Node.LayoutPriority = Traits.LayoutPriority;

	// Clear before running Body so state set during Body schedules a new pass instead of being lost.
	Node.bDirty = false;

	TArray<FVekaElement> ChildElements;
	if (Node.Kind == EVekaElementKind::View)
	{
		if (Node.View.IsValid())
		{
			FVekaContext Context(Node, Node.Environment);
			ChildElements.Add(Node.View->Body(Context).Id(TEXT("body")));
		}
	}
	else if (Node.GetPrimitive().AcceptsChildren())
	{
		ChildElements = Node.SourceElement.GetChildren();
	}

	const bool bChildrenChanged = ReconcileChildren(Node, ChildElements);
	Node.bBuilt = true;

	if (bDescriptionChanged || bChildrenChanged)
	{
		Node.InvalidateLayout();
	}
	Node.SyncWidget();
}

bool FVekaRuntime::ReconcileChildren(FVekaNode& Node, const TArray<FVekaElement>& Elements)
{
	TArray<FKeyedElement> Keyed;
	TSet<FName> UsedKeys;
	FlattenChildren(Elements, FString(), Keyed, UsedKeys);

	TMap<FName, TSharedPtr<FVekaNode>> OldByKey;
	OldByKey.Reserve(Node.Children.Num());
	for (const TSharedRef<FVekaNode>& Child : Node.Children)
	{
		OldByKey.Add(Child->Key, Child);
	}

	const FVekaAnimationSpec* Transaction = CurrentTransaction.IsSet() ? &CurrentTransaction.GetValue() : nullptr;
	const int32 GhostCountBefore = Node.RemovedChildren.Num();

	TArray<TSharedRef<FVekaNode>> NewChildren;
	NewChildren.Reserve(Keyed.Num());
	for (const FKeyedElement& Entry : Keyed)
	{
		TSharedPtr<FVekaNode> Existing;
		OldByKey.RemoveAndCopyValue(Entry.Key, Existing);

		bool bRevived = false;
		if (!Existing.IsValid())
		{
			// Coming back while its removal transition plays: reuse it (and its state).
			const int32 GhostIndex = Node.RemovedChildren.IndexOfByPredicate([&Entry](const TSharedRef<FVekaNode>& Ghost) { return Ghost->Key == Entry.Key; });
			if (GhostIndex != INDEX_NONE)
			{
				Existing = Node.RemovedChildren[GhostIndex];
				Node.RemovedChildren.RemoveAt(GhostIndex);
				Existing->bRemoving = false;
				bRevived = true;
			}
		}

		const bool bInserted = !Existing.IsValid();
		if (TSharedPtr<FVekaNode> Child = Reconcile(&Node, Existing, Entry.Element, Node.Environment, Entry.Key))
		{
			if ((bInserted && Transaction && Node.bBuilt) || bRevived)
			{
				Implicit->StartTransition(*Child, true, Transaction ? *Transaction : FVekaAnimationSpec::Smooth());
			}
			NewChildren.Add(Child.ToSharedRef());
		}
	}

	for (const TPair<FName, TSharedPtr<FVekaNode>>& Removed : OldByKey)
	{
		const TSharedRef<FVekaNode> Child = Removed.Value.ToSharedRef();
		if (Transaction)
		{
			// Keep drawing it at its last rect while it leaves.
			Node.RemovedChildren.Add(Child);
			TWeakPtr<FVekaNode> WeakParent = Node.AsShared();
			TWeakPtr<FVekaNode> WeakChild = Child;
			Implicit->StartTransition(*Child, false, *Transaction, [this, WeakParent, WeakChild]()
			{
				TSharedPtr<FVekaNode> Parent = WeakParent.Pin();
				TSharedPtr<FVekaNode> Gone = WeakChild.Pin();
				if (!Parent.IsValid() || !Gone.IsValid())
				{
					return;
				}
				if (Parent->RemovedChildren.Remove(Gone.ToSharedRef()) > 0)
				{
					Gone->Parent = nullptr;
					Implicit->ForgetNode(*Gone);
					Parent->InvalidateLayout();
					Parent->SyncWidget();
				}
			});
		}
		else
		{
			Implicit->ForgetNode(*Child);
			Child->Parent = nullptr;
		}
	}

	bool bChanged = NewChildren.Num() != Node.Children.Num() || Node.RemovedChildren.Num() != GhostCountBefore;
	for (int32 Index = 0; !bChanged && Index < NewChildren.Num(); ++Index)
	{
		bChanged = NewChildren[Index] != Node.Children[Index];
	}

	Node.Children = MoveTemp(NewChildren);
	return bChanged;
}

void FVekaRuntime::MarkNodeDirty(FVekaNode& Node)
{
	if (const FVekaAnimationSpec* Transaction = Veka::Anima::GetActiveTransaction())
	{
		Node.PendingTransaction = *Transaction;
	}
	if (Node.bDirty)
	{
		return;
	}
	Node.bDirty = true;
	DirtyNodes.Add(Node.AsShared());
	RequestUpdate();
}

void FVekaRuntime::RequestUpdate()
{
	if (OnUpdateRequested)
	{
		OnUpdateRequested();
	}
}

bool FVekaRuntime::Flush()
{
	if (bFlushing)
	{
		return false;
	}
	TGuardValue<bool> Guard(bFlushing, true);

	bool bAnyEvaluated = false;
	int32 Pass = 0;
	while (DirtyNodes.Num() > 0)
	{
		if (++Pass > 16)
		{
			UE_LOG(LogVekaUI, Warning, TEXT("VekaUI: state keeps changing during evaluation (16 passes). Remaining updates postponed to next frame."));
			break;
		}

		TArray<TSharedPtr<FVekaNode>> Batch;
		for (const TWeakPtr<FVekaNode>& Weak : DirtyNodes)
		{
			if (TSharedPtr<FVekaNode> Node = Weak.Pin())
			{
				if (Node->bDirty && IsAttachedTo(*Node, Root.Get()) && !Node->IsInRemovedSubtree())
				{
					Batch.Add(Node);
				}
				else
				{
					Node->bDirty = false;
				}
			}
		}
		DirtyNodes.Reset();

		// Parents first: re-evaluating a parent re-evaluates its child views and clears their dirty flag.
		Batch.Sort([](const TSharedPtr<FVekaNode>& A, const TSharedPtr<FVekaNode>& B) { return A->GetDepth() < B->GetDepth(); });

		for (const TSharedPtr<FVekaNode>& Node : Batch)
		{
			if (Node->bDirty)
			{
				TOptional<FVekaAnimationSpec> Transaction = MoveTemp(Node->PendingTransaction);
				Node->PendingTransaction.Reset();
				if (Transaction.IsSet())
				{
					BeginLayoutTransaction(Transaction.GetValue());
				}
				TGuardValue<TOptional<FVekaAnimationSpec>> TransactionGuard(CurrentTransaction, Transaction);
				EvaluateNode(*Node, false);
				bAnyEvaluated = true;
			}
		}
	}
	return bAnyEvaluated;
}

bool FVekaRuntime::Tick(double DeltaSeconds)
{
	Flush();
	Scheduler.Tick(DeltaSeconds);
	Implicit->FlushTouched();
	TickTimelines(DeltaSeconds);
	Flush();
	return HasPendingWork();
}

bool FVekaRuntime::HasPendingWork() const
{
	bool bTimelinePlaying = false;
	for (const TWeakPtr<FVekaTimelinePlayer>& Weak : Timelines)
	{
		const TSharedPtr<FVekaTimelinePlayer> Player = Weak.Pin();
		bTimelinePlaying |= Player.IsValid() && Player->IsPlaying();
	}
	return DirtyNodes.Num() > 0 || Scheduler.IsAnimating() || bTimelinePlaying;
}

void FVekaRuntime::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (Root.IsValid())
	{
		Root->AddReferencedObjects(Collector);
	}
}
