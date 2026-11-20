// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaTypes.h"

double FVekaAlignment::AlignH(EVekaHAlign Align, double Container, double Content)
{
	switch (Align)
	{
	case EVekaHAlign::Leading: return 0.0;
	case EVekaHAlign::Trailing: return Container - Content;
	default: return (Container - Content) * 0.5;
	}
}

double FVekaAlignment::AlignV(EVekaVAlign Align, double Container, double Content)
{
	switch (Align)
	{
	case EVekaVAlign::Top: return 0.0;
	case EVekaVAlign::Bottom: return Container - Content;
	default: return (Container - Content) * 0.5;
	}
}

FVector2D FVekaAlignment::Offset(const FVector2D& Container, const FVector2D& Content) const
{
	return FVector2D(AlignH(Horizontal, Container.X, Content.X), AlignV(Vertical, Container.Y, Content.Y));
}

FVekaCornerRadii FVekaCornerRadii::ClampedTo(const FVector2D& Size) const
{
	// Same rule as CSS: scale all radii by the smallest factor that keeps every side valid.
	auto Ratio = [](double Side, double A, double B)
	{
		const double Sum = A + B;
		return Sum > Side && Sum > 0.0 ? Side / Sum : 1.0;
	};

	double Factor = 1.0;
	Factor = FMath::Min(Factor, Ratio(Size.X, TopLeft, TopRight));
	Factor = FMath::Min(Factor, Ratio(Size.X, BottomLeft, BottomRight));
	Factor = FMath::Min(Factor, Ratio(Size.Y, TopLeft, BottomLeft));
	Factor = FMath::Min(Factor, Ratio(Size.Y, TopRight, BottomRight));
	Factor = FMath::Max(Factor, 0.0);

	return FVekaCornerRadii(TopLeft * Factor, TopRight * Factor, BottomRight * Factor, BottomLeft * Factor);
}

FVekaRect FVekaRect::Inset(const FMargin& Margin) const
{
	const FVector2D NewPosition(Position.X + Margin.Left, Position.Y + Margin.Top);
	const FVector2D NewSize(
		FMath::Max(0.0, Size.X - Margin.Left - Margin.Right),
		FMath::Max(0.0, Size.Y - Margin.Top - Margin.Bottom));
	return FVekaRect(NewPosition, NewSize);
}

FVekaRect FVekaRect::Inflate(double Amount) const
{
	return FVekaRect(Position - FVector2D(Amount, Amount), (Size + FVector2D(Amount * 2.0, Amount * 2.0)).ComponentMax(FVector2D::ZeroVector));
}
