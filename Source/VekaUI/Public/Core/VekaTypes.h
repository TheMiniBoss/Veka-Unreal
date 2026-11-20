// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "VekaTypes.generated.h"

namespace Veka
{
	/** Sizes at or above this value are treated as "infinite" (SwiftUI's `.infinity`). A large finite value avoids inf-inf NaNs. */
	inline constexpr double Infinity = 1.0e15;

	inline bool IsInfinite(double Value) { return Value >= Infinity * 0.5; }
}

/** Main axis of a Stack. Depth = children are layered on top of each other (ZStack). */
UENUM(BlueprintType)
enum class EVekaAxis : uint8
{
	Horizontal,
	Vertical,
	Depth
};

UENUM(BlueprintType)
enum class EVekaHAlign : uint8
{
	Leading,
	Center,
	Trailing
};

UENUM(BlueprintType)
enum class EVekaVAlign : uint8
{
	Top,
	Center,
	Bottom
};

UENUM(BlueprintType)
enum class EVekaColorScheme : uint8
{
	Light,
	Dark
};

/** 2D alignment used by stacks, frames and overlays. */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaAlignment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	EVekaHAlign Horizontal = EVekaHAlign::Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Layout")
	EVekaVAlign Vertical = EVekaVAlign::Center;

	FVekaAlignment() = default;
	FVekaAlignment(EVekaHAlign InHorizontal, EVekaVAlign InVertical) : Horizontal(InHorizontal), Vertical(InVertical) {}

	static FVekaAlignment Center() { return FVekaAlignment(); }
	static FVekaAlignment TopLeading() { return FVekaAlignment(EVekaHAlign::Leading, EVekaVAlign::Top); }
	static FVekaAlignment Top() { return FVekaAlignment(EVekaHAlign::Center, EVekaVAlign::Top); }
	static FVekaAlignment TopTrailing() { return FVekaAlignment(EVekaHAlign::Trailing, EVekaVAlign::Top); }
	static FVekaAlignment Leading() { return FVekaAlignment(EVekaHAlign::Leading, EVekaVAlign::Center); }
	static FVekaAlignment Trailing() { return FVekaAlignment(EVekaHAlign::Trailing, EVekaVAlign::Center); }
	static FVekaAlignment BottomLeading() { return FVekaAlignment(EVekaHAlign::Leading, EVekaVAlign::Bottom); }
	static FVekaAlignment Bottom() { return FVekaAlignment(EVekaHAlign::Center, EVekaVAlign::Bottom); }
	static FVekaAlignment BottomTrailing() { return FVekaAlignment(EVekaHAlign::Trailing, EVekaVAlign::Bottom); }

	static double AlignH(EVekaHAlign Align, double Container, double Content);
	static double AlignV(EVekaVAlign Align, double Container, double Content);

	/** Offset of a Content box placed inside a Container box. */
	FVector2D Offset(const FVector2D& Container, const FVector2D& Content) const;

	bool operator==(const FVekaAlignment& Other) const { return Horizontal == Other.Horizontal && Vertical == Other.Vertical; }
	bool operator!=(const FVekaAlignment& Other) const { return !(*this == Other); }
};

/** Per-corner radii, clockwise from the top-left corner (same order as Slate's FVector4 corner radii). */
USTRUCT(BlueprintType)
struct VEKAUI_API FVekaCornerRadii
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape", meta = (ClampMin = "0"))
	float TopLeft = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape", meta = (ClampMin = "0"))
	float TopRight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape", meta = (ClampMin = "0"))
	float BottomRight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Veka|Shape", meta = (ClampMin = "0"))
	float BottomLeft = 0.f;

	FVekaCornerRadii() = default;
	FVekaCornerRadii(float Uniform) : TopLeft(Uniform), TopRight(Uniform), BottomRight(Uniform), BottomLeft(Uniform) {}
	FVekaCornerRadii(float InTopLeft, float InTopRight, float InBottomRight, float InBottomLeft)
		: TopLeft(InTopLeft), TopRight(InTopRight), BottomRight(InBottomRight), BottomLeft(InBottomLeft) {}

	bool IsZero() const { return TopLeft <= 0.f && TopRight <= 0.f && BottomRight <= 0.f && BottomLeft <= 0.f; }
	bool IsUniform() const { return TopLeft == TopRight && TopRight == BottomRight && BottomRight == BottomLeft; }

	/** Radii scaled down so that adjacent corners never overlap inside a box of the given size. */
	FVekaCornerRadii ClampedTo(const FVector2D& Size) const;

	/** Slate order: (TopLeft, TopRight, BottomRight, BottomLeft). */
	FVector4 ToVector4() const { return FVector4(TopLeft, TopRight, BottomRight, BottomLeft); }

	bool operator==(const FVekaCornerRadii& Other) const
	{
		return TopLeft == Other.TopLeft && TopRight == Other.TopRight && BottomRight == Other.BottomRight && BottomLeft == Other.BottomLeft;
	}
	bool operator!=(const FVekaCornerRadii& Other) const { return !(*this == Other); }
};

/** Axis-aligned rectangle in local (Slate unit) space. */
struct VEKAUI_API FVekaRect
{
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;

	FVekaRect() = default;
	FVekaRect(const FVector2D& InPosition, const FVector2D& InSize) : Position(InPosition), Size(InSize) {}

	double Left() const { return Position.X; }
	double Top() const { return Position.Y; }
	double Right() const { return Position.X + Size.X; }
	double Bottom() const { return Position.Y + Size.Y; }
	FVector2D Center() const { return Position + Size * 0.5; }

	bool Contains(const FVector2D& Point) const
	{
		return Point.X >= Left() && Point.X <= Right() && Point.Y >= Top() && Point.Y <= Bottom();
	}

	/** Rect shrunk by a margin (never negative). */
	FVekaRect Inset(const FMargin& Margin) const;
	/** Rect grown by the same amount on every side. */
	FVekaRect Inflate(double Amount) const;
	FVekaRect Translated(const FVector2D& Delta) const { return FVekaRect(Position + Delta, Size); }

	bool Equals(const FVekaRect& Other, double Tolerance = KINDA_SMALL_NUMBER) const
	{
		return Position.Equals(Other.Position, Tolerance) && Size.Equals(Other.Size, Tolerance);
	}
};

/**
 * A size proposal, as in SwiftUI's layout protocol.
 * Unset = "ideal size" (no constraint). Veka::Infinity = "as large as you want". 0 = "as small as you can".
 */
struct VEKAUI_API FVekaProposal
{
	TOptional<double> Width;
	TOptional<double> Height;

	FVekaProposal() = default;
	FVekaProposal(TOptional<double> InWidth, TOptional<double> InHeight) : Width(InWidth), Height(InHeight) {}

	static FVekaProposal Unspecified() { return FVekaProposal(); }
	static FVekaProposal Zero() { return FVekaProposal(0.0, 0.0); }
	static FVekaProposal Infinite() { return FVekaProposal(Veka::Infinity, Veka::Infinity); }
	static FVekaProposal FromSize(const FVector2D& Size) { return FVekaProposal(Size.X, Size.Y); }

	TOptional<double> Get(EVekaAxis Axis) const { return Axis == EVekaAxis::Horizontal ? Width : Height; }
	void Set(EVekaAxis Axis, TOptional<double> Value) { (Axis == EVekaAxis::Horizontal ? Width : Height) = Value; }

	/** Proposal with unset dimensions replaced by Fallback. */
	FVector2D ReplaceUnspecified(const FVector2D& Fallback) const
	{
		return FVector2D(Width.Get(Fallback.X), Height.Get(Fallback.Y));
	}

	bool operator==(const FVekaProposal& Other) const { return Width == Other.Width && Height == Other.Height; }
	bool operator!=(const FVekaProposal& Other) const { return !(*this == Other); }
};

namespace Veka
{
	/** Component of V along a stack axis (Depth maps to vertical). */
	inline double AxisValue(const FVector2D& V, EVekaAxis Axis) { return Axis == EVekaAxis::Horizontal ? V.X : V.Y; }
	inline double CrossValue(const FVector2D& V, EVekaAxis Axis) { return Axis == EVekaAxis::Horizontal ? V.Y : V.X; }
	inline FVector2D MakeAxisVector(EVekaAxis Axis, double Main, double Cross)
	{
		return Axis == EVekaAxis::Horizontal ? FVector2D(Main, Cross) : FVector2D(Cross, Main);
	}
	inline EVekaAxis CrossAxis(EVekaAxis Axis) { return Axis == EVekaAxis::Horizontal ? EVekaAxis::Vertical : EVekaAxis::Horizontal; }
}
