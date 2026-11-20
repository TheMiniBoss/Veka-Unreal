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

/** Normal or additive blending for the shapes and gradients of the subtree. */
USTRUCT(BlueprintType, meta = (VekaName = "blendMode", VekaCategory = "Colora"))
struct VEKAUI_API FVekaBlendModeModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Colora")
	EVekaBlendMode Mode = EVekaBlendMode::Normal;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
};
