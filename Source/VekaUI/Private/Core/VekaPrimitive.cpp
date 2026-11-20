// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaPrimitive.h"
#include "Core/VekaNode.h"
#include "Colora/VekaPaintRenderer.h"

FVector2D FVekaLayoutChild::SizeThatFits(const FVekaProposal& Proposal) const
{
	return Node->Measure(Proposal);
}

void FVekaLayoutChild::Place(const FVekaRect& Rect) const
{
	Node->ArrangedRect = Rect;
}

double FVekaLayoutChild::GetLayoutPriority() const
{
	return Node->GetLayoutPriority();
}

FPaintGeometry FVekaPaintContext::MakePaintGeometry(const FVekaRect& Rect) const
{
	return Geometry.ToPaintGeometry(FVector2f(Rect.Size), FSlateLayoutTransform(FVector2f(Rect.Position)));
}

FLinearColor FVekaPaintContext::Tint(const FLinearColor& Color) const
{
	return Veka::PaintRender::ApplyTint(*this, Color);
}

FVector2D FVekaPrimitive::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	if (Children.IsEmpty())
	{
		return Proposal.ReplaceUnspecified(FVector2D::ZeroVector);
	}

	FVector2D Size = FVector2D::ZeroVector;
	for (const FVekaLayoutChild& Child : Children)
	{
		Size = Size.ComponentMax(Child.SizeThatFits(Proposal));
	}
	return Size;
}

void FVekaPrimitive::PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	const FVekaProposal Proposal = FVekaProposal::FromSize(Bounds.Size);
	for (const FVekaLayoutChild& Child : Children)
	{
		const FVector2D ChildSize = Child.SizeThatFits(Proposal);
		const FVector2D Offset = FVekaAlignment::Center().Offset(Bounds.Size, ChildSize);
		Child.Place(FVekaRect(Bounds.Position + Offset, ChildSize));
	}
}

bool FVekaPrimitive::HitTest(const FVector2D& LocalPoint, const FVector2D& Size) const
{
	return LocalPoint.X >= 0.0 && LocalPoint.Y >= 0.0 && LocalPoint.X <= Size.X && LocalPoint.Y <= Size.Y;
}
