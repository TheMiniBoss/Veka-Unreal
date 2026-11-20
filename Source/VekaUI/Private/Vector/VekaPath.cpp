// Copyright TheMiniBoss. All Rights Reserved.

#include "Vector/VekaPath.h"
#include "Algo/Reverse.h"

namespace
{
	/** Circle-arc cubic handle length. */
	constexpr double Kappa = 0.5522847498307936;

	FVekaPathCommand MakeCommand(EVekaPathVerb Verb, const FVector2D& P0 = FVector2D::ZeroVector, const FVector2D& P1 = FVector2D::ZeroVector, const FVector2D& P2 = FVector2D::ZeroVector)
	{
		FVekaPathCommand Command;
		Command.Verb = Verb;
		Command.P0 = P0;
		Command.P1 = P1;
		Command.P2 = P2;
		return Command;
	}

	int32 SegmentCount(double ApproxLength, double Tolerance)
	{
		const double SafeTolerance = FMath::Max(Tolerance, 0.01);
		return FMath::Clamp(FMath::CeilToInt32(FMath::Sqrt(ApproxLength / SafeTolerance) * 1.5), 2, 256);
	}

	double Angle(const FVector2D& U, const FVector2D& V)
	{
		return FMath::Atan2(U.X * V.Y - U.Y * V.X, U.X * V.X + U.Y * V.Y);
	}
}

// --- Building ---------------------------------------------------------------------------------------

FVector2D FVekaPath::GetCurrentPoint() const
{
	for (int32 Index = Commands.Num() - 1; Index >= 0; --Index)
	{
		const FVekaPathCommand& Command = Commands[Index];
		switch (Command.Verb)
		{
		case EVekaPathVerb::MoveTo:
		case EVekaPathVerb::LineTo:
			return Command.P0;
		case EVekaPathVerb::QuadTo:
			return Command.P1;
		case EVekaPathVerb::CubicTo:
			return Command.P2;
		case EVekaPathVerb::Close:
			return GetSubpathStart();
		}
	}
	return FVector2D::ZeroVector;
}

FVector2D FVekaPath::GetSubpathStart() const
{
	for (int32 Index = Commands.Num() - 1; Index >= 0; --Index)
	{
		if (Commands[Index].Verb == EVekaPathVerb::MoveTo)
		{
			return Commands[Index].P0;
		}
	}
	return FVector2D::ZeroVector;
}

FVekaPath& FVekaPath::MoveTo(const FVector2D& Point)
{
	Commands.Add(MakeCommand(EVekaPathVerb::MoveTo, Point));
	return *this;
}

FVekaPath& FVekaPath::LineTo(const FVector2D& Point)
{
	if (Commands.IsEmpty())
	{
		MoveTo(FVector2D::ZeroVector);
	}
	Commands.Add(MakeCommand(EVekaPathVerb::LineTo, Point));
	return *this;
}

FVekaPath& FVekaPath::QuadTo(const FVector2D& Control, const FVector2D& End)
{
	if (Commands.IsEmpty())
	{
		MoveTo(FVector2D::ZeroVector);
	}
	Commands.Add(MakeCommand(EVekaPathVerb::QuadTo, Control, End));
	return *this;
}

FVekaPath& FVekaPath::CubicTo(const FVector2D& Control1, const FVector2D& Control2, const FVector2D& End)
{
	if (Commands.IsEmpty())
	{
		MoveTo(FVector2D::ZeroVector);
	}
	Commands.Add(MakeCommand(EVekaPathVerb::CubicTo, Control1, Control2, End));
	return *this;
}

FVekaPath& FVekaPath::Close()
{
	if (!Commands.IsEmpty() && Commands.Last().Verb != EVekaPathVerb::Close)
	{
		Commands.Add(MakeCommand(EVekaPathVerb::Close));
	}
	return *this;
}

FVekaPath& FVekaPath::ArcTo(double RadiusX, double RadiusY, double XAxisRotationDegrees, bool bLargeArc, bool bSweep, const FVector2D& End)
{
	// SVG implementation notes F.6.5 / F.6.6: endpoint -> center parameterization, then <= 90° cubic segments.
	const FVector2D Start = GetCurrentPoint();
	if (Start.Equals(End, 1.0e-9))
	{
		return *this;
	}

	double Rx = FMath::Abs(RadiusX);
	double Ry = FMath::Abs(RadiusY);
	if (Rx < 1.0e-9 || Ry < 1.0e-9)
	{
		return LineTo(End);
	}

	const double Phi = FMath::DegreesToRadians(XAxisRotationDegrees);
	const double CosPhi = FMath::Cos(Phi);
	const double SinPhi = FMath::Sin(Phi);

	const double Dx2 = (Start.X - End.X) * 0.5;
	const double Dy2 = (Start.Y - End.Y) * 0.5;
	const double X1p = CosPhi * Dx2 + SinPhi * Dy2;
	const double Y1p = -SinPhi * Dx2 + CosPhi * Dy2;

	const double Lambda = (X1p * X1p) / (Rx * Rx) + (Y1p * Y1p) / (Ry * Ry);
	if (Lambda > 1.0)
	{
		const double Scale = FMath::Sqrt(Lambda);
		Rx *= Scale;
		Ry *= Scale;
	}

	const double Rx2 = Rx * Rx;
	const double Ry2 = Ry * Ry;
	const double Numerator = Rx2 * Ry2 - Rx2 * Y1p * Y1p - Ry2 * X1p * X1p;
	const double Denominator = Rx2 * Y1p * Y1p + Ry2 * X1p * X1p;
	double Coef = Denominator > 0.0 ? FMath::Sqrt(FMath::Max(0.0, Numerator / Denominator)) : 0.0;
	if (bLargeArc == bSweep)
	{
		Coef = -Coef;
	}

	const double Cxp = Coef * (Rx * Y1p / Ry);
	const double Cyp = Coef * (-Ry * X1p / Rx);
	const double Cx = CosPhi * Cxp - SinPhi * Cyp + (Start.X + End.X) * 0.5;
	const double Cy = SinPhi * Cxp + CosPhi * Cyp + (Start.Y + End.Y) * 0.5;

	const FVector2D U((X1p - Cxp) / Rx, (Y1p - Cyp) / Ry);
	const FVector2D V((-X1p - Cxp) / Rx, (-Y1p - Cyp) / Ry);
	const double Theta1 = Angle(FVector2D(1.0, 0.0), U);
	double DeltaTheta = Angle(U, V);
	if (!bSweep && DeltaTheta > 0.0)
	{
		DeltaTheta -= UE_DOUBLE_TWO_PI;
	}
	else if (bSweep && DeltaTheta < 0.0)
	{
		DeltaTheta += UE_DOUBLE_TWO_PI;
	}

	const int32 Segments = FMath::Max(1, FMath::CeilToInt32(FMath::Abs(DeltaTheta) / UE_DOUBLE_HALF_PI - 1.0e-9));
	const double Delta = DeltaTheta / Segments;
	const double T = 4.0 / 3.0 * FMath::Tan(Delta / 4.0);

	auto Map = [&](double X, double Y)
	{
		return FVector2D(Cx + Rx * X * CosPhi - Ry * Y * SinPhi, Cy + Rx * X * SinPhi + Ry * Y * CosPhi);
	};

	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const double A1 = Theta1 + Segment * Delta;
		const double A2 = A1 + Delta;
		const double Cos1 = FMath::Cos(A1), Sin1 = FMath::Sin(A1);
		const double Cos2 = FMath::Cos(A2), Sin2 = FMath::Sin(A2);

		const FVector2D C1 = Map(Cos1 - T * Sin1, Sin1 + T * Cos1);
		const FVector2D C2 = Map(Cos2 + T * Sin2, Sin2 - T * Cos2);
		const FVector2D P = Segment == Segments - 1 ? End : Map(Cos2, Sin2);
		CubicTo(C1, C2, P);
	}
	return *this;
}

FVekaPath& FVekaPath::AddRect(const FVekaRect& Rect)
{
	MoveTo(Rect.Position);
	LineTo(FVector2D(Rect.Right(), Rect.Top()));
	LineTo(FVector2D(Rect.Right(), Rect.Bottom()));
	LineTo(FVector2D(Rect.Left(), Rect.Bottom()));
	return Close();
}

FVekaPath& FVekaPath::AddRoundedRect(const FVekaRect& Rect, const FVekaCornerRadii& InRadii, bool bContinuous)
{
	const FVekaCornerRadii Radii = InRadii.ClampedTo(Rect.Size);
	if (Radii.IsZero())
	{
		return AddRect(Rect);
	}

	const double L = Rect.Left(), T = Rect.Top(), R = Rect.Right(), B = Rect.Bottom();
	const double HalfMin = FMath::Min(Rect.Size.X, Rect.Size.Y) * 0.5;

	// Circular corner: extent = r, handles at Kappa·r.
	// Continuous corner: extent = 1.528·r (capped), handles end at 0.178·extent from the corner, so the
	// curve leaves the edge with almost zero curvature and passes through the same midpoint as the arc.
	auto Extent = [&](double Radius) { return bContinuous ? FMath::Min(Radius * 1.528, HalfMin) : Radius; };
	auto Handle = [&](double ExtentValue) { return bContinuous ? ExtentValue * (1.0 - 0.178) : ExtentValue * Kappa; };

	const double ETL = Extent(Radii.TopLeft), ETR = Extent(Radii.TopRight), EBR = Extent(Radii.BottomRight), EBL = Extent(Radii.BottomLeft);

	MoveTo(FVector2D(L + ETL, T));
	LineTo(FVector2D(R - ETR, T));
	if (ETR > 0.0)
	{
		const double H = Handle(ETR);
		CubicTo(FVector2D(R - ETR + H, T), FVector2D(R, T + ETR - H), FVector2D(R, T + ETR));
	}
	LineTo(FVector2D(R, B - EBR));
	if (EBR > 0.0)
	{
		const double H = Handle(EBR);
		CubicTo(FVector2D(R, B - EBR + H), FVector2D(R - EBR + H, B), FVector2D(R - EBR, B));
	}
	LineTo(FVector2D(L + EBL, B));
	if (EBL > 0.0)
	{
		const double H = Handle(EBL);
		CubicTo(FVector2D(L + EBL - H, B), FVector2D(L, B - EBL + H), FVector2D(L, B - EBL));
	}
	LineTo(FVector2D(L, T + ETL));
	if (ETL > 0.0)
	{
		const double H = Handle(ETL);
		CubicTo(FVector2D(L, T + ETL - H), FVector2D(L + ETL - H, T), FVector2D(L + ETL, T));
	}
	return Close();
}

FVekaPath& FVekaPath::AddEllipse(const FVekaRect& Rect)
{
	const FVector2D C = Rect.Center();
	const double Rx = Rect.Size.X * 0.5;
	const double Ry = Rect.Size.Y * 0.5;
	const double Kx = Rx * Kappa;
	const double Ky = Ry * Kappa;

	MoveTo(FVector2D(C.X + Rx, C.Y));
	CubicTo(FVector2D(C.X + Rx, C.Y + Ky), FVector2D(C.X + Kx, C.Y + Ry), FVector2D(C.X, C.Y + Ry));
	CubicTo(FVector2D(C.X - Kx, C.Y + Ry), FVector2D(C.X - Rx, C.Y + Ky), FVector2D(C.X - Rx, C.Y));
	CubicTo(FVector2D(C.X - Rx, C.Y - Ky), FVector2D(C.X - Kx, C.Y - Ry), FVector2D(C.X, C.Y - Ry));
	CubicTo(FVector2D(C.X + Kx, C.Y - Ry), FVector2D(C.X + Rx, C.Y - Ky), FVector2D(C.X + Rx, C.Y));
	return Close();
}

FVekaPath& FVekaPath::AddCircle(const FVector2D& Center, double Radius)
{
	return AddEllipse(FVekaRect(Center - FVector2D(Radius, Radius), FVector2D(Radius * 2.0, Radius * 2.0)));
}

FVekaPath& FVekaPath::AddPolygon(TConstArrayView<FVector2D> Points, bool bClosed)
{
	if (Points.IsEmpty())
	{
		return *this;
	}
	MoveTo(Points[0]);
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		LineTo(Points[Index]);
	}
	if (bClosed)
	{
		Close();
	}
	return *this;
}

// --- Queries ----------------------------------------------------------------------------------------

void FVekaPath::Flatten(TArray<FVekaPolyline>& OutPolylines, double Tolerance) const
{
	FVekaPolyline* Current = nullptr;
	FVector2D Pen = FVector2D::ZeroVector;
	FVector2D SubpathStart = FVector2D::ZeroVector;

	auto Begin = [&](const FVector2D& Point)
	{
		Current = &OutPolylines.AddDefaulted_GetRef();
		Current->Points.Add(Point);
		Pen = Point;
		SubpathStart = Point;
	};
	auto EnsureStarted = [&]()
	{
		if (!Current)
		{
			Begin(Pen);
		}
	};

	for (const FVekaPathCommand& Command : Commands)
	{
		switch (Command.Verb)
		{
		case EVekaPathVerb::MoveTo:
			Begin(Command.P0);
			break;

		case EVekaPathVerb::LineTo:
			EnsureStarted();
			Current->Points.Add(Command.P0);
			Pen = Command.P0;
			break;

		case EVekaPathVerb::QuadTo:
		{
			EnsureStarted();
			const FVector2D P0 = Pen, P1 = Command.P0, P2 = Command.P1;
			const int32 Count = SegmentCount(FVector2D::Distance(P0, P1) + FVector2D::Distance(P1, P2), Tolerance);
			for (int32 Step = 1; Step <= Count; ++Step)
			{
				const double S = static_cast<double>(Step) / Count;
				const double U = 1.0 - S;
				Current->Points.Add(P0 * (U * U) + P1 * (2.0 * U * S) + P2 * (S * S));
			}
			Pen = P2;
			break;
		}

		case EVekaPathVerb::CubicTo:
		{
			EnsureStarted();
			const FVector2D P0 = Pen, P1 = Command.P0, P2 = Command.P1, P3 = Command.P2;
			const int32 Count = SegmentCount(FVector2D::Distance(P0, P1) + FVector2D::Distance(P1, P2) + FVector2D::Distance(P2, P3), Tolerance);
			for (int32 Step = 1; Step <= Count; ++Step)
			{
				const double S = static_cast<double>(Step) / Count;
				const double U = 1.0 - S;
				Current->Points.Add(P0 * (U * U * U) + P1 * (3.0 * U * U * S) + P2 * (3.0 * U * S * S) + P3 * (S * S * S));
			}
			Pen = P3;
			break;
		}

		case EVekaPathVerb::Close:
			if (Current)
			{
				Current->bClosed = true;
				Pen = SubpathStart;
				Current = nullptr;
			}
			break;
		}
	}

	OutPolylines.RemoveAll([](const FVekaPolyline& Polyline) { return Polyline.Points.Num() < 2; });
}

FBox2D FVekaPath::GetBounds() const
{
	TArray<FVekaPolyline> Polylines;
	Flatten(Polylines);
	FBox2D Bounds(ForceInit);
	for (const FVekaPolyline& Polyline : Polylines)
	{
		for (const FVector2D& Point : Polyline.Points)
		{
			Bounds += Point;
		}
	}
	return Bounds;
}

FVekaPath FVekaPath::Transformed(const FVector2D& Scale, const FVector2D& Translation) const
{
	FVekaPath Result = *this;
	for (FVekaPathCommand& Command : Result.Commands)
	{
		Command.P0 = Command.P0 * Scale + Translation;
		Command.P1 = Command.P1 * Scale + Translation;
		Command.P2 = Command.P2 * Scale + Translation;
	}
	return Result;
}

double FVekaPath::GetLength(double Tolerance) const
{
	TArray<FVekaPolyline> Polylines;
	Flatten(Polylines, Tolerance);
	double Length = 0.0;
	for (const FVekaPolyline& Polyline : Polylines)
	{
		Length += Veka::PathOps::PolylineLength(Polyline);
	}
	return Length;
}

bool FVekaPath::ContainsPoint(const FVector2D& Point, double Tolerance) const
{
	TArray<FVekaPolyline> Polylines;
	Flatten(Polylines, Tolerance);

	int32 Winding = 0;
	int32 Crossings = 0;
	for (const FVekaPolyline& Polyline : Polylines)
	{
		const TArray<FVector2D>& P = Polyline.Points;
		const int32 Num = P.Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FVector2D& A = P[Index];
			const FVector2D& B = P[(Index + 1) % Num]; // implicit close for filling
			if ((A.Y <= Point.Y) != (B.Y <= Point.Y))
			{
				const double X = A.X + (Point.Y - A.Y) * (B.X - A.X) / (B.Y - A.Y);
				if (X > Point.X)
				{
					++Crossings;
					Winding += B.Y > A.Y ? 1 : -1;
				}
			}
		}
	}
	return FillRule == EVekaFillRule::EvenOdd ? (Crossings % 2) == 1 : Winding != 0;
}

// --- SVG ----------------------------------------------------------------------------------------------

namespace
{
	class FSvgPathReader
	{
	public:
		explicit FSvgPathReader(const FString& InData) : Data(InData) {}

		void SkipSeparators()
		{
			while (Pos < Data.Len() && (FChar::IsWhitespace(Data[Pos]) || Data[Pos] == TEXT(',')))
			{
				++Pos;
			}
		}

		bool AtEnd()
		{
			SkipSeparators();
			return Pos >= Data.Len();
		}

		bool PeekCommand(TCHAR& OutCommand)
		{
			SkipSeparators();
			if (Pos < Data.Len() && FChar::IsAlpha(Data[Pos]) && Data[Pos] != TEXT('e') && Data[Pos] != TEXT('E'))
			{
				OutCommand = Data[Pos];
				return true;
			}
			return false;
		}

		void ConsumeCommand() { ++Pos; }

		bool PeekNumber()
		{
			SkipSeparators();
			if (Pos >= Data.Len())
			{
				return false;
			}
			const TCHAR C = Data[Pos];
			return FChar::IsDigit(C) || C == TEXT('-') || C == TEXT('+') || C == TEXT('.');
		}

		bool ReadNumber(double& Out)
		{
			SkipSeparators();
			const int32 Start = Pos;
			if (Pos < Data.Len() && (Data[Pos] == TEXT('-') || Data[Pos] == TEXT('+')))
			{
				++Pos;
			}
			bool bDigits = false;
			while (Pos < Data.Len() && FChar::IsDigit(Data[Pos]))
			{
				++Pos;
				bDigits = true;
			}
			if (Pos < Data.Len() && Data[Pos] == TEXT('.'))
			{
				++Pos;
				while (Pos < Data.Len() && FChar::IsDigit(Data[Pos]))
				{
					++Pos;
					bDigits = true;
				}
			}
			if (!bDigits)
			{
				Pos = Start;
				return false;
			}
			if (Pos < Data.Len() && (Data[Pos] == TEXT('e') || Data[Pos] == TEXT('E')))
			{
				const int32 ExponentStart = Pos;
				++Pos;
				if (Pos < Data.Len() && (Data[Pos] == TEXT('-') || Data[Pos] == TEXT('+')))
				{
					++Pos;
				}
				bool bExponentDigits = false;
				while (Pos < Data.Len() && FChar::IsDigit(Data[Pos]))
				{
					++Pos;
					bExponentDigits = true;
				}
				if (!bExponentDigits)
				{
					Pos = ExponentStart;
				}
			}
			Out = FCString::Atod(*Data.Mid(Start, Pos - Start));
			return true;
		}

		/** Arc flags may be written without separators ("a1 1 0 11 5 5"). */
		bool ReadFlag(bool& Out)
		{
			SkipSeparators();
			if (Pos < Data.Len() && (Data[Pos] == TEXT('0') || Data[Pos] == TEXT('1')))
			{
				Out = Data[Pos] == TEXT('1');
				++Pos;
				return true;
			}
			return false;
		}

		int32 GetPosition() const { return Pos; }

	private:
		const FString& Data;
		int32 Pos = 0;
	};
}

bool FVekaPath::FromSvg(const FString& PathData, FVekaPath& OutPath, FString* OutError)
{
	OutPath = FVekaPath();
	FSvgPathReader Reader(PathData);

	auto Fail = [&](const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("SVG path error at %d: %s"), Reader.GetPosition(), Message);
		}
		return false;
	};

	FVector2D Current = FVector2D::ZeroVector;
	FVector2D SubpathStart = FVector2D::ZeroVector;
	FVector2D LastControl = FVector2D::ZeroVector;
	TCHAR LastCommand = 0;
	TCHAR Command = 0;

	while (!Reader.AtEnd())
	{
		TCHAR NewCommand;
		if (Reader.PeekCommand(NewCommand))
		{
			Reader.ConsumeCommand();
			Command = NewCommand;
		}
		else if (Command == 0)
		{
			return Fail(TEXT("path must start with a command"));
		}
		else if (Command == TEXT('M'))
		{
			Command = TEXT('L'); // implicit lineto after moveto
		}
		else if (Command == TEXT('m'))
		{
			Command = TEXT('l');
		}

		const bool bRelative = FChar::IsLower(Command);
		const TCHAR Upper = FChar::ToUpper(Command);
		const FVector2D Base = bRelative ? Current : FVector2D::ZeroVector;

		auto ReadPoint = [&](FVector2D& Out)
		{
			double X, Y;
			if (!Reader.ReadNumber(X) || !Reader.ReadNumber(Y))
			{
				return false;
			}
			Out = Base + FVector2D(X, Y);
			return true;
		};

		switch (Upper)
		{
		case TEXT('M'):
		{
			FVector2D P;
			if (!ReadPoint(P)) return Fail(TEXT("M needs x y"));
			OutPath.MoveTo(P);
			Current = SubpathStart = LastControl = P;
			break;
		}
		case TEXT('L'):
		{
			FVector2D P;
			if (!ReadPoint(P)) return Fail(TEXT("L needs x y"));
			OutPath.LineTo(P);
			Current = LastControl = P;
			break;
		}
		case TEXT('H'):
		{
			double X;
			if (!Reader.ReadNumber(X)) return Fail(TEXT("H needs x"));
			Current = FVector2D(bRelative ? Current.X + X : X, Current.Y);
			OutPath.LineTo(Current);
			LastControl = Current;
			break;
		}
		case TEXT('V'):
		{
			double Y;
			if (!Reader.ReadNumber(Y)) return Fail(TEXT("V needs y"));
			Current = FVector2D(Current.X, bRelative ? Current.Y + Y : Y);
			OutPath.LineTo(Current);
			LastControl = Current;
			break;
		}
		case TEXT('C'):
		{
			FVector2D C1, C2, P;
			if (!ReadPoint(C1) || !ReadPoint(C2) || !ReadPoint(P)) return Fail(TEXT("C needs 6 numbers"));
			OutPath.CubicTo(C1, C2, P);
			LastControl = C2;
			Current = P;
			break;
		}
		case TEXT('S'):
		{
			FVector2D C2, P;
			if (!ReadPoint(C2) || !ReadPoint(P)) return Fail(TEXT("S needs 4 numbers"));
			const TCHAR PrevUpper = FChar::ToUpper(LastCommand);
			const FVector2D C1 = (PrevUpper == TEXT('C') || PrevUpper == TEXT('S')) ? Current * 2.0 - LastControl : Current;
			OutPath.CubicTo(C1, C2, P);
			LastControl = C2;
			Current = P;
			break;
		}
		case TEXT('Q'):
		{
			FVector2D C, P;
			if (!ReadPoint(C) || !ReadPoint(P)) return Fail(TEXT("Q needs 4 numbers"));
			OutPath.QuadTo(C, P);
			LastControl = C;
			Current = P;
			break;
		}
		case TEXT('T'):
		{
			FVector2D P;
			if (!ReadPoint(P)) return Fail(TEXT("T needs x y"));
			const TCHAR PrevUpper = FChar::ToUpper(LastCommand);
			const FVector2D C = (PrevUpper == TEXT('Q') || PrevUpper == TEXT('T')) ? Current * 2.0 - LastControl : Current;
			OutPath.QuadTo(C, P);
			LastControl = C;
			Current = P;
			break;
		}
		case TEXT('A'):
		{
			double Rx, Ry, Rotation, X, Y;
			bool bLarge, bSweep;
			if (!Reader.ReadNumber(Rx) || !Reader.ReadNumber(Ry) || !Reader.ReadNumber(Rotation)
				|| !Reader.ReadFlag(bLarge) || !Reader.ReadFlag(bSweep) || !Reader.ReadNumber(X) || !Reader.ReadNumber(Y))
			{
				return Fail(TEXT("A needs rx ry rotation large-arc sweep x y"));
			}
			const FVector2D P = Base + FVector2D(X, Y);
			if (OutPath.IsEmpty())
			{
				OutPath.MoveTo(Current);
			}
			OutPath.ArcTo(Rx, Ry, Rotation, bLarge, bSweep, P);
			Current = LastControl = P;
			break;
		}
		case TEXT('Z'):
			OutPath.Close();
			Current = LastControl = SubpathStart;
			LastCommand = Command;
			// Z takes no arguments: next token must be a command.
			Command = 0;
			continue;
		default:
			return Fail(TEXT("unknown command"));
		}

		LastCommand = Command;

		// Another command letter must follow if no more numbers.
		if (!Reader.PeekNumber())
		{
			TCHAR Next;
			if (!Reader.AtEnd() && !Reader.PeekCommand(Next))
			{
				return Fail(TEXT("unexpected character"));
			}
		}
	}
	return true;
}

// --- PathOps ------------------------------------------------------------------------------------------

namespace Veka::PathOps
{
	double PolylineLength(const FVekaPolyline& Polyline)
	{
		double Length = 0.0;
		const TArray<FVector2D>& P = Polyline.Points;
		for (int32 Index = 1; Index < P.Num(); ++Index)
		{
			Length += FVector2D::Distance(P[Index - 1], P[Index]);
		}
		if (Polyline.bClosed && P.Num() > 1)
		{
			Length += FVector2D::Distance(P.Last(), P[0]);
		}
		return Length;
	}

	namespace
	{
		/** Points of a polyline with the closing segment made explicit. */
		TArray<FVector2D> OpenPoints(const FVekaPolyline& Polyline)
		{
			TArray<FVector2D> Points = Polyline.Points;
			if (Polyline.bClosed && Points.Num() > 1 && !Points.Last().Equals(Points[0]))
			{
				Points.Add(Polyline.Points[0]); // not Points[0]: Add() may reallocate (UE asserts on aliasing)
			}
			return Points;
		}

		/** Appends the part of Points between distances A and B (local to this polyline). */
		void ExtractRange(const TArray<FVector2D>& Points, double A, double B, TArray<FVekaPolyline>& Out)
		{
			if (B <= A)
			{
				return;
			}
			FVekaPolyline Piece;
			double Walked = 0.0;
			for (int32 Index = 1; Index < Points.Num(); ++Index)
			{
				const FVector2D& P0 = Points[Index - 1];
				const FVector2D& P1 = Points[Index];
				const double SegmentLength = FVector2D::Distance(P0, P1);
				const double SegmentStart = Walked;
				const double SegmentEnd = Walked + SegmentLength;
				Walked = SegmentEnd;
				if (SegmentEnd < A || SegmentLength <= 0.0)
				{
					continue;
				}
				if (SegmentStart > B)
				{
					break;
				}
				const double T0 = FMath::Clamp((A - SegmentStart) / SegmentLength, 0.0, 1.0);
				const double T1 = FMath::Clamp((B - SegmentStart) / SegmentLength, 0.0, 1.0);
				if (Piece.Points.IsEmpty())
				{
					Piece.Points.Add(FMath::Lerp(P0, P1, T0));
				}
				Piece.Points.Add(FMath::Lerp(P0, P1, T1));
			}
			if (Piece.Points.Num() >= 2)
			{
				Out.Add(MoveTemp(Piece));
			}
		}
	}

	void Trim(const TArray<FVekaPolyline>& In, double From, double To, TArray<FVekaPolyline>& Out)
	{
		From = FMath::Clamp(From, 0.0, 1.0);
		To = FMath::Clamp(To, 0.0, 1.0);
		if (From <= 0.0 && To >= 1.0)
		{
			Out.Append(In);
			return;
		}

		double Total = 0.0;
		for (const FVekaPolyline& Polyline : In)
		{
			Total += PolylineLength(Polyline);
		}
		const double A = From * Total;
		const double B = To * Total;

		double Offset = 0.0;
		for (const FVekaPolyline& Polyline : In)
		{
			const double Length = PolylineLength(Polyline);
			ExtractRange(OpenPoints(Polyline), A - Offset, B - Offset, Out);
			Offset += Length;
		}
	}

	void Dash(const TArray<FVekaPolyline>& In, TConstArrayView<float> Pattern, double Phase, TArray<FVekaPolyline>& Out)
	{
		double PatternLength = 0.0;
		for (float Value : Pattern)
		{
			PatternLength += FMath::Max(0.f, Value);
		}
		if (Pattern.IsEmpty() || PatternLength <= 0.0)
		{
			Out.Append(In);
			return;
		}

		for (const FVekaPolyline& Polyline : In)
		{
			const TArray<FVector2D> Points = OpenPoints(Polyline);
			const double Length = PolylineLength(Polyline);

			// Find where the pattern starts for this phase.
			double Cursor = -FMath::Fmod(Phase, PatternLength);
			if (Cursor > 0.0)
			{
				Cursor -= PatternLength;
			}
			int32 PatternIndex = 0;
			while (Cursor < Length)
			{
				const double Segment = FMath::Max(0.f, Pattern[PatternIndex % Pattern.Num()]);
				const bool bOn = (PatternIndex % 2) == 0;
				if (bOn)
				{
					ExtractRange(Points, FMath::Max(Cursor, 0.0), FMath::Min(Cursor + Segment, Length), Out);
				}
				Cursor += Segment;
				++PatternIndex;
				if (PatternIndex > 100000)
				{
					break;
				}
			}
		}
	}

	double SignedArea(TConstArrayView<FVector2D> Polygon)
	{
		double Area = 0.0;
		const int32 Num = Polygon.Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FVector2D& A = Polygon[Index];
			const FVector2D& B = Polygon[(Index + 1) % Num];
			Area += A.X * B.Y - B.X * A.Y;
		}
		return Area * 0.5;
	}

	bool Triangulate(TConstArrayView<FVector2D> Polygon, TArray<int32>& OutIndices)
	{
		// Clean: drop duplicated consecutive points and the duplicated closing point.
		TArray<int32> Remaining;
		Remaining.Reserve(Polygon.Num());
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			if (Remaining.Num() > 0 && Polygon[Remaining.Last()].Equals(Polygon[Index], 1.0e-9))
			{
				continue;
			}
			Remaining.Add(Index);
		}
		if (Remaining.Num() > 1 && Polygon[Remaining.Last()].Equals(Polygon[Remaining[0]], 1.0e-9))
		{
			Remaining.Pop();
		}
		if (Remaining.Num() < 3)
		{
			return false;
		}

		// Work counter-clockwise (positive area in a y-down space means clockwise on screen; only consistency matters).
		TArray<FVector2D> Points;
		Points.Reserve(Remaining.Num());
		for (int32 Index : Remaining)
		{
			Points.Add(Polygon[Index]);
		}
		if (SignedArea(Points) < 0.0)
		{
			Algo::Reverse(Remaining);
		}

		auto Cross = [](const FVector2D& O, const FVector2D& A, const FVector2D& B)
		{
			return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X);
		};
		auto InTriangle = [&](const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C)
		{
			return Cross(A, B, P) >= 0.0 && Cross(B, C, P) >= 0.0 && Cross(C, A, P) >= 0.0;
		};

		int32 Guard = Remaining.Num() * Remaining.Num() + 16;
		while (Remaining.Num() > 3 && Guard-- > 0)
		{
			bool bClipped = false;
			const int32 Num = Remaining.Num();
			for (int32 I = 0; I < Num; ++I)
			{
				const int32 IPrev = Remaining[(I + Num - 1) % Num];
				const int32 ICurr = Remaining[I];
				const int32 INext = Remaining[(I + 1) % Num];
				const FVector2D& A = Polygon[IPrev];
				const FVector2D& B = Polygon[ICurr];
				const FVector2D& C = Polygon[INext];

				const double Convexity = Cross(A, B, C);
				if (Convexity <= 1.0e-12)
				{
					// Reflex or collinear: collinear points can simply be dropped.
					if (FMath::Abs(Convexity) <= 1.0e-12)
					{
						Remaining.RemoveAt(I);
						bClipped = true;
						break;
					}
					continue;
				}

				bool bContainsOther = false;
				for (int32 J = 0; J < Num; ++J)
				{
					const int32 Other = Remaining[J];
					if (Other == IPrev || Other == ICurr || Other == INext)
					{
						continue;
					}
					if (InTriangle(Polygon[Other], A, B, C))
					{
						bContainsOther = true;
						break;
					}
				}
				if (bContainsOther)
				{
					continue;
				}

				OutIndices.Add(IPrev);
				OutIndices.Add(ICurr);
				OutIndices.Add(INext);
				Remaining.RemoveAt(I);
				bClipped = true;
				break;
			}
			if (!bClipped)
			{
				return false;
			}
		}

		if (Remaining.Num() == 3)
		{
			OutIndices.Add(Remaining[0]);
			OutIndices.Add(Remaining[1]);
			OutIndices.Add(Remaining[2]);
		}
		return true;
	}
}
