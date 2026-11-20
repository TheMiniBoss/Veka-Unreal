// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Core/VekaModifier.h"
#include "VekaLayoutModifiers.generated.h"

USTRUCT(BlueprintType, meta = (VekaName = "padding", VekaCategory = "Layout"))
struct VEKAUI_API FVekaPaddingModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	FMargin Insets = FMargin(8.f);

	FVekaPaddingModifier() = default;
	explicit FVekaPaddingModifier(const FMargin& InInsets) : Insets(InInsets) {}

	virtual FVector2D Measure(const FVekaProposal& Proposal, FMeasureFn Inner) const override;
	virtual FVekaRect Place(const FVekaRect& Outer, FMeasureFn Inner) const override;
};

/**
 * Fixed and/or flexible frame (SwiftUI's frame(width:height:) and frame(minWidth:maxWidth:...)).
 * A negative value means "not set".
 */
USTRUCT(BlueprintType, meta = (VekaName = "frame", VekaCategory = "Layout"))
struct VEKAUI_API FVekaFrameModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	double Width = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	double Height = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	double MinWidth = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	double MaxWidth = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	double MinHeight = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	double MaxHeight = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	FVekaAlignment Alignment;

	virtual FVector2D Measure(const FVekaProposal& Proposal, FMeasureFn Inner) const override;
	virtual FVekaRect Place(const FVekaRect& Outer, FMeasureFn Inner) const override;

	/** Proposal forwarded to the content. */
	FVekaProposal MakeInnerProposal(const FVekaProposal& Proposal) const;
	/** Final size along one axis from the proposal and the content size. */
	static double ResolveAxis(TOptional<double> Proposed, double Content, double Fixed, double Min, double Max);
};

USTRUCT(BlueprintType, meta = (VekaName = "layoutPriority", VekaCategory = "Layout"))
struct VEKAUI_API FVekaLayoutPriorityModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	double Priority = 0.0;

	virtual void ModifyLayoutTraits(FVekaLayoutTraits& Traits) const override { Traits.LayoutPriority = Priority; }
};
