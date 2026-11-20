// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Core/VekaPrimitive.h"
#include "VekaText.generated.h"

UENUM(BlueprintType)
enum class EVekaTextTruncation : uint8
{
	/** Extra lines are dropped. */
	None,
	/** Last visible line ends with "…". */
	Tail
};

/**
 * Text. Font and color come from the environment unless overridden (`.Font(...)`, `.Foreground(...)`).
 * Phase 1 layout: greedy word wrapping with Slate font metrics. Phase 2 moves to the full text layout
 * (shaping, bidi, rich text) and semantic text styles.
 */
USTRUCT(BlueprintType, meta = (VekaName = "Text", VekaCategory = "Primitives"))
struct VEKAUI_API FVekaText : public FVekaPrimitive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (InlineEditConditionToggle))
	bool bOverrideFont = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (EditCondition = "bOverrideFont"))
	FSlateFontInfo Font;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (InlineEditConditionToggle))
	bool bOverrideColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (EditCondition = "bOverrideColor"))
	FLinearColor Color = FLinearColor::White;

	/** 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0"))
	int32 LineLimit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaHAlign Alignment = EVekaHAlign::Leading;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaTextTruncation Truncation = EVekaTextTruncation::Tail;

	/** Multiplier of the font line height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0.1"))
	float LineSpacing = 1.f;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;

	FSlateFontInfo ResolveFont(const FVekaEnvironment& Environment) const;
};

namespace Veka::TextLayout
{
	/** Width of a single line. */
	using FMeasureWidth = TFunctionRef<double(const FString&)>;

	/**
	 * Greedy word wrap. MaxWidth <= 0 or infinite = no wrapping (explicit '\n' still break).
	 * LineLimit 0 = unlimited; with bEllipsis the last kept line ends with "…" when text was cut.
	 */
	VEKAUI_API TArray<FString> BreakLines(const FString& Text, double MaxWidth, int32 LineLimit, bool bEllipsis, FMeasureWidth MeasureWidth);
}
