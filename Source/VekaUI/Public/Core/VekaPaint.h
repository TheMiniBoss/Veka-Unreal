// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Core/VekaTypes.h"
#include "VekaPaint.generated.h"

struct FVekaPaintContext;

/**
 * Phase 1 paints: none, solid, linear gradient. Radial / angular / mesh / material / glass come with Colora
 * (Phase 2) and the render module (Phase 4).
 */
UENUM(BlueprintType)
enum class EVekaPaintType : uint8
{
	None,
	Solid,
	LinearGradient
};

USTRUCT(BlueprintType)
struct VEKAUI_API FVekaGradientStop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FLinearColor Color = FLinearColor::White;

	/** 0..1 along the gradient. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint", meta = (ClampMin = "0", ClampMax = "1"))
	float Location = 0.f;

	FVekaGradientStop() = default;
	FVekaGradientStop(const FLinearColor& InColor, float InLocation) : Color(InColor), Location(InLocation) {}
};

/** What fills or strokes a shape. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaPaint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	EVekaPaintType Type = EVekaPaintType::Solid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	TArray<FVekaGradientStop> Stops;

	/** Unit-space start of a linear gradient ((0,0) = top-left, (1,1) = bottom-right). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FVector2D StartPoint = FVector2D(0.0, 0.5);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Paint")
	FVector2D EndPoint = FVector2D(1.0, 0.5);

	FVekaPaint() = default;
	/** Implicit: a color is a solid paint. */
	FVekaPaint(const FLinearColor& InColor) : Type(EVekaPaintType::Solid), Color(InColor) {}

	static FVekaPaint Solid(const FLinearColor& InColor) { return FVekaPaint(InColor); }
	static FVekaPaint Clear();
	static FVekaPaint Linear(TArray<FVekaGradientStop> InStops, const FVector2D& InStart = FVector2D(0.0, 0.5), const FVector2D& InEnd = FVector2D(1.0, 0.5));
	static FVekaPaint Linear(const FLinearColor& From, const FLinearColor& To, const FVector2D& InStart = FVector2D(0.0, 0.5), const FVector2D& InEnd = FVector2D(1.0, 0.5));

	bool IsVisible() const;

	/** Representative color (strokes and paths in Phase 1 use it for gradients). */
	FLinearColor GetAverageColor() const;

	/**
	 * Draws the paint in Rect with rounded corners. BrushCache must stay alive until the frame is rendered
	 * (keep it in the modifier or the primitive state, never on the stack).
	 */
	int32 Draw(FVekaPaintContext& Context, const FVekaRect& Rect, const FVekaCornerRadii& Radii, FSlateBrush& BrushCache) const;

	/** Draws only an outline of Width inside Rect. */
	int32 DrawOutline(FVekaPaintContext& Context, const FVekaRect& Rect, const FVekaCornerRadii& Radii, float Width, FSlateBrush& BrushCache) const;
};
