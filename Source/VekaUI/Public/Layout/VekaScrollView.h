// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "VekaScrollView.generated.h"

struct VEKAUI_API FVekaScrollState : public FVekaPrimitiveState
{
	FVector2D Offset = FVector2D::ZeroVector;
	FVector2D ContentSize = FVector2D::ZeroVector;
	FVector2D ViewportSize = FVector2D::ZeroVector;

	FVector2D GetMaxOffset() const { return (ContentSize - ViewportSize).ComponentMax(FVector2D::ZeroVector); }
	void ClampOffset() { Offset = Offset.ComponentMax(FVector2D::ZeroVector).ComponentMin(GetMaxOffset()); }
};

/**
 * Scrolls its content along Axis (Depth = both axes). Content is proposed an unspecified size along the
 * scroll axis. Phase 1 input: mouse wheel. Drag, inertia and overscroll arrive with Anima in Phase 2.
 */
USTRUCT(BlueprintType, meta = (VekaName = "ScrollView", VekaCategory = "Layout"))
struct VEKAUI_API FVekaScrollView : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	EVekaAxis Axis = EVekaAxis::Vertical;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	bool bShowsIndicators = true;

	/** Slate units per wheel notch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout", meta = (ClampMin = "1"))
	float WheelStep = 48.f;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual void PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 PaintOverlay(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool ClipsChildren() const override { return true; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;
	virtual bool WantsInput() const override { return true; }
	virtual FReply OnMouseWheel(FVekaInputContext& Context, const FPointerEvent& Event) const override;

	FVekaProposal MakeContentProposal(const FVekaProposal& Proposal) const;
	bool ScrollsHorizontally() const { return Axis != EVekaAxis::Vertical; }
	bool ScrollsVertically() const { return Axis != EVekaAxis::Horizontal; }
};
