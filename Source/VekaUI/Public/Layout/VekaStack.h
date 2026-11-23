// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "VekaStack.generated.h"

/**
 * One container for H, V and Z stacks (HStack / VStack / ZStack are builder aliases).
 * Horizontal / Vertical: children laid out along the axis, space shared like SwiftUI (least flexible first,
 * higher layout priority first). Depth: children layered, aligned.
 */
USTRUCT(BlueprintType, meta = (VekaName = "Stack", VekaCategory = "Layout"))
struct VEKAUI_API FVekaStack : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	EVekaAxis Axis = EVekaAxis::Vertical;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout", meta = (ClampMin = "0"))
	float Spacing = 8.f;

	/** Cross-axis alignment for H/V; both axes for Depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	FVekaAlignment Alignment;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual void PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual TOptional<EVekaAxis> GetStackAxis() const override;

	/** Sizes of each child along a linear axis for a proposal (shared by measure and place). */
	void ComputeLinearSizes(const FVekaProposal& Proposal, TConstArrayView<FVekaLayoutChild> Children, TArray<FVector2D>& OutSizes) const;
};

/** Flexible space along the enclosing stack axis. */
USTRUCT(BlueprintType, meta = (VekaName = "Spacer", VekaCategory = "Layout"))
struct VEKAUI_API FVekaSpacer : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout", meta = (ClampMin = "0"))
	float MinLength = 8.f;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual bool AcceptsChildren() const override { return false; }
};
