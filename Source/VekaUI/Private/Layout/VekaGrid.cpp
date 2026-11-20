// Copyright TheMiniBoss. All Rights Reserved.

#include "Layout/VekaGrid.h"

FVekaGridItem FVekaGridItem::Fixed(float InSize)
{
	FVekaGridItem Item;
	Item.Type = EVekaGridItemType::Fixed;
	Item.Size = InSize;
	return Item;
}

FVekaGridItem FVekaGridItem::Flexible(float InMin, float InMax)
{
	FVekaGridItem Item;
	Item.Type = EVekaGridItemType::Flexible;
	Item.Min = InMin;
	Item.Max = InMax;
	return Item;
}

FVekaGridItem FVekaGridItem::Adaptive(float InMin, float InMax)
{
	FVekaGridItem Item;
	Item.Type = EVekaGridItemType::Adaptive;
	Item.Min = FMath::Max(InMin, 1.f);
	Item.Max = InMax;
	return Item;
}

namespace
{
	double ClampToItem(double Value, const FVekaGridItem& Item)
	{
		Value = FMath::Max(Value, static_cast<double>(Item.Min));
		if (Item.Max > 0.f)
		{
			Value = FMath::Min(Value, static_cast<double>(Item.Max));
		}
		return Value;
	}
}

void FVekaGrid::ResolveColumns(TOptional<double> Width, TConstArrayView<FVekaLayoutChild> Children, TArray<double>& OutWidths) const
{
	OutWidths.Reset();

	// Expand adaptive items into as many columns as fit. Without a width: one column per adaptive item.
	struct FColumn
	{
		FVekaGridItem Item;
	};
	TArray<FColumn> Expanded;
	const bool bHasWidth = Width.IsSet() && !Veka::IsInfinite(Width.GetValue());

	double FixedTotal = 0.0;
	int32 NonAdaptive = 0;
	for (const FVekaGridItem& Item : Columns)
	{
		if (Item.Type == EVekaGridItemType::Fixed)
		{
			FixedTotal += Item.Size;
		}
		if (Item.Type != EVekaGridItemType::Adaptive)
		{
			++NonAdaptive;
		}
	}

	for (const FVekaGridItem& Item : Columns)
	{
		if (Item.Type == EVekaGridItemType::Adaptive && bHasWidth)
		{
			const double Available = FMath::Max(0.0, Width.GetValue() - FixedTotal - HorizontalSpacing * NonAdaptive);
			const int32 Count = FMath::Max(1, FMath::FloorToInt32((Available + HorizontalSpacing) / (Item.Min + HorizontalSpacing)));
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Expanded.Add({ FVekaGridItem::Flexible(Item.Min, Item.Max) });
			}
		}
		else if (Item.Type == EVekaGridItemType::Adaptive)
		{
			Expanded.Add({ FVekaGridItem::Flexible(Item.Min, Item.Max) });
		}
		else
		{
			Expanded.Add({ Item });
		}
	}

	if (Expanded.IsEmpty())
	{
		Expanded.Add({ FVekaGridItem::Flexible() });
	}

	const int32 NumColumns = Expanded.Num();
	OutWidths.SetNumZeroed(NumColumns);

	if (!bHasWidth)
	{
		// Ideal widths: widest child of each column.
		for (int32 Column = 0; Column < NumColumns; ++Column)
		{
			const FVekaGridItem& Item = Expanded[Column].Item;
			if (Item.Type == EVekaGridItemType::Fixed)
			{
				OutWidths[Column] = Item.Size;
				continue;
			}
			double Widest = 0.0;
			for (int32 Index = Column; Index < Children.Num(); Index += NumColumns)
			{
				Widest = FMath::Max(Widest, Children[Index].SizeThatFits(FVekaProposal::Unspecified()).X);
			}
			OutWidths[Column] = ClampToItem(Widest, Item);
		}
		return;
	}

	double Remaining = Width.GetValue() - HorizontalSpacing * (NumColumns - 1);
	int32 Flexible = 0;
	for (int32 Column = 0; Column < NumColumns; ++Column)
	{
		const FVekaGridItem& Item = Expanded[Column].Item;
		if (Item.Type == EVekaGridItemType::Fixed)
		{
			OutWidths[Column] = Item.Size;
			Remaining -= Item.Size;
		}
		else
		{
			++Flexible;
		}
	}

	// Flexible columns share what is left, respecting min/max (columns that hit a bound are settled first).
	TArray<int32> Open;
	for (int32 Column = 0; Column < NumColumns; ++Column)
	{
		if (Expanded[Column].Item.Type != EVekaGridItemType::Fixed)
		{
			Open.Add(Column);
		}
	}
	bool bSettled = false;
	while (!Open.IsEmpty() && !bSettled)
	{
		bSettled = true;
		const double Share = FMath::Max(0.0, Remaining) / Open.Num();
		for (int32 OpenIndex = Open.Num() - 1; OpenIndex >= 0; --OpenIndex)
		{
			const int32 Column = Open[OpenIndex];
			const double Clamped = ClampToItem(Share, Expanded[Column].Item);
			if (!FMath::IsNearlyEqual(Clamped, Share))
			{
				OutWidths[Column] = Clamped;
				Remaining -= Clamped;
				Open.RemoveAt(OpenIndex);
				bSettled = false;
			}
		}
		if (bSettled)
		{
			for (int32 Column : Open)
			{
				OutWidths[Column] = Share;
			}
		}
	}
}

void FVekaGrid::ResolveRows(const TArray<double>& ColumnWidths, TConstArrayView<FVekaLayoutChild> Children, TArray<double>& OutHeights) const
{
	OutHeights.Reset();
	const int32 NumColumns = FMath::Max(1, ColumnWidths.Num());
	const int32 NumRows = (Children.Num() + NumColumns - 1) / NumColumns;
	OutHeights.SetNumZeroed(NumRows);
	for (int32 Index = 0; Index < Children.Num(); ++Index)
	{
		const int32 Row = Index / NumColumns;
		const int32 Column = Index % NumColumns;
		const double Height = Children[Index].SizeThatFits(FVekaProposal(ColumnWidths[Column], TOptional<double>())).Y;
		OutHeights[Row] = FMath::Max(OutHeights[Row], Height);
	}
}

FVector2D FVekaGrid::SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	TArray<double> Widths;
	ResolveColumns(Proposal.Width, Children, Widths);
	TArray<double> Heights;
	ResolveRows(Widths, Children, Heights);

	double Width = HorizontalSpacing * FMath::Max(0, Widths.Num() - 1);
	for (double W : Widths)
	{
		Width += W;
	}
	double Height = VerticalSpacing * FMath::Max(0, Heights.Num() - 1);
	for (double H : Heights)
	{
		Height += H;
	}
	return FVector2D(Width, Height);
}

void FVekaGrid::PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	TArray<double> Widths;
	ResolveColumns(Bounds.Size.X, Children, Widths);
	TArray<double> Heights;
	ResolveRows(Widths, Children, Heights);

	const int32 NumColumns = FMath::Max(1, Widths.Num());
	double Y = Bounds.Position.Y;
	for (int32 Row = 0; Row < Heights.Num(); ++Row)
	{
		double X = Bounds.Position.X;
		for (int32 Column = 0; Column < NumColumns; ++Column)
		{
			const int32 Index = Row * NumColumns + Column;
			if (Index >= Children.Num())
			{
				break;
			}
			const FVector2D Cell(Widths[Column], Heights[Row]);
			const FVector2D Size = Children[Index].SizeThatFits(FVekaProposal::FromSize(Cell)).ComponentMin(Cell);
			Children[Index].Place(FVekaRect(FVector2D(X, Y) + Alignment.Offset(Cell, Size), Size));
			X += Widths[Column] + HorizontalSpacing;
		}
		Y += Heights[Row] + VerticalSpacing;
	}
}
