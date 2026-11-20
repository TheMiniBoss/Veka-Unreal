// Copyright TheMiniBoss. All Rights Reserved.

#include "Colora/VekaPaintRenderer.h"
#include "Hash/CityHash.h"
#include "Colora/VekaTheme.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaEnvironment.h"
#include "Vector/VekaPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Algo/Reverse.h"

// --- Sampler -----------------------------------------------------------------------------------------------

namespace Veka::Colora
{
	namespace
	{
		/** Catmull-Rom weights for the 4 points around a segment. */
		void CatmullRomWeights(double T, double Out[4])
		{
			const double T2 = T * T;
			const double T3 = T2 * T;
			Out[0] = 0.5 * (-T3 + 2.0 * T2 - T);
			Out[1] = 0.5 * (3.0 * T3 - 5.0 * T2 + 2.0);
			Out[2] = 0.5 * (-3.0 * T3 + 4.0 * T2 + T);
			Out[3] = 0.5 * (T3 - T2);
		}

		double Cross(const FVector2D& A, const FVector2D& B)
		{
			return A.X * B.Y - A.Y * B.X;
		}
	}

	FPaintSampler::FPaintSampler(const FVekaPaint& InPaint, const FVekaRect& InBounds)
		: Paint(InPaint)
		, Bounds(InBounds)
		, Space(InPaint.ColorSpace)
	{
		if (Paint.Stops.Num() > 0)
		{
			TArray<FVekaGradientStop> Sorted = Paint.Stops;
			Sorted.StableSort([](const FVekaGradientStop& A, const FVekaGradientStop& B) { return A.Location < B.Location; });
			for (const FVekaGradientStop& Stop : Sorted)
			{
				StopLocations.Add(FMath::Clamp(Stop.Location, 0.f, 1.f));
				StopValues.Add(ToSpace(Stop.Color, Space));
			}
		}

		if (Paint.Type == EVekaPaintType::MeshGradient && Paint.Mesh.IsValid())
		{
			MeshWidth = Paint.Mesh.Width;
			MeshHeight = Paint.Mesh.Height;
			bMeshSmooth = Paint.Mesh.bSmooth;
			// Hue is an angle: bicubic weights (which can be negative) would make it wrap. Smooth meshes use Oklab.
			if (bMeshSmooth && Space == EVekaColorSpace::OkLCH)
			{
				Space = EVekaColorSpace::OkLab;
			}
			bMeshRegular = true;
			for (int32 Y = 0; Y < MeshHeight; ++Y)
			{
				for (int32 X = 0; X < MeshWidth; ++X)
				{
					const FVekaMeshPoint& Point = Paint.Mesh.At(X, Y);
					MeshPositions.Add(Bounds.Position + Point.Position * Bounds.Size);
					MeshValues.Add(ToSpace(Point.Color, Space));
					const FVector2D Regular(static_cast<double>(X) / (MeshWidth - 1), static_cast<double>(Y) / (MeshHeight - 1));
					bMeshRegular &= Point.Position.Equals(Regular, 1.0e-4);
				}
			}
		}
	}

	double FPaintSampler::GetSampleSpacingPixels() const
	{
		switch (Paint.Type)
		{
		case EVekaPaintType::LinearGradient: return 16.0;
		case EVekaPaintType::RadialGradient:
		case EVekaPaintType::EllipticalGradient: return 10.0;
		case EVekaPaintType::AngularGradient: return 6.0;
		case EVekaPaintType::MeshGradient: return 10.0;
		default: return 0.0;
		}
	}

	float FPaintSampler::GetGradientT(const FVector2D& P) const
	{
		const FVector2D Size = Bounds.Size;
		switch (Paint.Type)
		{
		case EVekaPaintType::LinearGradient:
		{
			const FVector2D Start = Bounds.Position + Paint.StartPoint * Size;
			const FVector2D End = Bounds.Position + Paint.EndPoint * Size;
			const FVector2D Direction = End - Start;
			const double LengthSquared = Direction.SizeSquared();
			return LengthSquared > UE_SMALL_NUMBER ? static_cast<float>(FVector2D::DotProduct(P - Start, Direction) / LengthSquared) : 0.f;
		}
		case EVekaPaintType::RadialGradient:
		{
			const FVector2D Center = Bounds.Position + Paint.Center * Size;
			const double Distance = FVector2D::Distance(P, Center);
			const double Span = Paint.EndRadius - Paint.StartRadius;
			return FMath::Abs(Span) > UE_SMALL_NUMBER ? static_cast<float>((Distance - Paint.StartRadius) / Span) : (Distance >= Paint.EndRadius ? 1.f : 0.f);
		}
		case EVekaPaintType::EllipticalGradient:
		{
			const FVector2D Center = Bounds.Position + Paint.Center * Size;
			const FVector2D Half(FMath::Max(Size.X * 0.5, UE_SMALL_NUMBER), FMath::Max(Size.Y * 0.5, UE_SMALL_NUMBER));
			const double Distance = ((P - Center) / Half).Size();
			const double Span = Paint.EndRadius - Paint.StartRadius;
			return FMath::Abs(Span) > UE_SMALL_NUMBER ? static_cast<float>((Distance - Paint.StartRadius) / Span) : (Distance >= Paint.EndRadius ? 1.f : 0.f);
		}
		case EVekaPaintType::AngularGradient:
		{
			const FVector2D Center = Bounds.Position + Paint.Center * Size;
			const FVector2D D = P - Center;
			if (D.IsNearlyZero())
			{
				return 0.f;
			}
			// Y points down: atan2 grows clockwise on screen.
			const double Angle = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			const double Span = Paint.EndAngle - Paint.StartAngle;
			if (FMath::Abs(Span) < UE_SMALL_NUMBER)
			{
				return 0.f;
			}
			double Relative = FMath::Fmod(Angle - Paint.StartAngle, 360.0);
			if (Relative < 0.0)
			{
				Relative += 360.0;
			}
			const double AbsSpan = FMath::Min(FMath::Abs(Span), 360.0);
			if (Relative > AbsSpan)
			{
				// Outside a partial sweep: clamp to the closest end.
				return (Relative - AbsSpan) < (360.0 - Relative) ? 1.f : 0.f;
			}
			return static_cast<float>(Relative / AbsSpan);
		}
		default:
			return 0.f;
		}
	}

	FLinearColor FPaintSampler::SampleStops(float T) const
	{
		if (StopValues.IsEmpty())
		{
			return Paint.Color;
		}
		if (T <= StopLocations[0])
		{
			return FromSpace(StopValues[0], Space);
		}
		const int32 Last = StopValues.Num() - 1;
		if (T >= StopLocations[Last])
		{
			return FromSpace(StopValues[Last], Space);
		}
		for (int32 Index = 1; Index <= Last; ++Index)
		{
			if (T <= StopLocations[Index])
			{
				const float Span = StopLocations[Index] - StopLocations[Index - 1];
				const float Local = Span > UE_SMALL_NUMBER ? (T - StopLocations[Index - 1]) / Span : 1.f;
				return FromSpace(LerpInSpace(StopValues[Index - 1], StopValues[Index], Local, Space), Space);
			}
		}
		return FromSpace(StopValues[Last], Space);
	}

	FVector4f FPaintSampler::MeshPoint(int32 X, int32 Y) const
	{
		// Linear extrapolation outside the grid gives natural Catmull-Rom ends.
		if (X < 0)
		{
			return MeshPoint(0, Y) * 2.f - MeshPoint(1, Y);
		}
		if (X >= MeshWidth)
		{
			return MeshPoint(MeshWidth - 1, Y) * 2.f - MeshPoint(MeshWidth - 2, Y);
		}
		if (Y < 0)
		{
			return MeshPoint(X, 0) * 2.f - MeshPoint(X, 1);
		}
		if (Y >= MeshHeight)
		{
			return MeshPoint(X, MeshHeight - 1) * 2.f - MeshPoint(X, MeshHeight - 2);
		}
		return MeshValues[Y * MeshWidth + X];
	}

	FVector2D FPaintSampler::MeshPosition(int32 CellX, int32 CellY, double U, double V) const
	{
		// Positions are bilinear per cell (exactly invertible for regular grids); colors may be bicubic.
		const FVector2D& P00 = MeshPositions[CellY * MeshWidth + CellX];
		const FVector2D& P10 = MeshPositions[CellY * MeshWidth + CellX + 1];
		const FVector2D& P01 = MeshPositions[(CellY + 1) * MeshWidth + CellX];
		const FVector2D& P11 = MeshPositions[(CellY + 1) * MeshWidth + CellX + 1];
		return FMath::Lerp(FMath::Lerp(P00, P10, U), FMath::Lerp(P01, P11, U), V);
	}

	FVector4f FPaintSampler::MeshValue(int32 CellX, int32 CellY, double U, double V) const
	{
		if (!bMeshSmooth)
		{
			const FVector4f Top = LerpInSpace(MeshPoint(CellX, CellY), MeshPoint(CellX + 1, CellY), static_cast<float>(U), Space);
			const FVector4f Bottom = LerpInSpace(MeshPoint(CellX, CellY + 1), MeshPoint(CellX + 1, CellY + 1), static_cast<float>(U), Space);
			return LerpInSpace(Top, Bottom, static_cast<float>(V), Space);
		}

		double WU[4];
		double WV[4];
		CatmullRomWeights(U, WU);
		CatmullRomWeights(V, WV);
		FVector4f Sum(0.f, 0.f, 0.f, 0.f);
		for (int32 J = 0; J < 4; ++J)
		{
			for (int32 I = 0; I < 4; ++I)
			{
				Sum += MeshPoint(CellX - 1 + I, CellY - 1 + J) * static_cast<float>(WU[I] * WV[J]);
			}
		}
		return Sum;
	}

	bool FPaintSampler::FindMeshUV(const FVector2D& Point, int32& OutCellX, int32& OutCellY, double& OutU, double& OutV) const
	{
		const int32 CellsX = MeshWidth - 1;
		const int32 CellsY = MeshHeight - 1;

		if (bMeshRegular && Bounds.Size.X > 0.0 && Bounds.Size.Y > 0.0)
		{
			const FVector2D Unit = (Point - Bounds.Position) / Bounds.Size;
			const double GX = FMath::Clamp(Unit.X, 0.0, 1.0) * CellsX;
			const double GY = FMath::Clamp(Unit.Y, 0.0, 1.0) * CellsY;
			OutCellX = FMath::Min(FMath::FloorToInt32(GX), CellsX - 1);
			OutCellY = FMath::Min(FMath::FloorToInt32(GY), CellsY - 1);
			OutU = GX - OutCellX;
			OutV = GY - OutCellY;
			return true;
		}

		// Deformed grid: Newton on each candidate cell, keep the best (clamped) solution.
		double BestError = TNumericLimits<double>::Max();
		for (int32 CY = 0; CY < CellsY; ++CY)
		{
			for (int32 CX = 0; CX < CellsX; ++CX)
			{
				FBox2D Box(ForceInit);
				Box += MeshPositions[CY * MeshWidth + CX];
				Box += MeshPositions[CY * MeshWidth + CX + 1];
				Box += MeshPositions[(CY + 1) * MeshWidth + CX];
				Box += MeshPositions[(CY + 1) * MeshWidth + CX + 1];
				const bool bInBox = Box.ExpandBy(1.0).IsInside(Point);

				double U = 0.5;
				double V = 0.5;
				for (int32 Iteration = 0; Iteration < 12; ++Iteration)
				{
					const FVector2D F = MeshPosition(CX, CY, U, V) - Point;
					if (F.SizeSquared() < 1.0e-8)
					{
						break;
					}
					constexpr double H = 1.0e-4;
					const FVector2D DU = (MeshPosition(CX, CY, U + H, V) - MeshPosition(CX, CY, U - H, V)) / (2.0 * H);
					const FVector2D DV = (MeshPosition(CX, CY, U, V + H) - MeshPosition(CX, CY, U, V - H)) / (2.0 * H);
					const double Det = Cross(DU, DV);
					if (FMath::Abs(Det) < UE_SMALL_NUMBER)
					{
						break;
					}
					U -= (F.X * DV.Y - F.Y * DV.X) / Det;
					V -= (DU.X * F.Y - DU.Y * F.X) / Det;
					U = FMath::Clamp(U, -0.5, 1.5);
					V = FMath::Clamp(V, -0.5, 1.5);
				}

				const double ClampedU = FMath::Clamp(U, 0.0, 1.0);
				const double ClampedV = FMath::Clamp(V, 0.0, 1.0);
				double Error = FVector2D::DistSquared(MeshPosition(CX, CY, ClampedU, ClampedV), Point);
				if (!bInBox)
				{
					Error += 1.0;
				}
				if (Error < BestError)
				{
					BestError = Error;
					OutCellX = CX;
					OutCellY = CY;
					OutU = ClampedU;
					OutV = ClampedV;
				}
				if (Error < 1.0e-6)
				{
					return true;
				}
			}
		}
		return BestError < TNumericLimits<double>::Max();
	}

	FLinearColor FPaintSampler::SampleMesh(const FVector2D& LocalPoint) const
	{
		int32 CellX = 0;
		int32 CellY = 0;
		double U = 0.0;
		double V = 0.0;
		if (!FindMeshUV(LocalPoint, CellX, CellY, U, V))
		{
			return Paint.Color;
		}
		return FromSpace(MeshValue(CellX, CellY, U, V), Space);
	}

	FLinearColor FPaintSampler::Sample(const FVector2D& LocalPoint) const
	{
		switch (Paint.Type)
		{
		case EVekaPaintType::LinearGradient:
		case EVekaPaintType::RadialGradient:
		case EVekaPaintType::EllipticalGradient:
		case EVekaPaintType::AngularGradient:
			return SampleStops(GetGradientT(LocalPoint));
		case EVekaPaintType::MeshGradient:
			return MeshValues.IsEmpty() ? Paint.Color : SampleMesh(LocalPoint);
		case EVekaPaintType::None:
			return FLinearColor::Transparent;
		default:
			return Paint.Color;
		}
	}
}

// --- Renderer ----------------------------------------------------------------------------------------------

namespace Veka::PaintRender
{
	namespace
	{
		struct FMesh
		{
			TArray<FVector2D> Positions;
			/** Coverage alpha per vertex (1 inside, 0 on the outer fringe). */
			TArray<float> Coverage;
			TArray<int32> Indices;

			int32 AddVertex(const FVector2D& Position, float InCoverage = 1.f)
			{
				Coverage.Add(InCoverage);
				return Positions.Add(Position);
			}

			void AddFan(TConstArrayView<FVector2D> Polygon)
			{
				if (Polygon.Num() < 3)
				{
					return;
				}
				const int32 Base = Positions.Num();
				for (const FVector2D& Point : Polygon)
				{
					AddVertex(Point);
				}
				for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
				{
					Indices.Add(Base);
					Indices.Add(Base + Index);
					Indices.Add(Base + Index + 1);
				}
			}
		};

		double CrossZ(const FVector2D& A, const FVector2D& B)
		{
			return A.X * B.Y - A.Y * B.X;
		}

		/** Closed polygon without repeated points. */
		TArray<FVector2D> CleanPolygon(const FVekaPolyline& Polyline)
		{
			TArray<FVector2D> Out;
			Out.Reserve(Polyline.Points.Num());
			for (const FVector2D& Point : Polyline.Points)
			{
				if (Out.IsEmpty() || !Out.Last().Equals(Point, 1.0e-6))
				{
					Out.Add(Point);
				}
			}
			while (Out.Num() > 1 && Out.Last().Equals(Out[0], 1.0e-6))
			{
				Out.Pop();
			}
			return Out;
		}

		/** Makes the polygon counter-clockwise in the SignedArea convention (interior on the left of each edge). */
		void MakePositiveArea(TArray<FVector2D>& Polygon)
		{
			if (Veka::PathOps::SignedArea(Polygon) < 0.0)
			{
				Algo::Reverse(Polygon);
			}
		}

		bool IsConvex(TConstArrayView<FVector2D> Polygon)
		{
			const int32 Num = Polygon.Num();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const FVector2D& A = Polygon[Index];
				const FVector2D& B = Polygon[(Index + 1) % Num];
				const FVector2D& C = Polygon[(Index + 2) % Num];
				if (CrossZ(B - A, C - B) < -1.0e-9)
				{
					return false;
				}
			}
			return true;
		}

		/** Intersection of segment PQ with the line where Distance = 0, computed the same way for PQ and QP. */
		FVector2D Intersect(FVector2D P, FVector2D Q, double DP, double DQ)
		{
			if (P.X > Q.X || (P.X == Q.X && P.Y > Q.Y))
			{
				Swap(P, Q);
				Swap(DP, DQ);
			}
			const double T = DP / (DP - DQ);
			return P + (Q - P) * T;
		}

		/** Sutherland-Hodgman: keeps the part of Subject on the interior side of every edge of a positive-area convex Clip polygon. */
		void ClipConvex(TArray<FVector2D>& Subject, TConstArrayView<FVector2D> Clip)
		{
			TArray<FVector2D> Input;
			const int32 ClipNum = Clip.Num();
			for (int32 Edge = 0; Edge < ClipNum && Subject.Num() > 0; ++Edge)
			{
				const FVector2D& A = Clip[Edge];
				const FVector2D EdgeDir = Clip[(Edge + 1) % ClipNum] - A;
				Input = MoveTemp(Subject);
				Subject.Reset();
				const int32 Num = Input.Num();
				for (int32 Index = 0; Index < Num; ++Index)
				{
					const FVector2D& P = Input[Index];
					const FVector2D& Q = Input[(Index + 1) % Num];
					const double DP = CrossZ(EdgeDir, P - A);
					const double DQ = CrossZ(EdgeDir, Q - A);
					const bool bPIn = DP >= 0.0;
					const bool bQIn = DQ >= 0.0;
					if (bPIn)
					{
						Subject.Add(P);
					}
					if (bPIn != bQIn)
					{
						Subject.Add(Intersect(P, Q, DP, DQ));
					}
				}
			}
		}

		bool InsideConvex(const FVector2D& Point, TConstArrayView<FVector2D> Polygon)
		{
			const int32 Num = Polygon.Num();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				if (CrossZ(Polygon[(Index + 1) % Num] - Polygon[Index], Point - Polygon[Index]) < 0.0)
				{
					return false;
				}
			}
			return true;
		}

		/** Covers a convex polygon with grid cells of size Spacing (cells clipped to the polygon). */
		void AddGridCoverage(FMesh& Mesh, TConstArrayView<FVector2D> Convex, double Spacing)
		{
			FBox2D Box(ForceInit);
			for (const FVector2D& Point : Convex)
			{
				Box += Point;
			}
			// Align the grid to multiples of Spacing so neighbouring shapes and frames sample consistently.
			const int32 X0 = FMath::FloorToInt32(Box.Min.X / Spacing);
			const int32 Y0 = FMath::FloorToInt32(Box.Min.Y / Spacing);
			const int32 X1 = FMath::CeilToInt32(Box.Max.X / Spacing);
			const int32 Y1 = FMath::CeilToInt32(Box.Max.Y / Spacing);

			TArray<FVector2D> Cell;
			for (int32 Y = Y0; Y < Y1; ++Y)
			{
				for (int32 X = X0; X < X1; ++X)
				{
					const FVector2D Min(X * Spacing, Y * Spacing);
					const FVector2D Max((X + 1) * Spacing, (Y + 1) * Spacing);
					Cell.Reset();
					// TL, TR, BR, BL: positive SignedArea, same orientation as the clip polygon.
					Cell.Add(Min);
					Cell.Add(FVector2D(Max.X, Min.Y));
					Cell.Add(Max);
					Cell.Add(FVector2D(Min.X, Max.Y));
					const bool bAllInside = InsideConvex(Cell[0], Convex) && InsideConvex(Cell[1], Convex) && InsideConvex(Cell[2], Convex) && InsideConvex(Cell[3], Convex);
					if (!bAllInside)
					{
						ClipConvex(Cell, Convex);
					}
					Mesh.AddFan(Cell);
				}
			}
		}

		/** 1px fringe outside each edge: coverage goes from 1 on the edge to 0 outside. */
		void AddFringe(FMesh& Mesh, TConstArrayView<FVector2D> Polygon, double Width)
		{
			const int32 Num = Polygon.Num();
			if (Num < 3 || Width <= 0.0)
			{
				return;
			}
			const int32 Base = Mesh.Positions.Num();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const FVector2D& Prev = Polygon[(Index + Num - 1) % Num];
				const FVector2D& Point = Polygon[Index];
				const FVector2D& Next = Polygon[(Index + 1) % Num];
				const FVector2D E0 = (Point - Prev).GetSafeNormal();
				const FVector2D E1 = (Next - Point).GetSafeNormal();
				// Positive area: interior on the left, outward normal on the right (dy, -dx).
				const FVector2D N0(E0.Y, -E0.X);
				const FVector2D N1(E1.Y, -E1.X);
				FVector2D Normal = (N0 + N1).GetSafeNormal();
				if (Normal.IsNearlyZero())
				{
					Normal = N1;
				}
				const double Miter = 1.0 / FMath::Max(FVector2D::DotProduct(Normal, N1), 0.35);
				Mesh.AddVertex(Point, 1.f);
				Mesh.AddVertex(Point + Normal * Width * Miter, 0.f);
			}
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const int32 I0 = Base + Index * 2;
				const int32 I1 = Base + ((Index + 1) % Num) * 2;
				Mesh.Indices.Append({ I0, I1, I0 + 1, I1, I1 + 1, I0 + 1 });
			}
		}

		const FSlateBrush* GetWhiteBrush()
		{
			return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
		}

		FSlateVertex MakeVertex(const FSlateRenderTransform& Transform, const FVector2D& LocalPosition, const FColor& Color)
		{
			FSlateVertex Vertex;
			Vertex.Position = Transform.TransformPoint(FVector2f(LocalPosition));
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

		double GetPixelsPerUnit(const FVekaPaintContext& Context)
		{
			const FVector2f Scale = Context.Geometry.GetAccumulatedRenderTransform().GetMatrix().GetScale().GetVector();
			return FMath::Max(static_cast<double>(FMath::Max(FMath::Abs(Scale.X), FMath::Abs(Scale.Y))), 0.01);
		}
	}

	FLinearColor ApplyTint(const FVekaPaintContext& Context, const FLinearColor& Color)
	{
		FLinearColor Out = Color;
		if (const FVekaColorMatrix* Matrix = VekaEnv::GetColorMatrix(Context.Environment))
		{
			Out = Matrix->Apply(Out);
		}
		return Out * Context.WidgetStyle.GetColorAndOpacityTint();
	}


	/** Colors (sampled in the paint's space, tinted, coverage in alpha) and draws a mesh as Slate custom vertices. */
	static int32 EmitArrays(FVekaPaintContext& Context, const TArray<FVector2D>& Positions, const TArray<float>& Coverage, const TArray<int32>& MeshIndices,
		const Veka::Colora::FPaintSampler& Sampler, const FVekaPaint& Paint)
	{
		if (MeshIndices.IsEmpty())
		{
			return Context.LayerId;
		}
		FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
		const FSlateBrush* WhiteBrush = GetWhiteBrush();
		if (!Renderer || !WhiteBrush)
		{
			return Context.LayerId;
		}
		// The white brush's handle never changes for a renderer: look it up once (it was a lookup per mesh).
		static FSlateRenderer* CachedRenderer = nullptr;
		static FSlateResourceHandle CachedHandle;
		if (CachedRenderer != Renderer || !CachedHandle.IsValid())
		{
			CachedHandle = Renderer->GetResourceHandle(*WhiteBrush);
			CachedRenderer = Renderer;
		}
		const FSlateResourceHandle& Handle = CachedHandle;

		const bool bAdditive = VekaEnv::GetBlendMode(Context.Environment) == EVekaBlendMode::Add;
		const ESlateDrawEffect Effects = bAdditive ? (Context.DrawEffects | ESlateDrawEffect::PreMultipliedAlpha) : Context.DrawEffects;
		const FSlateRenderTransform& Transform = Context.Geometry.GetAccumulatedRenderTransform();
		const bool bConstant = !Paint.IsGradient();
		const FLinearColor ConstantColor = bConstant ? ApplyTint(Context, Sampler.Sample(FVector2D::ZeroVector)) : FLinearColor::Transparent;

		TArray<FSlateVertex> Vertices;
		Vertices.Reserve(Positions.Num());
		if (bConstant && !bAdditive)
		{
			// Uniform color: one sRGB conversion, then only the alpha follows the coverage.
			FColor Base = ConstantColor.ToFColor(true);
			const float Alpha = FMath::Clamp(ConstantColor.A, 0.f, 1.f) * 255.f;
			for (int32 Index = 0; Index < Positions.Num(); ++Index)
			{
				Base.A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Alpha * Coverage[Index]), 0, 255));
				Vertices.Add(MakeVertex(Transform, Positions[Index], Base));
			}
		}
		else
		{
			for (int32 Index = 0; Index < Positions.Num(); ++Index)
			{
				FLinearColor Color = bConstant ? ConstantColor : ApplyTint(Context, Sampler.Sample(Positions[Index]));
				Color.A *= Coverage[Index];
				if (bAdditive)
				{
					Color = FLinearColor(Color.R * Color.A, Color.G * Color.A, Color.B * Color.A, 0.f);
				}
				Vertices.Add(MakeVertex(Transform, Positions[Index], Color.ToFColor(true)));
			}
		}

		TArray<SlateIndex> Indices;
		Indices.SetNumUninitialized(MeshIndices.Num());
		for (int32 Index = 0; Index < MeshIndices.Num(); ++Index)
		{
			Indices[Index] = static_cast<SlateIndex>(MeshIndices[Index]);
		}
		FSlateDrawElement::MakeCustomVerts(Context.DrawElements, Context.LayerId, Handle, Vertices, Indices, nullptr, 0, 0, Effects);
		return Context.LayerId;
	}

	static int32 EmitMesh(FVekaPaintContext& Context, const FMesh& Mesh, const Veka::Colora::FPaintSampler& Sampler, const FVekaPaint& Paint)
	{
		return EmitArrays(Context, Mesh.Positions, Mesh.Coverage, Mesh.Indices, Sampler, Paint);
	}

	/**
	 * Tessellation memo: the same polylines with the same parameters give the same mesh, so a static UI tessellates
	 * once instead of every frame. Keyed by a hash of the points and parameters; unused entries are dropped.
	 */
	namespace TessellationCache
	{
		struct FEntry
		{
			FVekaTessellation Mesh;
			uint64 LastFrame = 0;
		};

		TMap<uint64, FEntry>& Get()
		{
			static TMap<uint64, FEntry> Cache;
			return Cache;
		}

		uint64 Key(const TArray<FVekaPolyline>& Polylines, std::initializer_list<double> Parameters)
		{
			uint64 Hash = 0x9E3779B97F4A7C15ull;
			for (const FVekaPolyline& Polyline : Polylines)
			{
				Hash = CityHash64WithSeed(reinterpret_cast<const char*>(Polyline.Points.GetData()), Polyline.Points.Num() * sizeof(FVector2D), Hash);
				Hash = CityHash128to64(Uint128_64(Hash, Polyline.bClosed ? 0x5bd1e995ull : 0x2127599bull));
			}
			for (const double Value : Parameters)
			{
				Hash = CityHash64WithSeed(reinterpret_cast<const char*>(&Value), sizeof(double), Hash);
			}
			return Hash;
		}

		const FVekaTessellation* Find(uint64 InKey)
		{
			if (FEntry* Entry = Get().Find(InKey))
			{
				Entry->LastFrame = GFrameCounter;
				return &Entry->Mesh;
			}
			return nullptr;
		}

		const FVekaTessellation& Add(uint64 InKey, FVekaTessellation&& Mesh)
		{
			TMap<uint64, FEntry>& Cache = Get();
			if (Cache.Num() > 8192)
			{
				// Drop what was not drawn for a few seconds.
				for (auto It = Cache.CreateIterator(); It; ++It)
				{
					if (GFrameCounter - It.Value().LastFrame > 240)
					{
						It.RemoveCurrent();
					}
				}
				if (Cache.Num() > 8192)
				{
					Cache.Reset();
				}
			}
			FEntry& Entry = Cache.Add(InKey);
			Entry.Mesh = MoveTemp(Mesh);
			Entry.LastFrame = GFrameCounter;
			return Entry.Mesh;
		}
	}

	static void BuildFill(const FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const Veka::Colora::FPaintSampler& Sampler, FMesh& Mesh)
	{
		const double PixelsPerUnit = GetPixelsPerUnit(Context);
		const double Spacing = Sampler.GetSampleSpacingPixels() / PixelsPerUnit;
		const double FringeWidth = 1.0 / PixelsPerUnit;

		for (const FVekaPolyline& Polyline : Polylines)
		{
			if (!Polyline.bClosed)
			{
				continue;
			}
			TArray<FVector2D> Polygon = CleanPolygon(Polyline);
			if (Polygon.Num() < 3)
			{
				continue;
			}
			MakePositiveArea(Polygon);

			if (Spacing > 0.0 && IsConvex(Polygon))
			{
				AddGridCoverage(Mesh, Polygon, Spacing);
			}
			else
			{
				TArray<int32> Triangles;
				if (!Veka::PathOps::Triangulate(Polygon, Triangles))
				{
					// Fan from the centroid (correct for star-shaped outlines).
					Triangles.Reset();
					FVector2D Centroid = FVector2D::ZeroVector;
					for (const FVector2D& Point : Polygon)
					{
						Centroid += Point;
					}
					Centroid /= static_cast<double>(Polygon.Num());
					const int32 Base = Mesh.AddVertex(Centroid);
					for (const FVector2D& Point : Polygon)
					{
						Mesh.AddVertex(Point);
					}
					for (int32 Index = 0; Index < Polygon.Num(); ++Index)
					{
						Mesh.Indices.Append({ Base, Base + 1 + Index, Base + 1 + (Index + 1) % Polygon.Num() });
					}
				}
				else if (Spacing > 0.0)
				{
					// Non-convex gradient fill: grid inside each (convex) triangle.
					for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
					{
						TArray<FVector2D> Triangle = { Polygon[Triangles[Index]], Polygon[Triangles[Index + 1]], Polygon[Triangles[Index + 2]] };
						MakePositiveArea(Triangle);
						AddGridCoverage(Mesh, Triangle, Spacing);
					}
				}
				else
				{
					const int32 Base = Mesh.Positions.Num();
					for (const FVector2D& Point : Polygon)
					{
						Mesh.AddVertex(Point);
					}
					for (int32 Index : Triangles)
					{
						Mesh.Indices.Add(Base + Index);
					}
				}
			}
			AddFringe(Mesh, Polygon, FringeWidth);
		}
	}

	int32 FillPolylines(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, const FVekaRect& Bounds)
	{
		if (!Paint.IsVisible() || Polylines.IsEmpty() || !FSlateApplication::IsInitialized())
		{
			return Context.LayerId;
		}
		const Veka::Colora::FPaintSampler Sampler(Paint, Bounds);
		const double PixelsPerUnit = GetPixelsPerUnit(Context);
		const uint64 Key = TessellationCache::Key(Polylines, { 1.0, Paint.IsGradient() ? Sampler.GetSampleSpacingPixels() : 0.0, PixelsPerUnit });
		const FVekaTessellation* Cached = TessellationCache::Find(Key);
		if (!Cached)
		{
			FMesh Mesh;
			BuildFill(Context, Polylines, Sampler, Mesh);
			FVekaTessellation Built;
			Built.Positions = MoveTemp(Mesh.Positions);
			Built.Coverage = MoveTemp(Mesh.Coverage);
			Built.Indices = MoveTemp(Mesh.Indices);
			Cached = &TessellationCache::Add(Key, MoveTemp(Built));
		}
		return EmitArrays(Context, Cached->Positions, Cached->Coverage, Cached->Indices, Sampler, Paint);
	}

	void TessellateFill(const FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, const FVekaRect& Bounds, FVekaTessellation& Out)
	{
		Out.Reset();
		if (!Paint.IsVisible() || Polylines.IsEmpty())
		{
			return;
		}
		const Veka::Colora::FPaintSampler Sampler(Paint, Bounds);
		FMesh Mesh;
		BuildFill(Context, Polylines, Sampler, Mesh);
		Out.Positions = MoveTemp(Mesh.Positions);
		Out.Coverage = MoveTemp(Mesh.Coverage);
		Out.Indices = MoveTemp(Mesh.Indices);
	}

	int32 DrawTessellation(FVekaPaintContext& Context, const FVekaTessellation& Mesh, const FVekaPaint& Paint, const FVekaRect& Bounds)
	{
		if (Mesh.IsEmpty() || !Paint.IsVisible() || !FSlateApplication::IsInitialized())
		{
			return Context.LayerId;
		}
		const Veka::Colora::FPaintSampler Sampler(Paint, Bounds);
		return EmitArrays(Context, Mesh.Positions, Mesh.Coverage, Mesh.Indices, Sampler, Paint);
	}

	namespace
	{
		int32 ArcSteps(double Span, double RadiusPixels)
		{
			return FMath::Clamp(FMath::CeilToInt32(FMath::Abs(Span) * FMath::Sqrt(FMath::Max(RadiusPixels, 1.0)) * 0.9), 2, 32);
		}

		/** Fan (core + fringe ring) around Center from direction U0, sweeping Span radians. */
		void AddRoundFan(FMesh& Mesh, const FVector2D& Center, const FVector2D& U0, double Span, double HalfWidth, double Fringe, double PixelsPerUnit)
		{
			const int32 Steps = ArcSteps(Span, HalfWidth * PixelsPerUnit);
			const double Start = FMath::Atan2(U0.Y, U0.X);
			const int32 CenterIndex = Mesh.AddVertex(Center, 1.f);
			int32 PrevIn = INDEX_NONE;
			int32 PrevOut = INDEX_NONE;
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const double Angle = Start + Span * Step / Steps;
				const FVector2D U(FMath::Cos(Angle), FMath::Sin(Angle));
				const int32 In = Mesh.AddVertex(Center + U * HalfWidth, 1.f);
				const int32 Out = Mesh.AddVertex(Center + U * (HalfWidth + Fringe), 0.f);
				if (PrevIn != INDEX_NONE)
				{
					Mesh.Indices.Append({ CenterIndex, PrevIn, In, PrevIn, PrevOut, Out, PrevIn, Out, In });
				}
				PrevIn = In;
				PrevOut = Out;
			}
		}

		/** Signed angle from A to B (unit vectors), in -PI..PI. */
		double SignedAngle(const FVector2D& A, const FVector2D& B)
		{
			return FMath::Atan2(CrossZ(A, B), FVector2D::DotProduct(A, B));
		}

		/** Quad strip of one segment: rows at -(h+f), -h, +h, +(h+f) across, coverage 0 1 1 0. */
		void AddSegment(FMesh& Mesh, const FVector2D& A, const FVector2D& B, const FVector2D& Normal, double HalfWidth, double Fringe,
			float StartCoverage = 1.f, float EndCoverage = 1.f)
		{
			const double Offsets[4] = { -(HalfWidth + Fringe), -HalfWidth, HalfWidth, HalfWidth + Fringe };
			const float Coverage[4] = { 0.f, 1.f, 1.f, 0.f };
			int32 RowA[4];
			int32 RowB[4];
			for (int32 Row = 0; Row < 4; ++Row)
			{
				RowA[Row] = Mesh.AddVertex(A + Normal * Offsets[Row], Coverage[Row] * StartCoverage);
				RowB[Row] = Mesh.AddVertex(B + Normal * Offsets[Row], Coverage[Row] * EndCoverage);
			}
			for (int32 Row = 0; Row < 3; ++Row)
			{
				Mesh.Indices.Append({ RowA[Row], RowB[Row], RowB[Row + 1], RowA[Row], RowB[Row + 1], RowA[Row + 1] });
			}
		}

		void AddStroke(FMesh& Mesh, TArray<FVector2D> Points, bool bClosed, double HalfWidth, double Fringe, EVekaLineCap Cap, EVekaLineJoin Join,
			double MiterLimit, double PixelsPerUnit)
		{
			if (bClosed && Points.Num() > 2)
			{
				const FVector2D First = Points[0]; // copy: Add() may reallocate
				Points.Add(First);
			}
			const int32 Num = Points.Num();
			if (Num < 2)
			{
				return;
			}

			TArray<FVector2D> Directions;
			Directions.Reserve(Num - 1);
			for (int32 Index = 0; Index + 1 < Num; ++Index)
			{
				Directions.Add((Points[Index + 1] - Points[Index]).GetSafeNormal());
			}
			const int32 NumDirections = Directions.Num();
			auto Normal = [](const FVector2D& Direction) { return FVector2D(-Direction.Y, Direction.X); };

			// Square caps extend the first / last segment; round caps add half discs; butt ends fade over the fringe.
			FVector2D Start = Points[0];
			FVector2D End = Points[Num - 1];
			const FVector2D D0 = Directions[0];
			const FVector2D D1 = Directions.Last();
			if (!bClosed && Cap == EVekaLineCap::Square)
			{
				Start -= D0 * HalfWidth;
				End += D1 * HalfWidth;
			}

			// One strip of 4 rows (fringe, core, core, fringe): consecutive segments share their rows at gentle turns
			// (curves: 4 vertices per point instead of a segment and a join each); sharp turns get a real join.
			const double Offsets[4] = { -(HalfWidth + Fringe), -HalfWidth, HalfWidth, HalfWidth + Fringe };
			const float Coverage[4] = { 0.f, 1.f, 1.f, 0.f };
			struct FRows { int32 Index[4]; };
			auto AddRows = [&](const FVector2D& P, const FVector2D& Across, double Scale)
			{
				FRows Rows;
				for (int32 Row = 0; Row < 4; ++Row)
				{
					Rows.Index[Row] = Mesh.AddVertex(P + Across * (Offsets[Row] * Scale), Coverage[Row]);
				}
				return Rows;
			};
			auto Connect = [&](const FRows& A, const FRows& B)
			{
				for (int32 Row = 0; Row < 3; ++Row)
				{
					Mesh.Indices.Append({ A.Index[Row], B.Index[Row], B.Index[Row + 1], A.Index[Row], B.Index[Row + 1], A.Index[Row + 1] });
				}
			};
			auto AddJoin = [&](const FVector2D& P, const FVector2D& In, const FVector2D& Out, double Turn)
			{
				// Joins fill the gap on the outer side of the turn.
				const double Side = Turn > 0.0 ? -1.0 : 1.0;
				const FVector2D N0 = Normal(In) * Side;
				const FVector2D N1 = Normal(Out) * Side;
				const double Span = SignedAngle(N0, N1);

				EVekaLineJoin Effective = Join;
				if (Join == EVekaLineJoin::Miter && 1.0 / FMath::Max(FMath::Cos(FMath::Abs(Turn) * 0.5), 1.0e-3) > MiterLimit)
				{
					Effective = EVekaLineJoin::Bevel;
				}
				if (Effective == EVekaLineJoin::Round)
				{
					AddRoundFan(Mesh, P, N0, Span, HalfWidth, Fringe, PixelsPerUnit);
					return;
				}
				const int32 C = Mesh.AddVertex(P, 1.f);
				const int32 A = Mesh.AddVertex(P + N0 * HalfWidth, 1.f);
				const int32 B = Mesh.AddVertex(P + N1 * HalfWidth, 1.f);
				const int32 AO = Mesh.AddVertex(P + N0 * (HalfWidth + Fringe), 0.f);
				const int32 BO = Mesh.AddVertex(P + N1 * (HalfWidth + Fringe), 0.f);
				if (Effective == EVekaLineJoin::Miter)
				{
					const FVector2D Bisector = (N0 + N1).GetSafeNormal();
					const double Length = HalfWidth / FMath::Max(FVector2D::DotProduct(Bisector, N0), 1.0e-3);
					const int32 M = Mesh.AddVertex(P + Bisector * Length, 1.f);
					const int32 MO = Mesh.AddVertex(P + Bisector * (Length + Fringe), 0.f);
					Mesh.Indices.Append({ C, A, M, C, M, B, A, AO, MO, A, MO, M, M, MO, BO, M, BO, B });
				}
				else
				{
					Mesh.Indices.Append({ C, A, B, A, AO, BO, A, BO, B });
				}
			};

			const double SmoothLimit = FMath::DegreesToRadians(25.0);
			FRows Previous;
			FRows FirstRows;
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const bool bFirst = Index == 0;
				const bool bLast = Index == Num - 1;
				if (!bClosed && (bFirst || bLast))
				{
					const FRows Rows = AddRows(bFirst ? Start : End, Normal(bFirst ? D0 : D1), 1.0);
					if (!bFirst)
					{
						Connect(Previous, Rows);
					}
					Previous = Rows;
					continue;
				}

				// Interior point (for a closed outline, the first and last points are the same corner).
				const FVector2D P = Points[Index];
				const FVector2D In = Directions[(Index - 1 + NumDirections) % NumDirections];
				const FVector2D Out = Directions[Index % NumDirections];
				const double Turn = SignedAngle(In, Out);
				if (FMath::Abs(Turn) < SmoothLimit)
				{
					if (bClosed && bLast)
					{
						Connect(Previous, FirstRows);
						continue;
					}
					const FVector2D Bisector = (Normal(In) + Normal(Out)).GetSafeNormal();
					const double Scale = 1.0 / FMath::Max(FVector2D::DotProduct(Bisector, Normal(In)), 0.5);
					const FRows Rows = AddRows(P, Bisector, Scale);
					if (bClosed && bFirst)
					{
						FirstRows = Rows;
					}
					else
					{
						Connect(Previous, Rows);
					}
					Previous = Rows;
					continue;
				}

				// Sharp turn: the strip ends square on the incoming side, a join fills the outside, a new strip starts.
				if (!(bClosed && bFirst))
				{
					Connect(Previous, AddRows(P, Normal(In), 1.0));
					AddJoin(P, In, Out, Turn);
				}
				if (!(bClosed && bLast))
				{
					const FRows Rows = AddRows(P, Normal(Out), 1.0);
					if (bClosed && bFirst)
					{
						FirstRows = Rows;
					}
					Previous = Rows;
				}
			}

			if (bClosed)
			{
				return;
			}
			if (Cap == EVekaLineCap::Round)
			{
				AddRoundFan(Mesh, Start, FVector2D(-D0.Y, D0.X), UE_PI, HalfWidth, Fringe, PixelsPerUnit);
				AddRoundFan(Mesh, End, FVector2D(D1.Y, -D1.X), UE_PI, HalfWidth, Fringe, PixelsPerUnit);
			}
			else
			{
				AddSegment(Mesh, Start - D0 * Fringe, Start, FVector2D(-D0.Y, D0.X), HalfWidth, Fringe, 0.f, 1.f);
				AddSegment(Mesh, End, End + D1 * Fringe, FVector2D(-D1.Y, D1.X), HalfWidth, Fringe, 1.f, 0.f);
			}
		}
	}

	static void BuildStroke(const FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, const Veka::Colora::FPaintSampler& Sampler,
		float Width, EVekaLineCap Cap, EVekaLineJoin Join, float MiterLimit, FMesh& Mesh)
	{
		const double PixelsPerUnit = GetPixelsPerUnit(Context);
		const double Fringe = 1.0 / PixelsPerUnit;
		double HalfWidth = Width * 0.5;
		float Thin = 1.f;
		if (Width * PixelsPerUnit < 1.0)
		{
			// Hairlines: keep a 1px core and fade it instead of thinning below a pixel.
			Thin = static_cast<float>(Width * PixelsPerUnit);
			HalfWidth = 0.5 / PixelsPerUnit;
		}
		const double Spacing = Paint.IsGradient() ? Sampler.GetSampleSpacingPixels() / PixelsPerUnit : 0.0;

		for (const FVekaPolyline& Polyline : Polylines)
		{
			// Repeated points (joints of Bezier segments) make zero-length segments.
			TArray<FVector2D> Points;
			if (Polyline.bClosed)
			{
				Points = CleanPolygon(Polyline);
			}
			else
			{
				for (const FVector2D& Point : Polyline.Points)
				{
					if (Points.IsEmpty() || !Points.Last().Equals(Point, 1.0e-6))
					{
						Points.Add(Point);
					}
				}
			}
			if (Spacing > 0.0 && Points.Num() > 1)
			{
				// Subdivide long segments so the color follows the gradient.
				TArray<FVector2D> Dense;
				for (int32 Index = 0; Index < Points.Num(); ++Index)
				{
					if (Index > 0)
					{
						const FVector2D A = Points[Index - 1];
						const FVector2D B = Points[Index];
						const int32 Steps = FMath::Min(FMath::FloorToInt32(FVector2D::Distance(A, B) / Spacing), 256);
						for (int32 Step = 1; Step <= Steps; ++Step)
						{
							Dense.Add(FMath::Lerp(A, B, static_cast<double>(Step) / (Steps + 1)));
						}
					}
					Dense.Add(Points[Index]);
				}
				Points = MoveTemp(Dense);
			}
			if (Points.Num() == 1)
			{
				if (Cap == EVekaLineCap::Round)
				{
					AddRoundFan(Mesh, Points[0], FVector2D(1.0, 0.0), UE_TWO_PI, HalfWidth, Fringe, PixelsPerUnit);
				}
				continue;
			}
			AddStroke(Mesh, MoveTemp(Points), Polyline.bClosed, HalfWidth, Fringe, Cap, Join, MiterLimit, PixelsPerUnit);
		}
		if (Thin < 1.f)
		{
			for (float& Coverage : Mesh.Coverage)
			{
				Coverage *= Thin;
			}
		}
	}

	int32 StrokePolylines(FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, float Width, const FVekaRect& Bounds,
		EVekaLineCap Cap, EVekaLineJoin Join, float MiterLimit)
	{
		if (!Paint.IsVisible() || Width <= 0.f || Polylines.IsEmpty() || !FSlateApplication::IsInitialized())
		{
			return Context.LayerId;
		}
		const Veka::Colora::FPaintSampler Sampler(Paint, Bounds);
		const double PixelsPerUnit = GetPixelsPerUnit(Context);
		const uint64 Key = TessellationCache::Key(Polylines, { 2.0, Paint.IsGradient() ? Sampler.GetSampleSpacingPixels() : 0.0, PixelsPerUnit, Width,
			static_cast<double>(Cap), static_cast<double>(Join), MiterLimit });
		const FVekaTessellation* Cached = TessellationCache::Find(Key);
		if (!Cached)
		{
			FMesh Mesh;
			BuildStroke(Context, Polylines, Paint, Sampler, Width, Cap, Join, MiterLimit, Mesh);
			FVekaTessellation Built;
			Built.Positions = MoveTemp(Mesh.Positions);
			Built.Coverage = MoveTemp(Mesh.Coverage);
			Built.Indices = MoveTemp(Mesh.Indices);
			Cached = &TessellationCache::Add(Key, MoveTemp(Built));
		}
		return EmitArrays(Context, Cached->Positions, Cached->Coverage, Cached->Indices, Sampler, Paint);
	}

	void TessellateStroke(const FVekaPaintContext& Context, const TArray<FVekaPolyline>& Polylines, const FVekaPaint& Paint, float Width, const FVekaRect& Bounds,
		EVekaLineCap Cap, EVekaLineJoin Join, float MiterLimit, FVekaTessellation& Out)
	{
		Out.Reset();
		if (!Paint.IsVisible() || Width <= 0.f || Polylines.IsEmpty())
		{
			return;
		}
		const Veka::Colora::FPaintSampler Sampler(Paint, Bounds);
		FMesh Mesh;
		BuildStroke(Context, Polylines, Paint, Sampler, Width, Cap, Join, MiterLimit, Mesh);
		Out.Positions = MoveTemp(Mesh.Positions);
		Out.Coverage = MoveTemp(Mesh.Coverage);
		Out.Indices = MoveTemp(Mesh.Indices);
	}
}
