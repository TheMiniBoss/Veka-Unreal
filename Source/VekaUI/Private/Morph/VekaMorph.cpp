// Copyright TheMiniBoss. All Rights Reserved.

#include "Morph/VekaMorph.h"
#include "Core/VekaNode.h"
#include "Core/VekaEnvironment.h"
#include "Colora/VekaTheme.h"
#include "Colora/VekaPaintRenderer.h"
#include "Primitives/VekaShapeView.h"
#include "Hash/CityHash.h"

// --- Shapes --------------------------------------------------------------------------------------------------

namespace
{
	double PolygonSignedDistance(TConstArrayView<FVector2D> Polygon, const FVector2D& Point)
	{
		double Best = TNumericLimits<double>::Max();
		bool bInside = false;
		for (int32 I = 0, J = Polygon.Num() - 1; I < Polygon.Num(); J = I++)
		{
			const FVector2D& A = Polygon[J];
			const FVector2D& B = Polygon[I];
			const FVector2D Edge = B - A;
			const double T = FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / FMath::Max(Edge.SizeSquared(), 1e-12), 0.0, 1.0);
			Best = FMath::Min(Best, FVector2D::DistSquared(Point, A + Edge * T));
			if ((A.Y > Point.Y) != (B.Y > Point.Y) && Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		const double Distance = FMath::Sqrt(Best);
		return bInside ? -Distance : Distance;
	}
}

double FVekaMorphShape::SignedDistance(const FVector2D& Point) const
{
	FVector2D Local = Point - Origin;
	if (bTransformed)
	{
		Local = Pivot + FVector2D(InverseTransform.TransformPoint(FVector2f(Local - Pivot)));
	}
	const double Distance = Polygon.Num() >= 3 ? PolygonSignedDistance(Polygon, Local) : Shape.SignedDistance(Local, Bounds);
	return Distance * DistanceScale;
}

FVekaRect FVekaMorphShape::GetGroupBounds() const
{
	if (!bTransformed)
	{
		return FVekaRect(Bounds.Position + Origin, Bounds.Size);
	}
	// Forward transform of the corners (inverse of the stored inverse).
	const FSlateRenderTransform Forward = InverseTransform.Inverse();
	const FVector2D Corners[4] = { Bounds.Position, Bounds.Position + FVector2D(Bounds.Size.X, 0.0), Bounds.Position + Bounds.Size, Bounds.Position + FVector2D(0.0, Bounds.Size.Y) };
	FVector2D Min(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
	FVector2D Max = -Min;
	for (const FVector2D& Corner : Corners)
	{
		const FVector2D Moved = Origin + Pivot + FVector2D(Forward.TransformPoint(FVector2f(Corner - Pivot)));
		Min = Min.ComponentMin(Moved);
		Max = Max.ComponentMax(Moved);
	}
	return FVekaRect(Min, Max - Min);
}

// --- Field and contour -----------------------------------------------------------------------------------------

namespace Veka::Morph
{
	double SmoothMin(double A, double B, double K)
	{
		if (K <= 0.0)
		{
			return FMath::Min(A, B);
		}
		const double H = FMath::Clamp(0.5 + 0.5 * (B - A) / K, 0.0, 1.0);
		return FMath::Lerp(B, A, H) - K * H * (1.0 - H);
	}

	double Field(TConstArrayView<FVekaMorphShape> Shapes, double Smoothness, const FVector2D& Point)
	{
		if (Shapes.IsEmpty())
		{
			return 1.0;
		}
		double Distance = Shapes[0].SignedDistance(Point);
		for (int32 Index = 1; Index < Shapes.Num(); ++Index)
		{
			Distance = SmoothMin(Distance, Shapes[Index].SignedDistance(Point), Smoothness);
		}
		return Distance;
	}

	void ExtractContour(TFunctionRef<double(const FVector2D&)> InField, const FVekaRect& Area, double CellSize, TArray<FVekaPolyline>& OutLoops)
	{
		OutLoops.Reset();
		if (Area.Size.X <= 0.0 || Area.Size.Y <= 0.0 || CellSize <= 0.0)
		{
			return;
		}
		const int32 NX = FMath::Clamp(FMath::CeilToInt32(Area.Size.X / CellSize), 1, 400);
		const int32 NY = FMath::Clamp(FMath::CeilToInt32(Area.Size.Y / CellSize), 1, 400);
		const double SX = Area.Size.X / NX;
		const double SY = Area.Size.Y / NY;
		const int32 Row = NX + 1;

		TArray<double> Values;
		Values.SetNumUninitialized(Row * (NY + 1));
		for (int32 J = 0; J <= NY; ++J)
		{
			for (int32 I = 0; I <= NX; ++I)
			{
				// The border is forced outside so every loop closes inside the area.
				const bool bBorder = I == 0 || J == 0 || I == NX || J == NY;
				const double Value = InField(Area.Position + FVector2D(I * SX, J * SY));
				Values[J * Row + I] = bBorder ? FMath::Max(Value, 1e-3) : Value;
			}
		}
		auto At = [&](int32 I, int32 J) { return Values[J * Row + I]; };
		auto Corner = [&](int32 I, int32 J) { return Area.Position + FVector2D(I * SX, J * SY); };

		// Edge ids: horizontal edges (I,J)-(I+1,J) first, then vertical edges (I,J)-(I,J+1).
		const int32 HorizontalCount = NX * (NY + 1);
		auto HId = [&](int32 I, int32 J) { return J * NX + I; };
		auto VId = [&](int32 I, int32 J) { return HorizontalCount + J * Row + I; };

		TMap<int32, FVector2D> Crossings;
		auto Crossing = [&](int32 Id, int32 I0, int32 J0, int32 I1, int32 J1)
		{
			if (!Crossings.Contains(Id))
			{
				const double A = At(I0, J0);
				const double B = At(I1, J1);
				const double T = FMath::Clamp(A / (A - B), 0.0, 1.0);
				Crossings.Add(Id, FMath::Lerp(Corner(I0, J0), Corner(I1, J1), T));
			}
			return Id;
		};

		TArray<TPair<int32, int32>> Segments;
		for (int32 J = 0; J < NY; ++J)
		{
			for (int32 I = 0; I < NX; ++I)
			{
				const bool In0 = At(I, J) < 0.0;
				const bool In1 = At(I + 1, J) < 0.0;
				const bool In2 = At(I + 1, J + 1) < 0.0;
				const bool In3 = At(I, J + 1) < 0.0;
				const int32 Case = (In0 ? 1 : 0) | (In1 ? 2 : 0) | (In2 ? 4 : 0) | (In3 ? 8 : 0);
				if (Case == 0 || Case == 15)
				{
					continue;
				}
				auto E0 = [&]() { return Crossing(HId(I, J), I, J, I + 1, J); };         // top
				auto E1 = [&]() { return Crossing(VId(I + 1, J), I + 1, J, I + 1, J + 1); }; // right
				auto E2 = [&]() { return Crossing(HId(I, J + 1), I, J + 1, I + 1, J + 1); }; // bottom
				auto E3 = [&]() { return Crossing(VId(I, J), I, J, I, J + 1); };         // left
				auto Add = [&](int32 A, int32 B) { Segments.Emplace(A, B); };
				const bool bCenterInside = (At(I, J) + At(I + 1, J) + At(I + 1, J + 1) + At(I, J + 1)) < 0.0;
				switch (Case)
				{
				case 1: Add(E3(), E0()); break;
				case 2: Add(E0(), E1()); break;
				case 3: Add(E3(), E1()); break;
				case 4: Add(E1(), E2()); break;
				case 5:
					if (bCenterInside) { Add(E0(), E1()); Add(E2(), E3()); }
					else { Add(E3(), E0()); Add(E1(), E2()); }
					break;
				case 6: Add(E0(), E2()); break;
				case 7: Add(E3(), E2()); break;
				case 8: Add(E2(), E3()); break;
				case 9: Add(E0(), E2()); break;
				case 10:
					if (bCenterInside) { Add(E3(), E0()); Add(E1(), E2()); }
					else { Add(E0(), E1()); Add(E2(), E3()); }
					break;
				case 11: Add(E1(), E2()); break;
				case 12: Add(E1(), E3()); break;
				case 13: Add(E0(), E1()); break;
				case 14: Add(E0(), E3()); break;
				default: break;
				}
			}
		}

		// Chain the segments into loops through their shared crossings.
		TMap<int32, TArray<int32, TInlineAllocator<2>>> ByEdge;
		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			ByEdge.FindOrAdd(Segments[Index].Key).Add(Index);
			ByEdge.FindOrAdd(Segments[Index].Value).Add(Index);
		}
		TBitArray<> Used(false, Segments.Num());
		for (int32 Start = 0; Start < Segments.Num(); ++Start)
		{
			if (Used[Start])
			{
				continue;
			}
			Used[Start] = true;
			FVekaPolyline Loop;
			Loop.bClosed = true;
			const int32 FirstEdge = Segments[Start].Key;
			int32 Edge = Segments[Start].Value;
			Loop.Points.Add(Crossings[FirstEdge]);
			Loop.Points.Add(Crossings[Edge]);
			int32 Current = Start;
			while (Edge != FirstEdge)
			{
				int32 Next = INDEX_NONE;
				for (const int32 Candidate : ByEdge[Edge])
				{
					if (Candidate != Current && !Used[Candidate])
					{
						Next = Candidate;
						break;
					}
				}
				if (Next == INDEX_NONE)
				{
					break;
				}
				Used[Next] = true;
				Edge = Segments[Next].Key == Edge ? Segments[Next].Value : Segments[Next].Key;
				Current = Next;
				if (Edge != FirstEdge)
				{
					Loop.Points.Add(Crossings[Edge]);
				}
			}
			if (Loop.Points.Num() >= 3)
			{
				OutLoops.Add(MoveTemp(Loop));
			}
		}
	}
}

// --- Shape group ---------------------------------------------------------------------------------------------

namespace
{
	struct FShapeGroupState : public FVekaPrimitiveState
	{
		uint64 Key = 0;
		TArray<FVekaPolyline> Loops;
	};

	void Capture(FVekaNode& Node, const FVector2D& ParentOrigin, const FVekaEnvironment& Environment, TArray<FVekaMorphShape>& Out, FVekaPaint& OutFill, bool& bOutForeground)
	{
		const FVector2D Origin = ParentOrigin + Node.GetArrangedRect().Position;
		// Arranged now (the node's own widget arranges later in the frame): content rect and children for this frame.
		Node.Arrange(Node.GetArrangedRect().Size);

		if (const FVekaShapeView* View = Node.GetPrimitiveAs<FVekaShapeView>())
		{
			FVekaMorphShape& Shape = Out.AddDefaulted_GetRef();
			Shape.Shape = View->Shape;
			Shape.Bounds = Node.GetContentRect();
			Shape.Origin = Origin;
			if (Node.HasRenderTransform())
			{
				const FVector2D Size = Node.GetArrangedRect().Size;
				const FSlateRenderTransform Transform = Node.ComputeRenderTransform(Size);
				Shape.bTransformed = true;
				Shape.InverseTransform = Transform.Inverse();
				Shape.Pivot = Size * 0.5;
				const FMatrix2x2f Matrix = Transform.GetMatrix();
				float A, B, C, D;
				Matrix.GetMatrix(A, B, C, D);
				Shape.DistanceScale = FMath::Sqrt(FMath::Max(FMath::Abs(A * D - B * C), 1e-6f));
			}
			if (View->Shape.Kind == EVekaShapeKind::Path)
			{
				TArray<FVekaPolyline> Polylines;
				View->Shape.MakePath(Shape.Bounds).Flatten(Polylines, 0.5);
				for (const FVekaPolyline& Polyline : Polylines)
				{
					Shape.Polygon.Append(Polyline.Points);
				}
			}
			if (Out.Num() == 1)
			{
				bOutForeground = View->bFillWithForeground || !View->bFill;
				OutFill = View->Fill;
			}
			return;
		}
		for (const TSharedRef<FVekaNode>& Child : Node.GetChildren())
		{
			Capture(*Child, Origin, Environment, Out, OutFill, bOutForeground);
		}
	}

	uint64 HashShapes(TConstArrayView<FVekaMorphShape> Shapes, double Smoothness, double CellSize)
	{
		TArray<double> Data;
		Data.Reserve(Shapes.Num() * 20 + 2);
		Data.Add(Smoothness);
		Data.Add(CellSize);
		for (const FVekaMorphShape& Shape : Shapes)
		{
			const FVekaCornerRadii& R = Shape.Shape.CornerRadii;
			Data.Append({ static_cast<double>(Shape.Shape.Kind), R.TopLeft, R.TopRight, R.BottomRight, R.BottomLeft, Shape.Shape.bContinuousCorners ? 1.0 : 0.0,
				Shape.Bounds.Position.X, Shape.Bounds.Position.Y, Shape.Bounds.Size.X, Shape.Bounds.Size.Y, Shape.Origin.X, Shape.Origin.Y,
				static_cast<double>(Shape.Polygon.Num()) });
			if (Shape.bTransformed)
			{
				float A, B, C, D;
				Shape.InverseTransform.GetMatrix().GetMatrix(A, B, C, D);
				const FVector2D T(Shape.InverseTransform.GetTranslation());
				Data.Append({ A, B, C, D, T.X, T.Y });
			}
			for (const FVector2D& Point : Shape.Polygon)
			{
				Data.Append({ Point.X, Point.Y });
			}
		}
		return CityHash64(reinterpret_cast<const char*>(Data.GetData()), Data.Num() * sizeof(double));
	}
}

TSharedPtr<FVekaPrimitiveState> FVekaShapeGroup::CreateState() const
{
	return MakeShared<FShapeGroupState>();
}

void FVekaShapeGroup::PlaceChildren(const FVekaRect& Bounds, FVekaLayoutContext& Context, TConstArrayView<FVekaLayoutChild> Children) const
{
	FVekaPrimitive::PlaceChildren(Bounds, Context, Children);

	CapturedShapes.Reset();
	CapturedFill = FVekaPaint();
	bCapturedForeground = true;
	CapturedContentOffset = Bounds.Position;
	for (const FVekaLayoutChild& Child : Children)
	{
		Capture(Child.GetNode(), FVector2D::ZeroVector, Context.Environment, CapturedShapes, CapturedFill, bCapturedForeground);
	}
}

int32 FVekaShapeGroup::Paint(FVekaPaintContext& Context, const FVekaRect& Bounds) const
{
	if (CapturedShapes.IsEmpty() || !Context.State)
	{
		return Context.LayerId;
	}
	FShapeGroupState& State = static_cast<FShapeGroupState&>(*Context.State);

	const FVector2f Scale = Context.Geometry.GetAccumulatedRenderTransform().GetMatrix().GetScale().GetVector();
	const double PixelsPerUnit = FMath::Max(static_cast<double>(FMath::Max(FMath::Abs(Scale.X), FMath::Abs(Scale.Y))), 0.01);
	const double CellSize = 1.5 / PixelsPerUnit;
	const uint64 Key = HashShapes(CapturedShapes, Smoothness, CellSize);
	if (Key != State.Key)
	{
		// The outline only changes when a shape moves or resizes: contour extracted then, reused otherwise.
		FVekaRect Area = CapturedShapes[0].GetGroupBounds();
		FVector2D Min = Area.Position;
		FVector2D Max = Area.Position + Area.Size;
		for (const FVekaMorphShape& Shape : CapturedShapes)
		{
			const FVekaRect ShapeBounds = Shape.GetGroupBounds();
			Min = Min.ComponentMin(ShapeBounds.Position);
			Max = Max.ComponentMax(ShapeBounds.Position + ShapeBounds.Size);
		}
		const double Margin = Smoothness + 3.0 * CellSize;
		Area = FVekaRect(Min - FVector2D(Margin, Margin), Max - Min + FVector2D(Margin, Margin) * 2.0);
		const TArray<FVekaMorphShape>& Shapes = CapturedShapes;
		const double K = Smoothness;
		Veka::Morph::ExtractContour([&Shapes, K](const FVector2D& Point) { return Veka::Morph::Field(Shapes, K, Point); }, Area, CellSize, State.Loops);
		State.Key = Key;
	}

	FVekaPaint Paint;
	if (bOverrideFill)
	{
		Paint = Fill;
	}
	else if (bCapturedForeground)
	{
		Paint = FVekaPaint(VekaEnv::GetForegroundColor(Context.Environment));
		if (const FVekaPaint* Foreground = Context.Environment.Find<FVekaPaint>(VekaEnv::ForegroundPaint()))
		{
			Paint = *Foreground;
		}
	}
	else
	{
		Paint = CapturedFill;
	}
	Paint = Veka::Colora::Resolve(Paint, Context.Environment);
	return Veka::PaintRender::FillPolylines(Context, State.Loops, Paint, Bounds);
}

bool FVekaShapeGroup::HitTest(const FVector2D& LocalPoint, const FVector2D& Size) const
{
	if (CapturedShapes.IsEmpty())
	{
		return FVekaPrimitive::HitTest(LocalPoint, Size);
	}
	return Veka::Morph::Field(CapturedShapes, Smoothness, LocalPoint + CapturedContentOffset) <= 0.0;
}

void FVekaShapeGroupScopeModifier::ModifyEnvironment(FVekaEnvironment& Environment) const
{
	Environment.Set<bool>(VekaEnv::InShapeGroup(), true);
}

bool VekaEnv::IsInShapeGroup(const FVekaEnvironment& Environment)
{
	return Environment.Get<bool>(InShapeGroup(), false);
}

namespace Veka
{
	FVekaElement FShapeGroupBuilder::Children(TArray<FVekaElement> InChildren) const
	{
		return FVekaElement::MakePrimitive(FInstancedStruct::Make(Group), MoveTemp(InChildren)).Modifier(FVekaShapeGroupScopeModifier());
	}

	FShapeGroupBuilder ShapeGroup(float Smoothness)
	{
		FVekaShapeGroup Group;
		Group.Smoothness = FMath::Max(Smoothness, 0.f);
		return FShapeGroupBuilder(Group);
	}

	FShapeGroupBuilder ShapeGroup(float Smoothness, const FVekaPaint& Fill)
	{
		FVekaShapeGroup Group;
		Group.Smoothness = FMath::Max(Smoothness, 0.f);
		Group.bOverrideFill = true;
		Group.Fill = Fill;
		return FShapeGroupBuilder(Group);
	}
}
