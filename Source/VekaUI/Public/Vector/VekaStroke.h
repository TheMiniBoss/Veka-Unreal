// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/VekaPaint.h"
#include "VekaStroke.generated.h"

UENUM(BlueprintType)
enum class EVekaLineCap : uint8
{
	Butt,
	Round,
	Square
};

UENUM(BlueprintType)
enum class EVekaLineJoin : uint8
{
	Miter,
	Round,
	Bevel
};

UENUM(BlueprintType)
enum class EVekaStrokeAlignment : uint8
{
	Center,
	Inside,
	Outside
};

/**
 * How a line or an outline is drawn.
 * Phase 1 renderer: width, solid color, dashes, trim. Caps / joins / alignment / gradient strokes are stored
 * now and rendered by the full stroke tessellator in Phase 2.
 */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaStrokeStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke")
	FVekaPaint Paint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke", meta = (ClampMin = "0"))
	float Width = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke")
	EVekaLineCap Cap = EVekaLineCap::Butt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke")
	EVekaLineJoin Join = EVekaLineJoin::Miter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke", meta = (ClampMin = "1"))
	float MiterLimit = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke")
	EVekaStrokeAlignment Alignment = EVekaStrokeAlignment::Center;

	/** On / off lengths. Empty = solid line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke")
	TArray<float> Dash;

	/** Offset into the dash pattern (animate it for "marching ants"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke")
	float DashPhase = 0.f;

	/** Part of the path drawn, 0..1 (animate for a draw-on effect). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke", meta = (ClampMin = "0", ClampMax = "1"))
	float TrimFrom = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Stroke", meta = (ClampMin = "0", ClampMax = "1"))
	float TrimTo = 1.f;

	FVekaStrokeStyle() = default;
	FVekaStrokeStyle(const FVekaPaint& InPaint, float InWidth) : Paint(InPaint), Width(InWidth) {}

	bool IsVisible() const { return Width > 0.f && Paint.IsVisible() && TrimTo > TrimFrom; }
	bool IsSimple() const { return Dash.IsEmpty() && TrimFrom <= 0.f && TrimTo >= 1.f; }
};
