// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "VekaGrid.generated.h"

UENUM(BlueprintType)
enum class EVekaGridItemType : uint8
{
	/** Exact width. */
	Fixed,
	/** One column sharing the remaining width, between Min and Max. */
	Flexible,
	/** As many columns of at least Min as fit (then flexible up to Max). */
	Adaptive
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaGridItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	EVekaGridItemType Type = EVekaGridItemType::Flexible;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout", meta = (ClampMin = "0"))
	float Size = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout", meta = (ClampMin = "0"))
	float Min = 10.f;

	/** <= 0 = unbounded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	float Max = 0.f;

	static FVekaGridItem Fixed(float InSize);
	static FVekaGridItem Flexible(float InMin = 10.f, float InMax = 0.f);
	static FVekaGridItem Adaptive(float InMin, float InMax = 0.f);
};

/** Grid filled row by row (SwiftUI's LazyVGrid, not lazy yet). */
USTRUCT(BlueprintType, meta = (VekaName = "Grid", VekaCategory = "Layout"))
struct VEKAUI_API FVekaGrid : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	TArray<FVekaGridItem> Columns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout", meta = (ClampMin = "0"))
	float HorizontalSpacing = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout", meta = (ClampMin = "0"))
	float VerticalSpacing = 8.f;

	/** Alignment of each child inside its cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	FVekaAlignment Alignment;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual void PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;

	/** Column widths for an available width (unset = ideal widths from the children). */
	void ResolveColumns(TOptional<double> Width, TConstArrayView<FVekaLayoutChild> Children, TArray<double>& OutWidths) const;
	void ResolveRows(const TArray<double>& ColumnWidths, TConstArrayView<FVekaLayoutChild> Children, TArray<double>& OutHeights) const;
};
