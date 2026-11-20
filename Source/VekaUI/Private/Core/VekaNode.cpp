// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaNode.h"
#include "Misc/ScopeExit.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaView.h"
#include "Anima/VekaImplicit.h"
#include "Slate/SVekaNodeWidget.h"
#include "UObject/GCObject.h"

namespace
{
	const FVekaPrimitive& DefaultPrimitive()
	{
		static const FVekaPrimitive Default;
		return Default;
	}

	FVector2D Sanitize(const FVector2D& Size)
	{
		auto Fix = [](double Value)
		{
			if (!FMath::IsFinite(Value) || Value < 0.0)
			{
				return 0.0;
			}
			return FMath::Min(Value, Veka::Infinity);
		};
		return FVector2D(Fix(Size.X), Fix(Size.Y));
	}
}

FVekaNode::FVekaNode(FVekaRuntime& InRuntime, FName InKey, EVekaElementKind InKind, FName InTypeName)
	: Runtime(InRuntime)
	, Key(InKey)
	, Kind(InKind)
	, TypeName(InTypeName)
{
}

FVekaNode::~FVekaNode() = default;

const FVekaPrimitive& FVekaNode::GetPrimitive() const
{
	const FVekaPrimitive* Typed = Primitive.GetPtr<FVekaPrimitive>();
	return Typed ? *Typed : DefaultPrimitive();
}

int32 FVekaNode::GetDepth() const
{
	int32 Depth = 0;
	for (const FVekaNode* Node = Parent; Node; Node = Node->Parent)
	{
		++Depth;
	}
	return Depth;
}

// --- Layout -------------------------------------------------------------------------------------

/** Proposals remembered per node. Nested stacks ask each child for more than 8 (0, infinity, ideal, shares...). */
static constexpr int32 MeasureCacheCapacity = 32;

FVector2D FVekaNode::Measure(const FVekaProposal& Proposal)
{
	for (const TPair<FVekaProposal, FVector2D>& Entry : MeasureCache)
	{
		if (Entry.Key == Proposal)
		{
			return Entry.Value;
		}
	}

	const FVector2D Size = Sanitize(MeasureLevel(Modifiers.Num() - 1, Proposal));

	if (MeasureCache.Num() >= MeasureCacheCapacity)
	{
		MeasureCache.RemoveAt(0);
	}
	MeasureCache.Emplace(Proposal, Size);
	return Size;
}

FVector2D FVekaNode::MeasureLevel(int32 Level, const FVekaProposal& Proposal)
{
	if (Level < 0)
	{
		return MeasureContent(Proposal);
	}

	const FVekaModifier* Modifier = Modifiers[Level].GetPtr<FVekaModifier>();
	if (!Modifier)
	{
		return MeasureLevel(Level - 1, Proposal);
	}
	return Modifier->Measure(Proposal, [this, Level](const FVekaProposal& Inner) { return MeasureLevel(Level - 1, Inner); });
}

FVector2D FVekaNode::MeasureContent(const FVekaProposal& Proposal)
{
	const TArray<FVekaLayoutChild> Proxies = MakeChildProxies();
	FVekaLayoutContext Context{ *this, Environment, PrimitiveState.Get(), GetParentStackAxis() };
	const FVector2D Size = GetPrimitive().SizeThatFits(Proposal, Context, Proxies);

	// Remember which proposal gave which size (most recent last) for FindContentProposal.
	ContentMeasureCache.RemoveAll([&Proposal](const TPair<FVekaProposal, FVector2D>& Entry) { return Entry.Key == Proposal; });
	if (ContentMeasureCache.Num() >= MeasureCacheCapacity)
	{
		ContentMeasureCache.RemoveAt(0);
	}
	ContentMeasureCache.Emplace(Proposal, Size);
	return Size;
}

TOptional<FVekaProposal> FVekaNode::FindContentProposal(const FVector2D& Size) const
{
	// Several proposals can give the same size (a one-word label measures the same at 0 and at infinity).
	// Replaying a degenerate one (0) would squeeze the children: prefer a proposal that contains the size.
	auto Contains = [&Size](const FVekaProposal& Proposal)
	{
		return (!Proposal.Width.IsSet() || Proposal.Width.GetValue() >= Size.X - 0.5)
			&& (!Proposal.Height.IsSet() || Proposal.Height.GetValue() >= Size.Y - 0.5);
	};
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 Index = ContentMeasureCache.Num() - 1; Index >= 0; --Index)
		{
			const TPair<FVekaProposal, FVector2D>& Entry = ContentMeasureCache[Index];
			if (Entry.Value.Equals(Size, 0.5) && (Pass == 1 || Contains(Entry.Key)))
			{
				return Entry.Key;
			}
		}
	}
	return TOptional<FVekaProposal>();
}

void FVekaNode::Arrange(const FVector2D& Size)
{
	ModifierRects.SetNum(Modifiers.Num());

	// While this node's rect animates (layout transition), backgrounds and borders follow the animated size but
	// the content keeps the final layout: children are never laid out at an intermediate size they were not
	// measured for (that squeezed siblings and re-wrapped text), they animate their own rects.
	const bool bRectAnimating = bPlaced && Runtime.GetImplicit().IsRectAnimating(*this);
	const FVector2D LayoutSize = bRectAnimating ? LayoutTarget.Size : Size;

	FVekaRect Rect(FVector2D::ZeroVector, Sanitize(Size));
	FVekaRect LayoutRect(FVector2D::ZeroVector, Sanitize(LayoutSize));
	for (int32 Level = Modifiers.Num() - 1; Level >= 0; --Level)
	{
		ModifierRects[Level] = Rect;
		if (const FVekaModifier* Modifier = Modifiers[Level].GetPtr<FVekaModifier>())
		{
			auto Inner = [this, Level](const FVekaProposal& Proposal) { return MeasureLevel(Level - 1, Proposal); };
			Rect = Modifier->Place(Rect, Inner);
			if (bRectAnimating)
			{
				LayoutRect = Modifier->Place(LayoutRect, Inner);
			}
		}
	}
	ContentRect = bRectAnimating ? LayoutRect : Rect;

	const TArray<FVekaLayoutChild> Proxies = MakeChildProxies();
	FVekaLayoutContext Context{ *this, Environment, PrimitiveState.Get(), GetParentStackAxis() };
	GetPrimitive().PlaceChildren(ContentRect, Context, Proxies);
}

void FVekaNode::ArrangeRecursive(const FVector2D& Size)
{
	Arrange(Size);
	for (const TSharedRef<FVekaNode>& Child : Children)
	{
		Child->ArrangeRecursive(Child->ArrangedRect.Size);
	}
}

TArray<FVekaLayoutChild> FVekaNode::MakeChildProxies() const
{
	TArray<FVekaLayoutChild> Proxies;
	Proxies.Reserve(Children.Num());
	for (const TSharedRef<FVekaNode>& Child : Children)
	{
		Proxies.Emplace(Child.Get());
	}
	return Proxies;
}

TOptional<EVekaAxis> FVekaNode::GetParentStackAxis() const
{
	return Parent ? Parent->GetPrimitive().GetStackAxis() : TOptional<EVekaAxis>();
}

bool FVekaNode::HitTest(const FVector2D& LocalPoint) const
{
	if (LocalPoint.X < 0.0 || LocalPoint.Y < 0.0 || LocalPoint.X > ArrangedRect.Size.X || LocalPoint.Y > ArrangedRect.Size.Y)
	{
		return false;
	}
	if (!ContentRect.Contains(LocalPoint))
	{
		// Inside a modifier area (padding, background): counts as a hit, like SwiftUI's contentShape of a background.
		return Modifiers.Num() > 0;
	}
	return GetPrimitive().HitTest(LocalPoint - ContentRect.Position, ContentRect.Size);
}

bool FVekaNode::IsInRemovedSubtree() const
{
	for (const FVekaNode* Node = this; Node; Node = Node->Parent)
	{
		if (Node->bRemoving)
		{
			return true;
		}
	}
	return false;
}

float FVekaNode::GetTransitionOpacity() const
{
	return TransitionEffects.IsEmpty() ? 1.f : FVekaTransition::EvaluateOpacity(TransitionEffects, TransitionProgress);
}

FVekaEnvironment FVekaNode::ComputeEnvironment() const
{
	// Outermost modifier first: an inner `.Foreground(...)` overrides an outer one, like SwiftUI.
	FVekaEnvironment Result = InheritedEnvironment;
	for (int32 Index = Modifiers.Num() - 1; Index >= 0; --Index)
	{
		if (const FVekaModifier* Typed = Modifiers[Index].GetPtr<FVekaModifier>())
		{
			Typed->ModifyEnvironment(Result);
		}
	}
	return Result;
}

void FVekaNode::RefreshEnvironment()
{
	FVekaEnvironment NewEnvironment = ComputeEnvironment();
	if (NewEnvironment == Environment)
	{
		return;
	}
	Environment = MoveTemp(NewEnvironment);
	// What the content measures may depend on the environment (font, text scale...).
	MeasureCache.Reset();
	ContentMeasureCache.Reset();
	if (Widget.IsValid())
	{
		Widget->Invalidate(EInvalidateWidgetReason::Layout);
	}
	auto Push = [this](const TSharedRef<FVekaNode>& Child)
	{
		Child->InheritedEnvironment = Environment;
		Child->RefreshEnvironment();
	};
	for (const TSharedRef<FVekaNode>& Child : Children)
	{
		Push(Child);
	}
	for (const TSharedRef<FVekaNode>& Child : RemovedChildren)
	{
		Push(Child);
	}
}

FVector2D FVekaNode::GetRootPosition() const
{
	FVector2D Position = FVector2D::ZeroVector;
	for (const FVekaNode* Node = this; Node; Node = Node->Parent)
	{
		Position += Node->ArrangedRect.Position;
	}
	return Position;
}

void FVekaNode::RecordMatchedGeometry()
{
	if (!MatchedKey.IsNone() && !IsInRemovedSubtree())
	{
		const FVector2D ParentOrigin = Parent ? Parent->GetRootPosition() : FVector2D::ZeroVector;
		Runtime.RecordMatchedGeometry(MatchedKey, *this, FVekaRect(ParentOrigin + ArrangedRect.Position, ArrangedRect.Size));
	}
}

void FVekaNode::PlaceAt(const FVekaRect& Rect)
{
	if (!bPlaced)
	{
		bPlaced = true;
		LayoutTarget = Rect;
		ArrangedRect = Rect;

		// Matched geometry: a view with this id was elsewhere (it is leaving, or left this frame): start from its
		// frame, and send the leaving one to this frame.
		const FVekaAnimationSpec* Spec = Runtime.GetLayoutTransaction();
		const FVekaRuntime::FMatchedGeometry* Matched = MatchedKey.IsNone() ? nullptr : Runtime.FindMatchedGeometry(MatchedKey);
		const TSharedPtr<FVekaNode> Other = Matched ? Matched->Node.Pin() : nullptr;
		if (Spec && Matched && Other.Get() != this)
		{
			const FVector2D ParentOrigin = Parent ? Parent->GetRootPosition() : FVector2D::ZeroVector;
			ArrangedRect = FVekaRect(Matched->RootRect.Position - ParentOrigin, Matched->RootRect.Size);
			Runtime.GetImplicit().AnimateRect(*this, Rect, *Spec);
			if (Other.IsValid() && Other->IsInRemovedSubtree())
			{
				const FVector2D OtherParentOrigin = Other->Parent ? Other->Parent->GetRootPosition() : FVector2D::ZeroVector;
				Runtime.GetImplicit().AnimateRect(*Other, FVekaRect(ParentOrigin + Rect.Position - OtherParentOrigin, Rect.Size), *Spec);
			}
		}
		RecordMatchedGeometry();
		if (!TransitionEffects.IsEmpty())
		{
			// Move transitions depend on the size, known only now.
			SyncWidget();
		}
		return;
	}
	ON_SCOPE_EXIT { RecordMatchedGeometry(); };

	FVekaImplicitAnimator& Implicit = Runtime.GetImplicit();
	if (Rect.Equals(LayoutTarget, 0.01))
	{
		if (!Implicit.IsRectAnimating(*this))
		{
			ArrangedRect = Rect;
		}
		return;
	}

	LayoutTarget = Rect;
	if (const FVekaAnimationSpec* Spec = Runtime.GetLayoutTransaction())
	{
		Implicit.AnimateRect(*this, Rect, *Spec);
	}
	else
	{
		Implicit.CancelRect(*this);
		ArrangedRect = Rect;
	}
}

bool FVekaNode::HasRenderTransform() const
{
	if (!TransitionEffects.IsEmpty() && TransitionProgress < 1.f)
	{
		return true;
	}
	for (const FInstancedStruct& Modifier : Modifiers)
	{
		if (const FVekaModifier* Typed = Modifier.GetPtr<FVekaModifier>())
		{
			if (Typed->HasRenderTransform())
			{
				return true;
			}
		}
	}
	return false;
}

FSlateRenderTransform FVekaNode::ComputeRenderTransform(const FVector2D& Size) const
{
	FSlateRenderTransform Transform;
	for (const FInstancedStruct& Modifier : Modifiers)
	{
		if (const FVekaModifier* Typed = Modifier.GetPtr<FVekaModifier>())
		{
			if (Typed->HasRenderTransform())
			{
				Typed->ModifyRenderTransform(Transform, Size);
			}
		}
	}
	if (!TransitionEffects.IsEmpty() && TransitionProgress < 1.f)
	{
		Transform = Concatenate(Transform, FVekaTransition::EvaluateTransform(TransitionEffects, TransitionProgress, Size));
	}
	return Transform;
}

bool FVekaNode::WantsInput() const
{
	return !Gestures.IsEmpty() || GetPrimitive().WantsInput();
}

// --- Invalidation -------------------------------------------------------------------------------

void FVekaNode::MarkDirty()
{
	Runtime.MarkNodeDirty(*this);
}

void FVekaNode::InvalidateLayout()
{
	// Re-measure this node with the proposals its parent used. If every size is unchanged, the parent's layout still
	// holds and only this node re-arranges: a hover or a color animation used to clear every ancestor's cache, and
	// re-measuring from the root cost ~90 000 measures (70 ms) in the Playground.
	const auto Previous = MeasureCache;
	MeasureCache.Reset();
	ContentMeasureCache.Reset();
	if (Widget.IsValid())
	{
		Widget->Invalidate(EInvalidateWidgetReason::Layout);
	}
	if (!Parent)
	{
		return;
	}
	// Never measured, or entries may have been evicted: the parent cannot be trusted, propagate.
	bool bSizeChanged = Previous.IsEmpty() || Previous.Num() >= MeasureCacheCapacity;
	for (int32 Index = 0; !bSizeChanged && Index < Previous.Num(); ++Index)
	{
		bSizeChanged = !Measure(Previous[Index].Key).Equals(Previous[Index].Value, 0.01);
	}
	if (bSizeChanged)
	{
		Parent->InvalidateLayout();
	}
}

void FVekaNode::InvalidatePaint()
{
	if (Widget.IsValid())
	{
		Widget->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

// --- Slate --------------------------------------------------------------------------------------

TSharedRef<SWidget> FVekaNode::GetOrCreateWidget()
{
	if (!Widget.IsValid())
	{
		SAssignNew(Widget, SVekaNodeWidget, AsShared());
	}
	return Widget.ToSharedRef();
}

void FVekaNode::SyncWidget()
{
	if (Widget.IsValid())
	{
		Widget->SyncFromNode();
	}
}

void FVekaNode::AddReferencedObjects(FReferenceCollector& Collector)
{
	Primitive.AddStructReferencedObjects(Collector);
	TargetPrimitive.AddStructReferencedObjects(Collector);
	for (FInstancedStruct& Modifier : Modifiers)
	{
		Modifier.AddStructReferencedObjects(Collector);
	}
	for (FInstancedStruct& Modifier : TargetModifiers)
	{
		Modifier.AddStructReferencedObjects(Collector);
	}
	for (const TSharedRef<FVekaNode>& Child : RemovedChildren)
	{
		Child->AddReferencedObjects(Collector);
	}
	if (View.IsValid())
	{
		ConstCastSharedPtr<FVekaView>(View)->AddReferencedObjects(Collector);
	}
	for (const TSharedRef<FVekaNode>& Child : Children)
	{
		Child->AddReferencedObjects(Collector);
	}
}
