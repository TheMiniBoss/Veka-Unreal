// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaNode.h"
#include "Core/VekaRuntime.h"
#include "Core/VekaView.h"
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

	if (MeasureCache.Num() >= 8)
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
	return GetPrimitive().SizeThatFits(Proposal, Context, Proxies);
}

void FVekaNode::Arrange(const FVector2D& Size)
{
	ModifierRects.SetNum(Modifiers.Num());

	FVekaRect Rect(FVector2D::ZeroVector, Sanitize(Size));
	for (int32 Level = Modifiers.Num() - 1; Level >= 0; --Level)
	{
		ModifierRects[Level] = Rect;
		if (const FVekaModifier* Modifier = Modifiers[Level].GetPtr<FVekaModifier>())
		{
			Rect = Modifier->Place(Rect, [this, Level](const FVekaProposal& Inner) { return MeasureLevel(Level - 1, Inner); });
		}
	}
	ContentRect = Rect;

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

bool FVekaNode::HasRenderTransform() const
{
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
	for (FVekaNode* Node = this; Node; Node = Node->Parent)
	{
		Node->MeasureCache.Reset();
		if (Node->Widget.IsValid())
		{
			Node->Widget->Invalidate(EInvalidateWidgetReason::Layout);
		}
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
	for (FInstancedStruct& Modifier : Modifiers)
	{
		Modifier.AddStructReferencedObjects(Collector);
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
