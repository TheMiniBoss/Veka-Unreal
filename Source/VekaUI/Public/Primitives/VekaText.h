// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Core/VekaPrimitive.h"
#include "Typography/VekaTypography.h"
#include "VekaText.generated.h"

/**
 * Text. Everything typographic comes from the environment (`.Font(EVekaTextStyle::Title1)`, `.FontWeight`,
 * `.Tracking`, `.LineLimit`, `.ContentTransition`, `.TextEffect`...), so it applies to a whole subtree.
 * Default: Body style in LT Flode Neue. Gradient foregrounds color each glyph.
 */
USTRUCT(BlueprintType, meta = (VekaName = "Text", VekaCategory = "Primitives"))
struct VEKAUI_API FVekaText : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	FText Text;

	/** Explicit Slate font for this text only (bypasses the typography). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (InlineEditConditionToggle))
	bool bOverrideFont = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (EditCondition = "bOverrideFont"))
	FSlateFontInfo Font;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (InlineEditConditionToggle))
	bool bOverrideColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (EditCondition = "bOverrideColor"))
	FLinearColor Color = FLinearColor::White;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;

	/** Typography of this text in an environment (with the explicit font, if any). */
	Veka::Typography::FResolvedText ResolveText(const FVekaEnvironment& Environment) const;
	FSlateFontInfo ResolveFont(const FVekaEnvironment& Environment) const { return ResolveText(Environment).Font; }
};

namespace Veka::TextLayout
{
	/** Width of a single line. */
	using FMeasureWidth = TFunctionRef<double(const FString&)>;

	/**
	 * Greedy word wrap. MaxWidth <= 0 or infinite = no wrapping (explicit '\n' still break). LineLimit 0 = unlimited.
	 * When text is cut, the last kept line gets "…" at its end, start or middle (Truncation), showing the end of the
	 * text for Head. OutCut tells if text was dropped.
	 */
	VEKAUI_API TArray<FString> BreakLines(const FString& Text, double MaxWidth, int32 LineLimit, EVekaTextTruncation Truncation, FMeasureWidth MeasureWidth, bool* OutCut = nullptr);

	/** Phase 1 signature: bEllipsis = tail truncation. */
	VEKAUI_API TArray<FString> BreakLines(const FString& Text, double MaxWidth, int32 LineLimit, bool bEllipsis, FMeasureWidth MeasureWidth);

	/** Longest "…" + end / start + "…" + end / start + "…" that fits MaxWidth. */
	VEKAUI_API FString TruncateLine(const FString& Line, double MaxWidth, EVekaTextTruncation Truncation, FMeasureWidth MeasureWidth);
}

namespace Veka::TextEffects
{
	/** Placement of one glyph by an effect or a content transition. */
	struct FGlyphTransform
	{
		FVector2D Offset = FVector2D::ZeroVector;
		float Alpha = 1.f;
		float Scale = 1.f;
	};

	/** Glyph Index of Count, Elapsed seconds after the effect started (on appear or content change). */
	VEKAUI_API FGlyphTransform Evaluate(const FVekaTextEffect& Effect, int32 Index, int32 Count, double Elapsed);

	/** True once a one-shot effect (Appear, Typewriter) is complete. Wave never finishes. */
	VEKAUI_API bool IsFinished(const FVekaTextEffect& Effect, int32 Count, double Elapsed);

	/** +1 if the number in New is larger than in Old (digits roll up), -1 if smaller, 0 if not comparable. */
	VEKAUI_API int32 NumericDirection(const FString& Old, const FString& New);
}
