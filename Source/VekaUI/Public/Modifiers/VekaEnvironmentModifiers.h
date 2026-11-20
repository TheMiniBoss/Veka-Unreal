// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Core/VekaModifier.h"
#include "VekaEnvironmentModifiers.generated.h"

USTRUCT(BlueprintType, meta = (VekaName = "foreground", VekaCategory = "Environment"))
struct VEKAUI_API FVekaForegroundModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Environment")
	FLinearColor Color = FLinearColor::White;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
};

USTRUCT(BlueprintType, meta = (VekaName = "font", VekaCategory = "Environment"))
struct VEKAUI_API FVekaFontModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Environment")
	FSlateFontInfo Font;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
};

USTRUCT(BlueprintType, meta = (VekaName = "colorScheme", VekaCategory = "Environment"))
struct VEKAUI_API FVekaColorSchemeModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Environment")
	EVekaColorScheme Scheme = EVekaColorScheme::Dark;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
};
