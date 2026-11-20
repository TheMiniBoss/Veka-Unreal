// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/VekaPaint.h"
#include "Typography/VekaTypography.h"
#include "VekaTheme.generated.h"

class FVekaEnvironment;

/** One theme color: a paint per color scheme (a token can resolve to a gradient). */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaThemeColor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Theme")
	FVekaPaint Light;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Theme")
	FVekaPaint Dark;

	FVekaThemeColor() = default;
	FVekaThemeColor(const FVekaPaint& InLight, const FVekaPaint& InDark) : Light(InLight), Dark(InDark) {}
};

/**
 * Colora theme: semantic tokens -> paints per color scheme, plus design metrics (radii, spacings...).
 * A theme only overrides what changes: missing tokens come from Parent, then from the built-in Veka defaults.
 * Apply it to any subtree with `.Theme(MyTheme)`; switch light / dark with `.ColorScheme(...)`.
 * Fonts per text style come with the typography milestone (2c).
 */
UCLASS(BlueprintType)
class VEKAUI_API UVekaTheme : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Theme")
	TObjectPtr<UVekaTheme> Parent;

	/** Token name -> paint per scheme. Names are free; built-in names: Veka::Colora::DefaultTokenNames(). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Theme")
	TMap<FName, FVekaThemeColor> Colors;

	/** Design metrics (CornerRadius.Card, Spacing.M...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Theme")
	TMap<FName, float> Metrics;

	/** Font family of the theme (nullptr = parent, then LT Flode Neue). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Theme")
	TObjectPtr<UVekaFontFamily> FontFamily;

	/** Overrides of the text styles (size, weight, line height). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Theme")
	TMap<EVekaTextStyle, FVekaTextStyleSpec> TextStyles;

	const FVekaTextStyleSpec* FindTextStyle(EVekaTextStyle Style) const;

	/** Looks the token up in this theme and its parents (not in the built-in defaults). */
	const FVekaThemeColor* FindColor(FName Token) const;
	const float* FindMetric(FName Name) const;
};

namespace VekaEnv
{
	/** const UVekaTheme* — theme of the subtree (nullptr = built-in defaults). */
	inline FName Theme() { static const FName Name(TEXT("Veka.Theme")); return Name; }
	/** FVekaColorMatrix — accumulated color adjustments (brightness, saturation...). */
	inline FName ColorMatrix() { static const FName Name(TEXT("Veka.ColorMatrix")); return Name; }
	/** EVekaBlendMode — how paints of the subtree are composited. */
	inline FName BlendMode() { static const FName Name(TEXT("Veka.BlendMode")); return Name; }

	VEKAUI_API const UVekaTheme* GetTheme(const FVekaEnvironment& Environment);
	VEKAUI_API EVekaBlendMode GetBlendMode(const FVekaEnvironment& Environment);
	VEKAUI_API const FVekaColorMatrix* GetColorMatrix(const FVekaEnvironment& Environment);
}

namespace Veka::Colora
{
	/** The 26 built-in tokens (Alpha 13 Colora presets). */
	VEKAUI_API TConstArrayView<FName> DefaultTokenNames();

	/** Built-in color of a token for a scheme (system palette). False if the token is unknown. */
	VEKAUI_API bool GetDefaultColor(FName Token, EVekaColorScheme Scheme, FLinearColor& OutColor);

	/** Alpha multiplier of a hierarchy level. */
	VEKAUI_API float HierarchyAlpha(EVekaHierarchy Hierarchy);

	/**
	 * Paint with every token replaced by its value for the environment's theme and color scheme
	 * (token paints, gradient stops, mesh points). Unknown tokens resolve to magenta, with a warning once.
	 */
	VEKAUI_API FVekaPaint Resolve(const FVekaPaint& Paint, const FVekaEnvironment& Environment);

	/** Solid color of a token (gradient tokens give their average color). */
	VEKAUI_API FLinearColor ResolveColor(FName Token, const FVekaEnvironment& Environment, EVekaHierarchy Hierarchy = EVekaHierarchy::Primary);

	/** Theme metric, or Default when no theme in the chain defines it. */
	VEKAUI_API float Metric(FName Name, const FVekaEnvironment& Environment, float Default);
}
