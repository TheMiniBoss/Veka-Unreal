// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "VekaFont.generated.h"

class UFontFace;
struct FCompositeFont;

/** The 12 weights of Alpha 13 (LT Flode Neue). */
UENUM(BlueprintType)
enum class EVekaFontWeight : uint8
{
	Hairline,
	Thin,
	ExtraLight,
	Light,
	News,
	Regular,
	Medium,
	SemiBold,
	Bold,
	ExtraBold,
	Black,
	ExtraBlack
};

/**
 * A font family: one face per weight, upright and italic. Missing weights fall back to the closest one.
 * Without an asset, Veka uses its built-in family (LT Flode Neue, 12 weights + italics).
 */
UCLASS(BlueprintType)
class VEKAUI_API UVekaFontFamily : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Font")
	TMap<EVekaFontWeight, TObjectPtr<UFontFace>> Faces;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Veka|Font")
	TMap<EVekaFontWeight, TObjectPtr<UFontFace>> ItalicFaces;

	/** Composite font built from the faces (rebuilt when the asset changes). */
	TSharedPtr<const FCompositeFont> GetCompositeFont() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	mutable TSharedPtr<const FCompositeFont> Cached;
};

namespace Veka::Typography
{
	/** Typeface name of a weight inside a Veka composite font ("Bold", "BoldItalic"...). */
	VEKAUI_API FName TypefaceName(EVekaFontWeight Weight, bool bItalic);

	/** Built-in family: LT Flode Neue from the plugin's Resources/Fonts, engine font as fallback for missing glyphs. */
	VEKAUI_API TSharedPtr<const FCompositeFont> GetDefaultFamily();

	/** Slate font of a family (nullptr = default family). Tracking in em (0.01 = 1 %). */
	VEKAUI_API FSlateFontInfo MakeFont(const UVekaFontFamily* Family, float Size, EVekaFontWeight Weight, bool bItalic = false, float TrackingEm = 0.f);
}
