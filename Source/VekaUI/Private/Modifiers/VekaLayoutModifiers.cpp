// Copyright TheMiniBoss. All Rights Reserved.

#include "Modifiers/VekaLayoutModifiers.h"

namespace
{
	bool IsSet(double Value) { return Value >= 0.0; }

	TOptional<double> Shrink(TOptional<double> Value, double Amount)
	{
		if (!Value.IsSet() || Veka::IsInfinite(Value.GetValue()))
		{
			return Value;
		}
		return FMath::Max(0.0, Value.GetValue() - Amount);
	}
}

FVector2D FVekaPaddingModifier::Measure(const FVekaProposal& Proposal, FMeasureFn Inner) const
{
	const double Horizontal = Insets.Left + Insets.Right;
	const double Vertical = Insets.Top + Insets.Bottom;
	const FVector2D Content = Inner(FVekaProposal(Shrink(Proposal.Width, Horizontal), Shrink(Proposal.Height, Vertical)));
	return Content + FVector2D(Horizontal, Vertical);
}

FVekaRect FVekaPaddingModifier::Place(const FVekaRect& Outer, FMeasureFn Inner) const
{
	return Outer.Inset(Insets);
}

FVekaProposal FVekaFrameModifier::MakeInnerProposal(const FVekaProposal& Proposal) const
{
	auto Axis = [](TOptional<double> Proposed, double Fixed, double Min, double Max) -> TOptional<double>
	{
		if (IsSet(Fixed))
		{
			return Fixed;
		}
		if (!Proposed.IsSet())
		{
			return Proposed;
		}
		double Value = Proposed.GetValue();
		if (IsSet(Min))
		{
			Value = FMath::Max(Value, Min);
		}
		if (IsSet(Max))
		{
			Value = FMath::Min(Value, Max);
		}
		return Value;
	};
	return FVekaProposal(Axis(Proposal.Width, Width, MinWidth, MaxWidth), Axis(Proposal.Height, Height, MinHeight, MaxHeight));
}

double FVekaFrameModifier::ResolveAxis(TOptional<double> Proposed, double Content, double Fixed, double Min, double Max)
{
	if (IsSet(Fixed))
	{
		return Fixed;
	}
	if (!IsSet(Min) && !IsSet(Max))
	{
		return Content;
	}

	// SwiftUI flexible frame: with a max bound, take the proposal (then clamp); otherwise the content size.
	double Value = Content;
	if (Proposed.IsSet() && IsSet(Max))
	{
		Value = Proposed.GetValue();
	}
	if (IsSet(Min))
	{
		Value = FMath::Max(Value, Min);
	}
	if (IsSet(Max))
	{
		Value = FMath::Min(Value, Max);
	}
	return Value;
}

FVector2D FVekaFrameModifier::Measure(const FVekaProposal& Proposal, FMeasureFn Inner) const
{
	const FVector2D Content = Inner(MakeInnerProposal(Proposal));
	return FVector2D(
		ResolveAxis(Proposal.Width, Content.X, Width, MinWidth, MaxWidth),
		ResolveAxis(Proposal.Height, Content.Y, Height, MinHeight, MaxHeight));
}

FVekaRect FVekaFrameModifier::Place(const FVekaRect& Outer, FMeasureFn Inner) const
{
	// The content gets the frame's final size as proposal (through the frame's own bounds: a root node is given the
	// whole host, larger than its fixed frame), then is aligned inside the frame.
	const FVector2D Content = Inner(MakeInnerProposal(FVekaProposal::FromSize(Outer.Size))).ComponentMin(Outer.Size);
	return FVekaRect(Outer.Position + Alignment.Offset(Outer.Size, Content), Content);
}
