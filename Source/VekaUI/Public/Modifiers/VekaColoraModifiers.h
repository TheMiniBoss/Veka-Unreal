// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaModifier.h"
#include "Colora/VekaColor.h"
#include "VekaColoraModifiers.generated.h"

class UVekaTheme;

/** Theme of the subtree (Colora tokens and metrics). */
USTRUCT(BlueprintType, meta = (VekaName = "theme", VekaCategory = "Colora"))
struct VEKAUI_API FVekaThemeModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Colora")
	TObjectPtr<UVekaTheme> Theme;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
};

/**
 * Brightness / contrast / saturation / hue rotation / grayscale of everything drawn inside. Stacked adjustments
 * compose (inner first). Applied to the colors Veka computes (solid, gradients, text, strokes): exact and free.
 * Textures and materials only get their tint adjusted until the render module (Phase 4).
 */
USTRUCT(BlueprintType, meta = (VekaName = "colorAdjust", VekaCategory = "Colora"))
struct VEKAUI_API FVekaColorAdjustModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Colora")
	EVekaColorAdjust Adjust = EVekaColorAdjust::Saturation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Colora")
	float Amount = 1.f;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
};

/**
 * How the subtree is composited with what is under it. Normal / Add: the shapes and gradients draw directly. Multiply
 * ... Luminosity: the subtree is blended with its backdrop, inside the node's rect (needs backdrops on; at
 * r.Veka.Backdrop.Quality 0 it draws normally).
 */
USTRUCT(BlueprintType, meta = (VekaName = "blendMode", VekaCategory = "Colora"))
struct VEKAUI_API FVekaBlendModeModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Colora")
	EVekaBlendMode Mode = EVekaBlendMode::Normal;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
	virtual bool PaintsContentTwice() const override;
	virtual int32 PaintBehind(FVekaPaintContext& Context, const FVekaRect& Rect) const override;
	virtual int32 PaintBetween(FVekaPaintContext& Context, const FVekaRect& Rect) const override;
	virtual int32 PaintFront(FVekaPaintContext& Context, const FVekaRect& Rect) const override;

	/** Blended with the backdrop (not Normal or Add). */
	static bool UsesBackdrop(EVekaBlendMode Mode) { return Mode > EVekaBlendMode::Add; }

private:
	/** Elements of this frame's blend (made in PaintBehind, used by PaintBetween / PaintFront). */
	mutable TSharedPtr<struct FVekaBlendElements> Elements;
};
