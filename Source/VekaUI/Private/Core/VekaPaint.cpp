// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaPaint.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "Colora/VekaPaintRenderer.h"
#include "Vector/VekaPath.h"
#include "Rendering/DrawElements.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"

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

	double PixelsPerUnit(const FVekaPaintContext& Context)
	{
		const FVector2f Scale = Context.Geometry.GetAccumulatedRenderTransform().GetMatrix().GetScale().GetVector();
		return FMath::Max(static_cast<double>(FMath::Max(FMath::Abs(Scale.X), FMath::Abs(Scale.Y))), 0.01);
	}

	TArray<FVekaPolyline> RoundedRectPolylines(const FVekaPaintContext& Context, const FVekaRect& Rect, const FVekaCornerRadii& Radii)
	{
		FVekaPath Path;
		if (Radii.IsZero())
		{
			Path.AddRect(Rect);
		}
		else
		{
			Path.AddRoundedRect(Rect, Radii);
		}
		TArray<FVekaPolyline> Polylines;
		Path.Flatten(Polylines, 0.25 / PixelsPerUnit(Context));
		return Polylines;
	}

	bool IsAdditive(const FVekaPaintContext& Context)
	{
		return VekaEnv::GetBlendMode(Context.Environment) == EVekaBlendMode::Add;
	}

	int32 DrawResource(FVekaPaintContext& Context, const FVekaPaint& Paint, const FVekaRect& Rect, const FVekaCornerRadii& Radii, FSlateBrush& Brush)
	{
		Brush = FSlateBrush();
		if (Paint.Type == EVekaPaintType::Image)
		{
			Brush.SetResourceObject(Paint.Image);
			Brush.ImageSize = FVector2D(Paint.Image->GetSurfaceWidth(), Paint.Image->GetSurfaceHeight());
			Brush.Margin = Paint.ImageMargin;
			Brush.Tiling = Paint.ImageTiling;
			if (Paint.ImageMargin.GetDesiredSize().IsNearlyZero() == false)
			{
				Brush.DrawAs = ESlateBrushDrawType::Box;
			}
			else if (!Radii.IsZero() && Paint.ImageTiling == ESlateBrushTileType::NoTile)
			{
				SetupRoundedBox(Brush, Radii, FLinearColor::Transparent, 0.f);
			}
			else
			{
				Brush.DrawAs = ESlateBrushDrawType::Image;
			}
		}
		else
		{
			// Materials: rectangular (rounded material clips come with the render module).
			Brush.SetResourceObject(Paint.Material);
			Brush.ImageSize = Rect.Size;
			Brush.DrawAs = ESlateBrushDrawType::Image;
		}
		FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Rect), &Brush, Context.DrawEffects,
			Veka::PaintRender::ApplyTint(Context, Paint.Color));
		return Context.LayerId;
	}
}

// --- Mesh gradient -----------------------------------------------------------------------------------------

FVekaMeshGradient FVekaMeshGradient::Grid(int32 InWidth, int32 InHeight, TConstArrayView<FLinearColor> Colors, bool bInSmooth)
{
	FVekaMeshGradient Mesh;
	Mesh.Width = FMath::Max(InWidth, 2);
	Mesh.Height = FMath::Max(InHeight, 2);
	Mesh.bSmooth = bInSmooth;
	for (int32 Y = 0; Y < Mesh.Height; ++Y)
	{
		for (int32 X = 0; X < Mesh.Width; ++X)
		{
			FVekaMeshPoint Point;
			Point.Position = FVector2D(static_cast<double>(X) / (Mesh.Width - 1), static_cast<double>(Y) / (Mesh.Height - 1));
			const int32 Index = Y * Mesh.Width + X;
			Point.Color = Colors.IsValidIndex(Index) ? Colors[Index] : FLinearColor::White;
			Mesh.Points.Add(Point);
		}
	}
	return Mesh;
}

// --- Paint factories ---------------------------------------------------------------------------------------

FVekaPaint FVekaPaint::Clear()
{
	FVekaPaint Paint;
	Paint.Type = EVekaPaintType::None;
	Paint.Color = FLinearColor::Transparent;
	return Paint;
}

FVekaPaint FVekaPaint::FromToken(FName InToken, EVekaHierarchy InHierarchy)
{
	FVekaPaint Paint;
	Paint.Type = EVekaPaintType::Token;
	Paint.Token = InToken;
	Paint.Hierarchy = InHierarchy;
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

FVekaPaint FVekaPaint::Radial(TArray<FVekaGradientStop> InStops, float InStartRadius, float InEndRadius, const FVector2D& InCenter)
{
	FVekaPaint Paint;
	Paint.Type = EVekaPaintType::RadialGradient;
	Paint.Stops = MoveTemp(InStops);
	Paint.StartRadius = InStartRadius;
	Paint.EndRadius = InEndRadius;
	Paint.Center = InCenter;
	Paint.Color = Paint.GetAverageColor();
	return Paint;
}

FVekaPaint FVekaPaint::Elliptical(TArray<FVekaGradientStop> InStops, float InStartFraction, float InEndFraction, const FVector2D& InCenter)
{
	FVekaPaint Paint = Radial(MoveTemp(InStops), InStartFraction, InEndFraction, InCenter);
	Paint.Type = EVekaPaintType::EllipticalGradient;
	return Paint;
}

FVekaPaint FVekaPaint::Angular(TArray<FVekaGradientStop> InStops, float InStartAngle, float InEndAngle, const FVector2D& InCenter)
{
	FVekaPaint Paint;
	Paint.Type = EVekaPaintType::AngularGradient;
	Paint.Stops = MoveTemp(InStops);
	Paint.StartAngle = InStartAngle;
	Paint.EndAngle = InEndAngle;
	Paint.Center = InCenter;
	Paint.Color = Paint.GetAverageColor();
	return Paint;
}

FVekaPaint FVekaPaint::FromMesh(const FVekaMeshGradient& InMesh)
{
	FVekaPaint Paint;
	Paint.Type = EVekaPaintType::MeshGradient;
	Paint.Mesh = InMesh;
	Paint.Color = Paint.GetAverageColor();
	return Paint;
}

FVekaPaint FVekaPaint::FromImage(UTexture* InTexture, const FMargin& InNineSlice, ESlateBrushTileType::Type InTiling)
{
	FVekaPaint Paint;
	Paint.Type = EVekaPaintType::Image;
	Paint.Image = InTexture;
	Paint.ImageMargin = InNineSlice;
	Paint.ImageTiling = InTiling;
	return Paint;
}

FVekaPaint FVekaPaint::FromMaterial(UMaterialInterface* InMaterial)
{
	FVekaPaint Paint;
	Paint.Type = EVekaPaintType::Material;
	Paint.Material = InMaterial;
	return Paint;
}

// --- Queries -----------------------------------------------------------------------------------------------

bool FVekaPaint::operator==(const FVekaPaint& Other) const
{
	return StaticStruct()->CompareScriptStruct(this, &Other, PPF_None);
}

bool FVekaPaint::IsGradient() const
{
	switch (Type)
	{
	case EVekaPaintType::LinearGradient:
	case EVekaPaintType::RadialGradient:
	case EVekaPaintType::EllipticalGradient:
	case EVekaPaintType::AngularGradient:
	case EVekaPaintType::MeshGradient:
		return true;
	default:
		return false;
	}
}

bool FVekaPaint::IsVisible() const
{
	switch (Type)
	{
	case EVekaPaintType::Solid:
		return Color.A > 0.f;
	case EVekaPaintType::LinearGradient:
	case EVekaPaintType::RadialGradient:
	case EVekaPaintType::EllipticalGradient:
	case EVekaPaintType::AngularGradient:
		return Stops.ContainsByPredicate([](const FVekaGradientStop& Stop) { return Stop.Color.A > 0.f || !Stop.Token.IsNone(); });
	case EVekaPaintType::MeshGradient:
		return Mesh.IsValid() && Mesh.Points.ContainsByPredicate([](const FVekaMeshPoint& Point) { return Point.Color.A > 0.f || !Point.Token.IsNone(); });
	case EVekaPaintType::Image:
		return Image != nullptr && Color.A > 0.f;
	case EVekaPaintType::Material:
		return Material != nullptr && Color.A > 0.f;
	case EVekaPaintType::Token:
		return !Token.IsNone() && Hierarchy != EVekaHierarchy::Transparent;
	default:
		return false;
	}
}

FLinearColor FVekaPaint::GetAverageColor() const
{
	if (Type == EVekaPaintType::MeshGradient && Mesh.Points.Num() > 0)
	{
		FLinearColor Sum = FLinearColor::Transparent;
		for (const FVekaMeshPoint& Point : Mesh.Points)
		{
			Sum += Point.Color;
		}
		return Sum / static_cast<float>(Mesh.Points.Num());
	}
	if (IsGradient() && Stops.Num() > 0)
	{
		FLinearColor Sum = FLinearColor::Transparent;
		for (const FVekaGradientStop& Stop : Stops)
		{
			Sum += Stop.Color;
		}
		return Sum / static_cast<float>(Stops.Num());
	}
	return Color;
}

FLinearColor FVekaPaint::Evaluate(const FVector2D& LocalPoint, const FVekaRect& Bounds) const
{
	return Veka::Colora::FPaintSampler(*this, Bounds).Sample(LocalPoint);
}

// --- Drawing -----------------------------------------------------------------------------------------------

int32 FVekaPaint::Draw(FVekaPaintContext& Context, const FVekaRect& Rect, const FVekaCornerRadii& Radii, FSlateBrush& BrushCache) const
{
	if (Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}
	const FVekaPaint Paint = Veka::Colora::Resolve(*this, Context.Environment);
	if (!Paint.IsVisible())
	{
		return Context.LayerId;
	}

	const FVekaCornerRadii Clamped = Radii.ClampedTo(Rect.Size);
	if (Paint.Type == EVekaPaintType::Image || Paint.Type == EVekaPaintType::Material)
	{
		return DrawResource(Context, Paint, Rect, Clamped, BrushCache);
	}

	if (Paint.Type == EVekaPaintType::Solid && !IsAdditive(Context))
	{
		// Fast path: Slate's rounded box (batched, antialiased). Its fill comes from the MakeBox tint.
		SetupRoundedBox(BrushCache, Clamped, FLinearColor::Transparent, 0.f);
		FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Rect), &BrushCache, Context.DrawEffects,
			Veka::PaintRender::ApplyTint(Context, Paint.Color));
		return Context.LayerId;
	}

	return Veka::PaintRender::FillPolylines(Context, RoundedRectPolylines(Context, Rect, Clamped), Paint, Rect);
}

int32 FVekaPaint::DrawOutline(FVekaPaintContext& Context, const FVekaRect& Rect, const FVekaCornerRadii& Radii, float Width, FSlateBrush& BrushCache) const
{
	if (Width <= 0.f || Rect.Size.X <= 0.0 || Rect.Size.Y <= 0.0)
	{
		return Context.LayerId;
	}
	FVekaPaint Paint = Veka::Colora::Resolve(*this, Context.Environment);
	if (!Paint.IsVisible())
	{
		return Context.LayerId;
	}
	if (Paint.Type == EVekaPaintType::Image || Paint.Type == EVekaPaintType::Material)
	{
		Paint = FVekaPaint(Paint.GetAverageColor());
	}

	const FVekaCornerRadii Clamped = Radii.ClampedTo(Rect.Size);
	if (Paint.Type == EVekaPaintType::Solid && !IsAdditive(Context))
	{
		// Fill transparent, outline colored. A RoundedBox element is filled with the MakeBox tint (the brush
		// TintColor is not used), so the tint must be transparent.
		SetupRoundedBox(BrushCache, Clamped, Veka::PaintRender::ApplyTint(Context, Paint.Color), Width);
		BrushCache.TintColor = FSlateColor(FLinearColor::Transparent);
		FSlateDrawElement::MakeBox(Context.DrawElements, Context.LayerId, Context.MakePaintGeometry(Rect), &BrushCache, Context.DrawEffects,
			FLinearColor::Transparent);
		return Context.LayerId;
	}

	// Gradient outline: a line centered Width / 2 inside the rect, colored per point.
	const float Half = Width * 0.5f;
	const FVekaRect Inner = Rect.Inflate(-Half);
	const FVekaCornerRadii InnerRadii(FMath::Max(Clamped.TopLeft - Half, 0.f), FMath::Max(Clamped.TopRight - Half, 0.f),
		FMath::Max(Clamped.BottomRight - Half, 0.f), FMath::Max(Clamped.BottomLeft - Half, 0.f));
	return Veka::PaintRender::StrokePolylines(Context, RoundedRectPolylines(Context, Inner, InnerRadii), Paint, Width, Rect);
}
