// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "VekaCanvas.generated.h"

class FVekaEnvironment;

/**
 * Where a canvas draws, in its local space (pixels, origin top left): dots, lines and glows, many per frame, without a
 * view per mark (SwiftUI's Canvas).
 */
class VEKAUI_API FVekaCanvasDrawing
{
public:
	FVekaCanvasDrawing(FVekaPaintContext& InContext, const FVekaRect& InBounds, bool bInAdditive);

	FVector2D GetSize() const { return Bounds.Size; }
	const FVekaEnvironment& GetEnvironment() const;

	/** A filled circle. */
	void Dot(const FVector2D& Center, double Radius, const FLinearColor& Color);
	/** A straight line whose color (and opacity) goes from FromColor to ToColor. */
	void Line(const FVector2D& From, const FVector2D& To, double Width, const FLinearColor& FromColor, const FLinearColor& ToColor);
	/** A soft disc: Color at the center, fading to nothing at Radius. */
	void Glow(const FVector2D& Center, double Radius, const FLinearColor& Color);

	int32 GetMarkCount() const { return Marks; }

private:
	FLinearColor Final(const FLinearColor& Color) const;

	FVekaPaintContext& Context;
	FVekaRect Bounds;
	bool bAdditive = false;
	ESlateDrawEffect Effects = ESlateDrawEffect::None;
	int32 Marks = 0;
};

/** What fills a canvas: the .vk interpreter gives one (a Canvas block); C++ can give its own. */
class VEKAUI_API FVekaCanvasPainter
{
public:
	virtual ~FVekaCanvasPainter() = default;
	virtual void Draw(FVekaCanvasDrawing& Drawing) const = 0;
	/** Redraws every frame (its drawing reads `time`). */
	virtual bool IsAnimated() const { return false; }
};

/**
 * A drawing area (`Canvas(blend: .Add) { Dot(...) Line(...) Glow(...) }` in Veka): its painter draws marks every
 * time it paints, with the canvas size; an animated painter repaints every frame without re-evaluating the view.
 * Takes the proposed size.
 */
USTRUCT(BlueprintType, meta = (VekaName = "Canvas", VekaCategory = "Drawing"))
struct VEKAUI_API FVekaCanvas : public FVekaPrimitive
{
	GENERATED_BODY()

	/** Marks add their light (SwiftUI's .plusLighter): overlaps get brighter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Canvas")
	bool bAdditive = false;

	/** Changes with every new painter (the reconciler compares properties: a new drawing repaints). */
	UPROPERTY()
	int32 PainterRevision = 0;

	TSharedPtr<const FVekaCanvasPainter> Painter;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;
};
