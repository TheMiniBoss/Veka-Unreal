// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaPaint.h"
#include "Core/VekaPrimitive.h"
#include "Rendering/DrawElements.h"
#include "Algo/Reverse.h"

namespace
{
	void SetupRoundedBox(FSlateBrush& Brush, const FVekaCornerRadii& Radii, const FLinearColor& OutlineColor, float OutlineWidth)
	{
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Brush.OutlineSettings.CornerRadii = Radii.ToVector4();
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
		Brush.OutlineSettings.Width = OutlineWidth;
	}
}

FVekaPaint FVekaPaint::Clear()
{
	FVekaPaint Paint;
	Paint.Type = EVekaPaintType::None;
	Paint.Color = FLinearColor::Transparent;
	return Paint;
}

FVekaPaint FVekaPaint::Linear(TArray<FVekaGradientStop> InStops, const FVector2D& InStart, const FVector2D& InEnd)
{
	FVekaPaint Paint;
	Paint.Type = EVekaPaintType::LinearGradient;
	Paint.Stops = MoveTemp(InStops);
	Paint.StartPoint = InStart;
	Paint.EndPoint = InEnd;
	Paint.Color = Paint.GetAverageColor();
	return Paint;
}

FVekaPaint FVekaPaint::Linear(const FLinearColor& From, const FLinearColor& To, const FVector2D& InStart, const FVector2D& InEnd)
{
	return Linear({ FVekaGradientStop(From, 0.f), FVekaGradientStop(To, 1.f) }, InStart, InEnd);
}

bool FVekaPaint::IsVisible() const
{
	switch (Type)
	{
	case EVekaPaintType::Solid:
		return Color.A > 0.f;
	case EVekaPaintType::LinearGradient:
		return Stops.ContainsByPredicate([](const FVekaGradientStop& Stop) { return Stop.Color.A > 0.f; });
	default:
		return false;
	}
}

FLinearColor FVekaPaint::GetAverageColor() const
{
	if (Type != EVekaPaintType::LinearGradient || Stops.IsEmpty())
	{
		return Color;
	}
	FLinearColor Sum = FLinearColor::Transparent;
	for (const FVekaGradientStop& Stop : Stops)
	{
		Sum += Stop.Color;
	}
	return Sum / static_cast<float>(Stops.Num());
}

int32 FVekaPaint::Draw(FVekaPaintContext& Context, const FVekaRect& Rect, const FVekaCornerRadii& Radii, FSlateBrush& BrushCache) const
{
	if (!IsVisible() || Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}

	const FVekaCornerRadii Clamped = Radii.ClampedTo(Rect.Size);

	if (Type == EVekaPaintType::LinearGradient && Stops.Num() >= 2)
	{
		// Slate gradients run along one axis: pick the dominant direction of Start -> End.
		const FVector2D Direction = EndPoint - StartPoint;
		const bool bHorizontal = FMath::Abs(Direction.X) >= FMath::Abs(Direction.Y);
		const bool bReversed = bHorizontal ? Direction.X < 0.0 : Direction.Y < 0.0;
		const double Length = bHorizontal ? Rect.Size.X : Rect.Size.Y;

		TArray<FSlateGradientStop> SlateStops;
		SlateStops.Reserve(Stops.Num());
		for (const FVekaGradientStop& Stop : Stops)
		{
			const double Location = bReversed ? 1.0 - Stop.Location : Stop.Location;
			const FVector2D Position = bHorizontal ? FVector2D(Location * Length, 0.0) : FVector2D(0.0, Location * Length);
			SlateStops.Add(FSlateGradientStop(Position, Context.Tint(Stop.Color)));
		}
		if (bReversed)
		{
			Algo::Reverse(SlateStops);
		}

		// Orient_Vertical = stops placed along X (vertical bands). To verify on the Local side.
		FSlateDrawElement::MakeGradient(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Rect), MoveTemp(SlateStops),
			bHorizontal ? Orient_Vertical : Orient_Horizontal, Context.DrawEffects, FVector4f(Clamped.ToVector4()));
		return Context.LayerId;
	}

	SetupRoundedBox(BrushCache, Clamped, FLinearColor::Transparent, 0.f);
	FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Rect), &BrushCache, Context.DrawEffects,
		Context.Tint(GetAverageColor()));
	return Context.LayerId;
}

int32 FVekaPaint::DrawOutline(FVekaPaintContext& Context, const FVekaRect& Rect, const FVekaCornerRadii& Radii, float Width, FSlateBrush& BrushCache) const
{
	if (!IsVisible() || Width <= 0.f || Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}

	// Fill transparent, outline colored. The outline color carries the inherited tint itself.
	SetupRoundedBox(BrushCache, Radii.ClampedTo(Rect.Size), Context.Tint(GetAverageColor()), Width);
	BrushCache.TintColor = FSlateColor(FLinearColor::Transparent);
	FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Rect), &BrushCache, Context.DrawEffects,
		FLinearColor::White);
	return Context.LayerId;
}
