// Copyright TheMiniBoss. All Rights Reserved.

#include "Vector/VekaVectorRenderer.h"
#include "Vector/VekaStroke.h"
#include "Core/VekaPrimitive.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FSlateBrush* GetWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	}

	FSlateVertex MakeVertex(const FSlateRenderTransform& Transform, const FVector2D& LocalPosition, const FColor& Color)
	{
		FSlateVertex Vertex;
		const FVector2f Position = Transform.TransformPoint(FVector2f(LocalPosition));
		Vertex.Position = Position;
		Vertex.TexCoords[0] = 0.5f;
		Vertex.TexCoords[1] = 0.5f;
		Vertex.TexCoords[2] = 1.f;
		Vertex.TexCoords[3] = 1.f;
		Vertex.MaterialTexCoords = FVector2f(0.5f, 0.5f);
		Vertex.Color = Color;
		Vertex.SecondaryColor = FColor::White;
		Vertex.PixelSize[0] = 1;
		Vertex.PixelSize[1] = 1;
		return Vertex;
	}

	void DrawPolylineLines(FVekaPaintContext& Context, const FVekaPolyline& Polyline, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2f> Points;
		Points.Reserve(Polyline.Points.Num() + 1);
		for (const FVector2D& Point : Polyline.Points)
		{
			Points.Add(FVector2f(Point));
		}
		if (Polyline.bClosed && Polyline.Points.Num() > 1)
		{
			Points.Add(FVector2f(Polyline.Points[0]));
		}
		if (Points.Num() < 2)
		{
			return;
		}
		// Points are local to the node geometry.
		FSlateDrawElement::MakeLines(Context.DrawElements, Context.LayerId, Context.Geometry.ToPaintGeometry(), MoveTemp(Points),
			Context.DrawEffects, Context.Tint(Color), true, Thickness);
	}
}

namespace Veka::VectorRender
{
	int32 Fill(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FLinearColor& Color)
	{
		if (Color.A <= 0.f || Polylines.IsEmpty() || !FSlateApplication::IsInitialized())
		{
			return Context.LayerId;
		}

		FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
		const FSlateBrush* WhiteBrush = GetWhiteBrush();
		if (!Renderer || !WhiteBrush)
		{
			return Context.LayerId;
		}
		const FSlateResourceHandle Handle = Renderer->GetResourceHandle(*WhiteBrush);

		const FLinearColor Tinted = Context.Tint(Color);
		const FColor VertexColor = Tinted.ToFColor(true);
		const FSlateRenderTransform& Transform = Context.Geometry.GetAccumulatedRenderTransform();

		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		for (const FVekaPolyline& Polyline : Polylines)
		{
			if (!Polyline.bClosed || Polyline.Points.Num() < 3)
			{
				continue;
			}

			TArray<int32> Triangles;
			if (!Veka::PathOps::Triangulate(Polyline.Points, Triangles))
			{
				// Fallback: fan from the centroid (correct for convex and star-shaped outlines).
				Triangles.Reset();
				const int32 Base = Vertices.Num();
				FVector2D Centroid = FVector2D::ZeroVector;
				for (const FVector2D& Point : Polyline.Points)
				{
					Centroid += Point;
				}
				Centroid /= static_cast<double>(Polyline.Points.Num());
				Vertices.Add(MakeVertex(Transform, Centroid, VertexColor));
				for (const FVector2D& Point : Polyline.Points)
				{
					Vertices.Add(MakeVertex(Transform, Point, VertexColor));
				}
				const int32 Num = Polyline.Points.Num();
				for (int32 Index = 0; Index < Num; ++Index)
				{
					Indices.Add(static_cast<SlateIndex>(Base));
					Indices.Add(static_cast<SlateIndex>(Base + 1 + Index));
					Indices.Add(static_cast<SlateIndex>(Base + 1 + (Index + 1) % Num));
				}
				continue;
			}

			const int32 Base = Vertices.Num();
			for (const FVector2D& Point : Polyline.Points)
			{
				Vertices.Add(MakeVertex(Transform, Point, VertexColor));
			}
			for (int32 Index : Triangles)
			{
				Indices.Add(static_cast<SlateIndex>(Base + Index));
			}
		}

		if (Indices.Num() > 0)
		{
			FSlateDrawElement::MakeCustomVerts(Context.DrawElements, Context.LayerId, Handle, Vertices, Indices, nullptr, 0, 0, Context.DrawEffects);
		}

		// Custom verts are not antialiased: a 1px antialiased outline in the same color softens the edge.
		for (const FVekaPolyline& Polyline : Polylines)
		{
			if (Polyline.bClosed)
			{
				DrawPolylineLines(Context, Polyline, Color, 1.f);
			}
		}
		return Context.LayerId;
	}

	int32 Stroke(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaStrokeStyle& Style)
	{
		if (!Style.IsVisible() || Polylines.IsEmpty())
		{
			return Context.LayerId;
		}

		TArray<FVekaPolyline> Trimmed;
		Veka::PathOps::Trim(Polylines, Style.TrimFrom, Style.TrimTo, Trimmed);

		TArray<FVekaPolyline> Dashed;
		if (Style.Dash.Num() > 0)
		{
			Veka::PathOps::Dash(Trimmed, Style.Dash, Style.DashPhase, Dashed);
		}
		else
		{
			Dashed = MoveTemp(Trimmed);
		}

		const FLinearColor Color = Style.Paint.GetAverageColor();
		for (const FVekaPolyline& Polyline : Dashed)
		{
			DrawPolylineLines(Context, Polyline, Color, Style.Width);
		}
		return Context.LayerId;
	}
}
