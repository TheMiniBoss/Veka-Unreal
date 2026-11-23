// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaPaint.h"
#include "Vector/VekaShape.h"
#include "Vector/VekaStroke.h"
#include "VekaShapeView.generated.h"

/**
 * A shape drawn as a view: fill and/or stroke. Takes the proposed size (10x10 when unspecified), like SwiftUI.
 * Hit-testing follows the exact shape (no clicks in rounded corners).
 * Veka::Color(c) is a Rectangle filled with c.
 */
USTRUCT(BlueprintType, meta = (VekaName = "Shape", VekaCategory = "Primitives"))
struct VEKAUI_API FVekaShapeView : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape")
	FVekaShape Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape", meta = (InlineEditConditionToggle))
	bool bFill = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape", meta = (EditCondition = "bFill"))
	FVekaPaint Fill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape", meta = (InlineEditConditionToggle))
	bool bStroke = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape", meta = (EditCondition = "bStroke"))
	FVekaStrokeStyle Stroke;

	/** True when the fill comes from the environment foreground color (shapes without an explicit fill). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape")
	bool bFillWithForeground = false;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool HitTest(const FVector2D& LocalPoint, const FVector2D& Size) const override;
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;
};
