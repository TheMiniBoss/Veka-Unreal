// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaRuntime.h"
#include "Core/VekaNode.h"
#include "Core/VekaView.h"
#include "VekaUILog.h"

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
{
}

FVekaRuntime::~FVekaRuntime()
{
	Scheduler.CancelAll();
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

	if (!Node.IsValid() || Node->Kind != Kind || Node->TypeName != TypeName)
	{
		Node = MakeShared<FVekaNode>(*this, Key, Kind, TypeName);
		Node->Primitive = Kind == EVekaElementKind::Primitive ? Element.GetPrimitive() : FInstancedStruct::Make(FVekaPrimitive());
		Node->PrimitiveState = Node->GetPrimitive().CreateState();
		Node->Modifiers = Element.GetModifiers();
		bChanged = true;
	}
	else
	{
		// Always take the latest values (callbacks are not compared); only UPROPERTY differences invalidate.
		if (Kind == EVekaElementKind::Primitive)
		{
			bChanged |= !(Node->Primitive == Element.GetPrimitive());
			Node->Primitive = Element.GetPrimitive();
		}
		bChanged |= !ModifiersEqual(Node->Modifiers, Element.GetModifiers());
		Node->Modifiers = Element.GetModifiers();
		bChanged |= Node->InheritedEnvironment != InheritedEnvironment;
	}

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
	Node.Environment = Node.InheritedEnvironment;
	Node.Gestures.Reset();
	FVekaLayoutTraits Traits;
	// Outermost modifier first: an inner `.Foreground(...)` overrides an outer one, like SwiftUI.
	for (int32 Index = Node.Modifiers.Num() - 1; Index >= 0; --Index)
	{
		if (const FVekaModifier* Typed = Node.Modifiers[Index].GetPtr<FVekaModifier>())
		{
			Typed->ModifyEnvironment(Node.Environment);
		}
	}
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

	TArray<TSharedRef<FVekaNode>> NewChildren;
	NewChildren.Reserve(Keyed.Num());
	for (const FKeyedElement& Entry : Keyed)
	{
		TSharedPtr<FVekaNode> Existing;
		OldByKey.RemoveAndCopyValue(Entry.Key, Existing);
		if (TSharedPtr<FVekaNode> Child = Reconcile(&Node, Existing, Entry.Element, Node.Environment, Entry.Key))
		{
			NewChildren.Add(Child.ToSharedRef());
		}
	}

	for (const TPair<FName, TSharedPtr<FVekaNode>>& Removed : OldByKey)
	{
		Removed.Value->Parent = nullptr;
	}

	bool bChanged = NewChildren.Num() != Node.Children.Num();
	for (int32 Index = 0; !bChanged && Index < NewChildren.Num(); ++Index)
	{
		bChanged = NewChildren[Index] != Node.Children[Index];
	}

	Node.Children = MoveTemp(NewChildren);
	return bChanged;
}

void FVekaRuntime::MarkNodeDirty(FVekaNode& Node)
{
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
				if (Node->bDirty && IsAttachedTo(*Node, Root.Get()))
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
	Flush();
	return HasPendingWork();
}

bool FVekaRuntime::HasPendingWork() const
{
	return DirtyNodes.Num() > 0 || Scheduler.IsAnimating();
}

void FVekaRuntime::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (Root.IsValid())
	{
		Root->AddReferencedObjects(Collector);
	}
}
