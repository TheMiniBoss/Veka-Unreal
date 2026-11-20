// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaModifier.h"
#include "Core/VekaPaint.h"
#include "Symbols/VekaSymbol.h"
#include "VekaSymbolView.generated.h"

class FVekaEnvironment;

/** How a symbol animates when its name or variant changes. */
UENUM(BlueprintType)
enum class EVekaSymbolReplace : uint8
{
	/** Swapped at once. */
	None,
	/** The old symbol shrinks away, the new one pops in. */
	Replace,
	/** Same symbol, other variant: only what differs animates (the slash draws itself); else Replace. */
	MagicReplace
};

/**
 * How the symbols of a subtree look (SwiftUI symbolRenderingMode / symbolVariant / imageScale / symbolEffect).
 * Symbols take the size and weight of the surrounding font.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaSymbolStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	EVekaSymbolRenderingMode RenderingMode = EVekaSymbolRenderingMode::Monochrome;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FVekaSymbolVariant Variant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	EVekaSymbolScale Scale = EVekaSymbolScale::Medium;

	/** Palette mode: paints of the primary, secondary and tertiary layers (missing ones repeat the last). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	TArray<FVekaPaint> Palette;

	/** Use Weight instead of the font weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	bool bOverrideWeight = false;

	/** 0 = ultralight .. 1 = black (any value in between). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol", meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 0.375f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FVekaSymbolEffect Effect;

	/** Indefinite effects run while true (Appear / Disappear / DrawOn play when it becomes true). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	bool bEffectActive = false;

	/** Every change plays the effect once (SwiftUI `.symbolEffect(_:value:)`). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	int64 EffectTrigger = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	EVekaSymbolReplace Replace = EVekaSymbolReplace::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol", meta = (ClampMin = "0"))
	float ReplaceDuration = 0.35f;

	bool operator==(const FVekaSymbolStyle& Other) const;
};

UENUM(BlueprintType)
enum class EVekaSymbolStyleField : uint8
{
	RenderingMode,
	/** Adds to the outer variant (fill + circle...). */
	Variant,
	Scale,
	Palette,
	Weight,
	/** Effect, active flag and trigger. */
	Effect,
	/** Replace mode and duration. */
	Replace
};

/** Sets one field of the symbol style for the subtree. */
USTRUCT(BlueprintType, meta = (VekaName = "symbolStyle", VekaCategory = "Symbols"))
struct VEKAUI_API FVekaSymbolStyleModifier : public FVekaModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	EVekaSymbolStyleField Field = EVekaSymbolStyleField::RenderingMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FVekaSymbolStyle Value;

	virtual void ModifyEnvironment(FVekaEnvironment& Environment) const override;
};

namespace VekaEnv
{
	/** FVekaSymbolStyle of the subtree. */
	inline FName SymbolStyle() { static const FName Name(TEXT("Veka.SymbolStyle")); return Name; }

	VEKAUI_API const FVekaSymbolStyle& GetSymbolStyle(const FVekaEnvironment& Environment);
}

/**
 * A Veka symbol ("bell", "wifi", "checkmark.circle.fill", Alpha 13 names like "Warning").
 * Sized and weighted like the surrounding text; colored by the foreground and the symbol style.
 */
USTRUCT(BlueprintType, meta = (VekaName = "Symbol", VekaCategory = "Primitives"))
struct VEKAUI_API FVekaSymbolView : public FVekaPrimitive
{
	GENERATED_BODY()

	/** Symbol name, with optional variant suffixes (.fill, .circle, .square, .slash). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FName Name;

	/** Added to the name suffixes and the environment variant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	FVekaSymbolVariant Variant;

	/** 0..1 lights the variable layers (wifi arcs, bars); -1 = not variable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol", meta = (ClampMin = "-1", ClampMax = "1"))
	float VariableValue = -1.f;

	/** Badge at the top right: count (> 0), dot (-1) or none (0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Symbol")
	int32 Badge = 0;

	virtual FVector2D SizeThatFits(const FVekaProposal& Proposal, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const override;
	virtual int32 Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const override;
	virtual bool AcceptsChildren() const override { return false; }
	virtual TSharedPtr<FVekaPrimitiveState> CreateState() const override;
};

namespace Veka::Symbols
{
	/** Side of the symbol box for this environment (font size and symbol scale). */
	VEKAUI_API float GetSymbolSize(const FVekaEnvironment& Environment);
}
