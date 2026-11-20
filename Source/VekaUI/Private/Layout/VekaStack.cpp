// Copyright TheMiniBoss. All Rights Reserved.

#include "Layout/VekaStack.h"

TOptional<EVekaAxis> FVekaStack::GetStackAxis() const
{
	return Axis == EVekaAxis::Depth ? TOptional<EVekaAxis>() : TOptional<EVekaAxis>(Axis);
}

void FVekaStack::ComputeLinearSizes(const FVekaProposal& Proposal, TConstArrayView<FVekaLayoutChild> Children, TArray<FVector2D>& OutSizes) const
{
	const int32 Num = Children.Num();
	OutSizes.SetNumZeroed(Num);
	if (Num == 0)
	{
		return;
	}

	const EVekaAxis Cross = Veka::CrossAxis(Axis);
	const double TotalSpacing = Spacing * (Num - 1);
	const TOptional<double> MainProposal = Proposal.Get(Axis);

	auto MakeChildProposal = [&](TOptional<double> Main)
	{
		FVekaProposal ChildProposal;
		ChildProposal.Set(Axis, Main);
		ChildProposal.Set(Cross, Proposal.Get(Cross));
		return ChildProposal;
	};

	// Ideal size along the axis (ScrollView content, nested ideal measurement).
	if (!MainProposal.IsSet())
	{
		for (int32 Index = 0; Index < Num; ++Index)
		{
			OutSizes[Index] = Children[Index].SizeThatFits(MakeChildProposal(TOptional<double>()));
		}
		return;
	}

	// SwiftUI distribution: children are served by layout priority (highest first). A priority group may
	// use everything except the minimum size of the lower-priority children. Inside a group, the least
	// flexible children go first and each is offered an equal share of what is left.
	struct FEntry
	{
		int32 Index;
		double Priority;
		double MinMain;
		double Flexibility;
	};
	TArray<FEntry> Entries;
	Entries.Reserve(Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const double MinMain = Veka::AxisValue(Children[Index].SizeThatFits(MakeChildProposal(0.0)), Axis);
		const double MaxMain = Veka::AxisValue(Children[Index].SizeThatFits(MakeChildProposal(Veka::Infinity)), Axis);
		Entries.Add({ Index, Children[Index].GetLayoutPriority(), MinMain, MaxMain - MinMain });
	}
	Entries.StableSort([](const FEntry& A, const FEntry& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority > B.Priority;
		}
		return A.Flexibility < B.Flexibility;
	});

	double Remaining = FMath::Max(0.0, MainProposal.GetValue() - TotalSpacing);
	int32 GroupStart = 0;
	while (GroupStart < Entries.Num())
	{
		int32 GroupEnd = GroupStart;
		while (GroupEnd < Entries.Num() && Entries[GroupEnd].Priority == Entries[GroupStart].Priority)
		{
			++GroupEnd;
		}

		double LowerMinimum = 0.0;
		for (int32 Lower = GroupEnd; Lower < Entries.Num(); ++Lower)
		{
			LowerMinimum += Entries[Lower].MinMain;
		}

		double GroupAvailable = FMath::Max(0.0, Remaining - LowerMinimum);
		int32 Left = GroupEnd - GroupStart;
		for (int32 Position = GroupStart; Position < GroupEnd; ++Position)
		{
			const FEntry& Entry = Entries[Position];
			const double Share = FMath::Max(0.0, GroupAvailable / Left);
			const FVector2D Size = Children[Entry.Index].SizeThatFits(MakeChildProposal(Share));
			OutSizes[Entry.Index] = Size;
			const double Used = Veka::AxisValue(Size, Axis);
			GroupAvailable = FMath::Max(0.0, GroupAvailable - Used);
			Remaining = FMath::Max(0.0, Remaining - Used);
			--Left;
		}
		GroupStart = GroupEnd;
	}
}

FVector2D FVekaStack::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	if (Children.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}

	if (Axis == EVekaAxis::Depth)
	{
		FVector2D Size = FVector2D::ZeroVector;
		for (const FVekaLayoutChild& Child : Children)
		{
			Size = Size.ComponentMax(Child.SizeThatFits(Proposal));
		}
		return Size;
	}

	TArray<FVector2D> Sizes;
	ComputeLinearSizes(Proposal, Children, Sizes);

	double Main = Spacing * (Children.Num() - 1);
	double CrossSize = 0.0;
	for (const FVector2D& Size : Sizes)
	{
		Main += Veka::AxisValue(Size, Axis);
		CrossSize = FMath::Max(CrossSize, Veka::CrossValue(Size, Axis));
	}
	return Veka::MakeAxisVector(Axis, Main, CrossSize);
}

void FVekaStack::PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	if (Children.IsEmpty())
	{
		return;
	}

	const FVekaProposal Proposal = FVekaProposal::FromSize(Bounds.Size);

	if (Axis == EVekaAxis::Depth)
	{
		for (const FVekaLayoutChild& Child : Children)
		{
			const FVector2D Size = Child.SizeThatFits(Proposal);
			Child.Place(FVekaRect(Bounds.Position + Alignment.Offset(Bounds.Size, Size), Size));
		}
		return;
	}

	TArray<FVector2D> Sizes;
	ComputeLinearSizes(Proposal, Children, Sizes);

	const double CrossContainer = Veka::CrossValue(Bounds.Size, Axis);
	double Cursor = 0.0;
	for (int32 Index = 0; Index < Children.Num(); ++Index)
	{
		const FVector2D& Size = Sizes[Index];
		const double CrossSize = Veka::CrossValue(Size, Axis);
		const double CrossOffset = Axis == EVekaAxis::Horizontal
			? FVekaAlignment::AlignV(Alignment.Vertical, CrossContainer, CrossSize)
			: FVekaAlignment::AlignH(Alignment.Horizontal, CrossContainer, CrossSize);

		const FVector2D Position = Bounds.Position + Veka::MakeAxisVector(Axis, Cursor, CrossOffset);
		Children[Index].Place(FVekaRect(Position, Size));
		Cursor += Veka::AxisValue(Size, Axis) + Spacing;
	}
}

FVector2D FVekaSpacer::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	if (!Context.ParentStackAxis.IsSet())
	{
		// Outside a stack: flexible in both directions, MinLength when nothing is proposed.
		return Proposal.ReplaceUnspecified(FVector2D(MinLength, MinLength)).ComponentMax(FVector2D(MinLength, MinLength));
	}

	const EVekaAxis StackAxis = Context.ParentStackAxis.GetValue();
	const TOptional<double> Main = Proposal.Get(StackAxis);
	const double Length = FMath::Max<double>(MinLength, Main.Get(MinLength));
	return Veka::MakeAxisVector(StackAxis, Length, 0.0);
}
