// Copyright TheMiniBoss. All Rights Reserved.

#include "Layout/VekaScrollView.h"
#include "Core/VekaNode.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"

namespace
{
	struct FVekaScrollViewState : public FVekaScrollState
	{
		FSlateBrush IndicatorBrush;
	};
}

TSharedPtr<FVekaPrimitiveState> FVekaScrollView::CreateState() const
{
	return MakeShared<FVekaScrollViewState>();
}

FVekaProposal FVekaScrollView::MakeContentProposal(const FVekaProposal& Proposal) const
{
	FVekaProposal Content = Proposal;
	if (ScrollsHorizontally())
	{
		Content.Width.Reset();
	}
	if (ScrollsVertically())
	{
		Content.Height.Reset();
	}
	return Content;
}

FVector2D FVekaScrollView::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	// A scroll view takes the space it is offered; without a proposal it is as big as its content.
	FVector2D Content = FVector2D::ZeroVector;
	for (const FVekaLayoutChild& Child : Children)
	{
		Content = Content.ComponentMax(Child.SizeThatFits(MakeContentProposal(Proposal)));
	}
	return Proposal.ReplaceUnspecified(Content);
}

void FVekaScrollView::PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	FVekaScrollState* State = static_cast<FVekaScrollState*>(Context.State);

	FVector2D ContentSize = FVector2D::ZeroVector;
	for (const FVekaLayoutChild& Child : Children)
	{
		FVector2D Size = Child.SizeThatFits(MakeContentProposal(FVekaProposal::FromSize(Bounds.Size)));
		// The content is at least as large as the viewport on the non-scrolling axis.
		if (!ScrollsHorizontally())
		{
			Size.X = FMath::Min(Size.X, Bounds.Size.X);
		}
		if (!ScrollsVertically())
		{
			Size.Y = FMath::Min(Size.Y, Bounds.Size.Y);
		}
		ContentSize = ContentSize.ComponentMax(Size);
	}

	FVector2D Offset = FVector2D::ZeroVector;
	if (State)
	{
		State->ContentSize = ContentSize;
		State->ViewportSize = Bounds.Size;
		State->ClampOffset();
		Offset = State->Offset;
	}

	for (const FVekaLayoutChild& Child : Children)
	{
		Child.Place(FVekaRect(Bounds.Position - Offset, ContentSize));
	}
}

FReply FVekaScrollView::OnMouseWheel(FVekaInputContext& Context, const FPointerEvent& Event) const
{
	FVekaScrollState* State = static_cast<FVekaScrollState*>(Context.State);
	if (!State)
	{
		return FReply::Unhandled();
	}

	const FVector2D Before = State->Offset;
	const double Delta = -Event.GetWheelDelta() * WheelStep;
	if (ScrollsVertically() && !Event.IsShiftDown())
	{
		State->Offset.Y += Delta;
	}
	else if (ScrollsHorizontally())
	{
		State->Offset.X += Delta;
	}
	State->ClampOffset();

	if (State->Offset.Equals(Before))
	{
		// At the edge: let a parent scroll view handle it.
		return FReply::Unhandled();
	}
	Context.Node.InvalidateLayout();
	return FReply::Handled();
}

int32 FVekaScrollView::PaintOverlay(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	FVekaScrollViewState* State = static_cast<FVekaScrollViewState*>(Context.State);
	if (!bShowsIndicators || !State)
	{
		return Context.LayerId;
	}

	FSlateBrush& Brush = State->IndicatorBrush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	Brush.OutlineSettings.Width = 0.f;
	const FLinearColor Color = Context.Tint(FLinearColor(0.5f, 0.5f, 0.5f, 0.55f));
	constexpr double Thickness = 4.0;
	constexpr double Inset = 2.0;

	if (ScrollsVertically() && State->ContentSize.Y > Bounds.Size.Y + 0.5)
	{
		const double Ratio = Bounds.Size.Y / State->ContentSize.Y;
		const double Length = FMath::Max(24.0, (Bounds.Size.Y - Inset * 2.0) * Ratio);
		const double Travel = Bounds.Size.Y - Inset * 2.0 - Length;
		const double Progress = State->GetMaxOffset().Y > 0.0 ? State->Offset.Y / State->GetMaxOffset().Y : 0.0;
		const FVekaRect Bar(FVector2D(Bounds.Right() - Thickness - Inset, Bounds.Top() + Inset + Travel * Progress), FVector2D(Thickness, Length));
		FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Bar), &Brush, Context.DrawEffects, Color);
	}
	if (ScrollsHorizontally() && State->ContentSize.X > Bounds.Size.X + 0.5)
	{
		const double Ratio = Bounds.Size.X / State->ContentSize.X;
		const double Length = FMath::Max(24.0, (Bounds.Size.X - Inset * 2.0) * Ratio);
		const double Travel = Bounds.Size.X - Inset * 2.0 - Length;
		const double Progress = State->GetMaxOffset().X > 0.0 ? State->Offset.X / State->GetMaxOffset().X : 0.0;
		const FVekaRect Bar(FVector2D(Bounds.Left() + Inset + Travel * Progress, Bounds.Bottom() - Thickness - Inset), FVector2D(Length, Thickness));
		FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Bar), &Brush, Context.DrawEffects, Color);
	}
	return Context.LayerId;
}
