// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "VekaImage.generated.h"

UENUM(BlueprintType)
enum class EVekaContentMode : uint8
{
	/** Whole image visible, aspect kept. */
	Fit,
	/** Covers the rect, aspect kept, overflow clipped. */
	Fill,
	/** Stretched to the rect. */
	Stretch
};

/**
 * Texture or material. Like SwiftUI, an image has its natural size unless bResizable is set,
 * in which case it takes the proposed size according to ContentMode.
 */
USTRUCT(BlueprintType, meta = (VekaName = "Image", VekaCategory = "Primitives"))
struct VEKAUI_API FVekaImage : public FVekaPrimitive
{
	GENERATED_BODY()

	/** UTexture or UMaterialInterface (UI domain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Image", meta = (AllowedClasses = "/Script/Engine.Texture,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Image")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Image")
	bool bResizable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Image")
	EVekaContentMode ContentMode = EVekaContentMode::Fit;

	/** Natural size. Zero = read from the texture (64x64 for materials). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Image")
	FVector2D NaturalSize = FVector2D::ZeroVector;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;

	FVector2D GetNaturalSize() const;
};
