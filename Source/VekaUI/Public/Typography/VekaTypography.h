// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Core/VekaTypes.h"
#include "Core/VekaModifier.h"
#include "Typography/VekaFont.h"
#include "VekaTypography.generated.h"

class FVekaEnvironment;
class UVekaTheme;

/** Semantic text styles (theme tokens): size, weight, line height and optical tracking come from the style. */
UENUM(BlueprintType)
enum class EVekaTextStyle : uint8
{
	LargeTitle,
	Title1,
	Title2,
	Title3,
	Headline,
	Body,
	Callout,
	Subheadline,
	Footnote,
	Caption1,
	Caption2
};

UENUM(BlueprintType)
enum class EVekaTextCase : uint8
{
	None,
	Uppercase,
	Lowercase
};

UENUM(BlueprintType)
enum class EVekaTextTruncation : uint8
{
	/** Extra lines are dropped. */
	None,
	/** "…" at the start of the last line. */
	Head,
	/** "…" in the middle of the last line. */
	Middle,
	/** "…" at the end of the last line. */
	Tail
};

/** How the text animates when its content changes (SwiftUI's contentTransition). */
UENUM(BlueprintType)
enum class EVekaContentTransition : uint8
{
	None,
	/** Cross-fade between the old and the new text. */
	Opacity,
	/** Changed characters roll up (value increases) or down (decreases), like a counter. */
	NumericText
};

/** Per-glyph text effects (a pluggable text renderer). */
UENUM(BlueprintType)
enum class EVekaTextEffectKind : uint8
{
	None,
	/** Glyphs fade and rise in one after the other (on appear and on content change). */
	Appear,
	/** Glyphs are revealed one by one. */
	Typewriter,
	/** Continuous wave. */
	Wave
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaTextEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaTextEffectKind Kind = EVekaTextEffectKind::None;

	/** Seconds between two glyphs (Appear, Typewriter) or of one wave period (Wave). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0"))
	float Stagger = 0.04f;

	/** Seconds of one glyph's own animation (Appear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0"))
	float Duration = 0.35f;

	/** Local units (vertical offset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	float Amplitude = 8.f;

	bool operator==(const FVekaTextEffect& Other) const
	{
		return Kind == Other.Kind && Stagger == Other.Stagger && Duration == Other.Duration && Amplitude == Other.Amplitude;
	}
};

/** Size / weight / line height of a text style, before the global text scale. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaTextStyleSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "1"))
	float Size = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaFontWeight Weight = EVekaFontWeight::Regular;

	/** Multiplier of the font's natural line height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0.5"))
	float LineHeight = 1.f;
};

/**
 * Typography of a subtree (environment value). Set field by field by the text modifiers (`.Font(style)`,
 * `.FontWeight(...)`, `.Tracking(...)`...); inner modifiers override outer ones.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaTypography
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaTextStyle Style = EVekaTextStyle::Body;

	/** Family of the subtree (nullptr = theme family, then LT Flode Neue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	TObjectPtr<UVekaFontFamily> Family;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	bool bOverrideWeight = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaFontWeight Weight = EVekaFontWeight::Regular;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	bool bItalic = false;

	/** 0 = size of the style. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0"))
	float Size = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	bool bOverrideTracking = false;
	/** em (0.05 = 5 % of the size). Default: optical tracking of the size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	float Tracking = 0.f;

	/** 0 = line height of the style. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0"))
	float LineSpacing = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaTextCase Case = EVekaTextCase::None;

	/** Digits all take the width of the widest one (tables, counters, timers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	bool bMonospacedDigits = false;

	/** 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0"))
	int32 LineLimit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaTextTruncation Truncation = EVekaTextTruncation::Tail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaHAlign Alignment = EVekaHAlign::Leading;

	/** Text may shrink down to this factor to fit before truncating (SwiftUI minimumScaleFactor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0.1", ClampMax = "1"))
	float MinimumScaleFactor = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaContentTransition ContentTransition = EVekaContentTransition::None;

	/** Seconds of a content transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0"))
	float ContentTransitionDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	FVekaTextEffect Effect;

	/** Global text size multiplier (accessibility "text size"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text", meta = (ClampMin = "0.5"))
	float Scale = 1.f;

	/** Explicit Slate font (`.Font(FSlateFontInfo)`): replaces family / style / weight / size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	bool bCustomFont = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	FSlateFontInfo CustomFont;

	bool operator==(const FVekaTypography& Other) const;
};

/** Which fields of FVekaTypography a text modifier sets. */
UENUM(BlueprintType)
enum class EVekaTypographyField : uint8
{
	Style,
	Family,
	Weight,
	Italic,
	Size,
	Tracking,
	LineSpacing,
	Case,
	MonospacedDigits,
	LineLimit,
	Truncation,
	Alignment,
	MinimumScaleFactor,
	ContentTransition,
	Effect,
	Scale,
	CustomFont
};

/** Sets one typography field for the subtree (all text modifiers are this struct). */
USTRUCT(BlueprintType, meta = (VekaName = "typography", VekaCategory = "Text"))
struct VEKAUI_API FVekaTypographyModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	EVekaTypographyField Field = EVekaTypographyField::Style;

	/** Carries the value of Field. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Text")
	FVekaTypography Value;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
};

namespace VekaEnv
{
	/** FVekaTypography of the subtree. */
	inline FName Typography() { static const FName Name(TEXT("Veka.Typography")); return Name; }

	VEKAUI_API const FVekaTypography& GetTypography(const FVekaEnvironment& Environment);
}

namespace Veka::Typography
{
	/** Built-in spec of a style (Veka scale), or the theme's override. */
	VEKAUI_API FVekaTextStyleSpec GetStyleSpec(EVekaTextStyle Style, const UVekaTheme* Theme = nullptr);

	/** Optical tracking in em for a size: a little looser when small, tighter when large. */
	VEKAUI_API float OpticalTracking(float Size);

	/** Everything a text needs to lay out, resolved from the environment. */
	struct VEKAUI_API FResolvedText
	{
		FSlateFontInfo Font;
		float LineHeight = 1.f;
		EVekaTextCase Case = EVekaTextCase::None;
		bool bMonospacedDigits = false;
		int32 LineLimit = 0;
		EVekaTextTruncation Truncation = EVekaTextTruncation::Tail;
		EVekaHAlign Alignment = EVekaHAlign::Leading;
		float MinimumScaleFactor = 1.f;
		EVekaContentTransition ContentTransition = EVekaContentTransition::None;
		float ContentTransitionDuration = 0.35f;
		FVekaTextEffect Effect;
	};

	VEKAUI_API FResolvedText Resolve(const FVekaEnvironment& Environment);

	/** Slate font of a style with the environment's family, theme and text scale (for code that draws text itself). */
	VEKAUI_API FSlateFontInfo StyleFont(EVekaTextStyle Style, const FVekaEnvironment* Environment = nullptr);
}
