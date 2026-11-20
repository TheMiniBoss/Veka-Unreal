// Copyright TheMiniBoss. All Rights Reserved.

// Built-in Veka symbols: original stroke skeletons on a 24 x 24 grid (live area ~3..21), drawn for this library.
// Roles follow the Alpha 13 color code: Primary (red), Secondary (green), Tertiary (blue).

#include "Symbols/VekaSymbol.h"

namespace Veka::Symbols::Private
{
	namespace
	{
		using ERole = EVekaSymbolRole;

		struct FBuilder
		{
			FVekaSymbolDefinition Def;
			ERole Role = ERole::Primary;
			int32 Variable = -1;
			FName Token;
			float Width = 1.f;
			/** True right after a shape is added: Color() then also tints that shape. */
			bool bJustAdded = false;

			FBuilder(const TCHAR* Name, const TCHAR* Category, const TCHAR* Keywords = TEXT(""))
			{
				Def.Name = FName(Name);
				Def.Category = FName(Category);
				FString(Keywords).ParseIntoArray(Def.Keywords, TEXT(" "), true);
			}

			FBuilder& P() { bJustAdded = false; Role = ERole::Primary; return *this; }
			FBuilder& S() { bJustAdded = false; Role = ERole::Secondary; return *this; }
			FBuilder& T() { bJustAdded = false; Role = ERole::Tertiary; return *this; }
			FBuilder& Var(int32 Index) { bJustAdded = false; Variable = Index; Def.VariableSteps = FMath::Max(Def.VariableSteps, Index + 1); return *this; }
			FBuilder& NoVar() { bJustAdded = false; Variable = -1; return *this; }
			FBuilder& Color(const TCHAR* InToken)
			{
				Token = InToken ? FName(InToken) : NAME_None;
				if (bJustAdded && !Def.Shapes.IsEmpty() && Def.Shapes.Last().ColorToken.IsNone())
				{
					Def.Shapes.Last().ColorToken = Token;
				}
				return *this;
			}
			FBuilder& Thin(float Scale = 0.75f) { bJustAdded = false; Width = Scale; return *this; }
			FBuilder& Normal() { bJustAdded = false; Width = 1.f; return *this; }
			FBuilder& Anchor(double X, double Y) { Def.Anchor = FVector2D(X, Y); return *this; }

			FVekaSymbolShape& Add(FVekaPath&& Path, EVekaSymbolShapeKind Kind, bool bFillable)
			{
				FVekaSymbolShape& Shape = Def.Shapes.AddDefaulted_GetRef();
				Shape.Kind = Kind;
				Shape.Path = MoveTemp(Path);
				Shape.Role = Role;
				Shape.VariableIndex = Variable;
				Shape.bFillable = bFillable;
				Shape.ColorToken = Token;
				Shape.StrokeScale = Width;
				bJustAdded = true;
				return Shape;
			}

			FBuilder& Line(double X1, double Y1, double X2, double Y2)
			{
				FVekaPath Path;
				Path.MoveTo(FVector2D(X1, Y1)).LineTo(FVector2D(X2, Y2));
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Stroke, false);
				return *this;
			}

			FBuilder& Poly(std::initializer_list<double> XY, bool bClosed = false, bool bFillable = false)
			{
				TArray<FVector2D> Points;
				const TArray<double> Values(XY);
				for (int32 Index = 0; Index + 1 < Values.Num(); Index += 2)
				{
					Points.Add(FVector2D(Values[Index], Values[Index + 1]));
				}
				FVekaPath Path;
				Path.AddPolygon(Points, bClosed);
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Stroke, bFillable);
				return *this;
			}

			/** Closed outline the .fill variant fills. */
			FBuilder& Closed(std::initializer_list<double> XY) { return Poly(XY, true, true); }

			FBuilder& Circle(double X, double Y, double R, bool bFillable = true)
			{
				FVekaPath Path;
				Path.AddCircle(FVector2D(X, Y), R);
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Stroke, bFillable);
				return *this;
			}

			/** Arc from A0 to A1 degrees (0 = +X, clockwise on screen). */
			FBuilder& Arc(double X, double Y, double R, double A0, double A1)
			{
				TArray<FVector2D> Points;
				const int32 Steps = FMath::Max(2, FMath::CeilToInt32(FMath::Abs(A1 - A0) / 7.5));
				for (int32 Step = 0; Step <= Steps; ++Step)
				{
					const double Angle = FMath::DegreesToRadians(FMath::Lerp(A0, A1, static_cast<double>(Step) / Steps));
					Points.Add(FVector2D(X + R * FMath::Cos(Angle), Y + R * FMath::Sin(Angle)));
				}
				FVekaPath Path;
				Path.AddPolygon(Points, false);
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Stroke, false);
				return *this;
			}

			FBuilder& RRect(double X, double Y, double W, double H, double R, bool bFillable = true)
			{
				FVekaPath Path;
				Path.AddRoundedRect(FVekaRect(FVector2D(X, Y), FVector2D(W, H)), FVekaCornerRadii(static_cast<float>(R)));
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Stroke, bFillable);
				return *this;
			}

			/** Round dot (a zero-length stroke): its size follows the weight. */
			FBuilder& Dot(double X, double Y, float Scale = 1.5f)
			{
				const float Saved = Width;
				Width = Scale;
				Line(X, Y, X, Y);
				Width = Saved;
				return *this;
			}

			FBuilder& Svg(const TCHAR* Data, bool bFillable = false)
			{
				FVekaPath Path;
				ensureMsgf(FVekaPath::FromSvg(Data, Path), TEXT("Bad symbol path in %s"), *Def.Name.ToString());
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Stroke, bFillable);
				return *this;
			}

			FBuilder& FillSvg(const TCHAR* Data)
			{
				FVekaPath Path;
				ensureMsgf(FVekaPath::FromSvg(Data, Path), TEXT("Bad symbol path in %s"), *Def.Name.ToString());
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Fill, false);
				return *this;
			}

			FBuilder& FillCircle(double X, double Y, double R)
			{
				FVekaPath Path;
				Path.AddCircle(FVector2D(X, Y), R);
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Fill, false);
				return *this;
			}

			/** Arrowhead at the tip, pointing toward Angle degrees (0 = right, 90 = down). */
			FBuilder& Head(double X, double Y, double Angle, double Length = 5.0)
			{
				const double A = FMath::DegreesToRadians(Angle);
				const double Spread = FMath::DegreesToRadians(42.0);
				const FVector2D Tip(X, Y);
				const FVector2D W1 = Tip - FVector2D(FMath::Cos(A - Spread), FMath::Sin(A - Spread)) * Length;
				const FVector2D W2 = Tip - FVector2D(FMath::Cos(A + Spread), FMath::Sin(A + Spread)) * Length;
				return Poly({ W1.X, W1.Y, Tip.X, Tip.Y, W2.X, W2.Y });
			}

			FBuilder& RegularPolygon(double X, double Y, int32 Sides, double R, double RotationDeg = -90.0, bool bStar = false, double InnerR = 0.0)
			{
				TArray<FVector2D> Points;
				const int32 Count = bStar ? Sides * 2 : Sides;
				for (int32 Index = 0; Index < Count; ++Index)
				{
					const double Radius = bStar && (Index % 2 == 1) ? InnerR : R;
					const double Angle = FMath::DegreesToRadians(RotationDeg + 360.0 * Index / Count);
					Points.Add(FVector2D(X + Radius * FMath::Cos(Angle), Y + Radius * FMath::Sin(Angle)));
				}
				FVekaPath Path;
				Path.AddPolygon(Points, true);
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Stroke, true);
				return *this;
			}

			FBuilder& Star(double X, double Y, int32 Points, double R, double InnerR, double RotationDeg = -90.0)
			{
				return RegularPolygon(X, Y, Points, R, RotationDeg, true, InnerR);
			}

			FBuilder& Gear(double X, double Y, int32 Teeth, double R, double InnerR)
			{
				TArray<FVector2D> Points;
				for (int32 Tooth = 0; Tooth < Teeth; ++Tooth)
				{
					const double Base = UE_TWO_PI * Tooth / Teeth;
					const double Step = UE_TWO_PI / Teeth;
					const double Angles[4] = { Base - Step * 0.18, Base + Step * 0.18, Base + Step * 0.32, Base + Step * 0.68 };
					const double Radii[4] = { R, R, InnerR, InnerR };
					for (int32 Corner = 0; Corner < 4; ++Corner)
					{
						Points.Add(FVector2D(X + Radii[Corner] * FMath::Cos(Angles[Corner]), Y + Radii[Corner] * FMath::Sin(Angles[Corner])));
					}
				}
				FVekaPath Path;
				Path.AddPolygon(Points, true);
				Add(MoveTemp(Path), EVekaSymbolShapeKind::Stroke, true);
				return *this;
			}
		};

		void Add(TArray<FVekaSymbolDefinition>& Out, const FBuilder& Builder)
		{
			Out.Add(Builder.Def);
		}

		const TCHAR* Cloud = TEXT("M7 18.5 H17 A4 4 0 0 0 17.4 10.5 A5.6 5.6 0 0 0 6.6 10.2 A4.2 4.2 0 0 0 7 18.5 Z");
		const TCHAR* CloudHigh = TEXT("M7 14.5 H17 A3.6 3.6 0 0 0 17.4 7.3 A5 5 0 0 0 6.6 7 A3.8 3.8 0 0 0 7 14.5 Z");
	}

	void BuildBuiltInLibrary(TArray<FVekaSymbolDefinition>& Out)
	{
		// --- Arrows -------------------------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("arrow.up"), TEXT("Arrows"), TEXT("up top")).Line(12, 20, 12, 4.5).Head(12, 4.5, -90));
		Add(Out, FBuilder(TEXT("arrow.down"), TEXT("Arrows"), TEXT("down bottom")).Line(12, 4, 12, 19.5).Head(12, 19.5, 90));
		Add(Out, FBuilder(TEXT("arrow.left"), TEXT("Arrows"), TEXT("back previous")).Line(20, 12, 4.5, 12).Head(4.5, 12, 180));
		Add(Out, FBuilder(TEXT("arrow.right"), TEXT("Arrows"), TEXT("next forward")).Line(4, 12, 19.5, 12).Head(19.5, 12, 0));
		Add(Out, FBuilder(TEXT("arrow.up.right"), TEXT("Arrows"), TEXT("external open")).Line(6, 18, 17.5, 6.5).Head(17.5, 6.5, -45, 6));
		Add(Out, FBuilder(TEXT("arrow.up.left"), TEXT("Arrows")).Line(18, 18, 6.5, 6.5).Head(6.5, 6.5, -135, 6));
		Add(Out, FBuilder(TEXT("arrow.down.right"), TEXT("Arrows")).Line(6, 6, 17.5, 17.5).Head(17.5, 17.5, 45, 6));
		Add(Out, FBuilder(TEXT("arrow.down.left"), TEXT("Arrows")).Line(18, 6, 6.5, 17.5).Head(6.5, 17.5, 135, 6));
		Add(Out, FBuilder(TEXT("arrow.up.down"), TEXT("Arrows"), TEXT("sort swap")).Line(8, 20, 8, 4.5).Head(8, 4.5, -90, 4).S().Line(16, 4, 16, 19.5).Head(16, 19.5, 90, 4));
		Add(Out, FBuilder(TEXT("arrow.left.right"), TEXT("Arrows"), TEXT("swap exchange")).Line(20, 8, 4.5, 8).Head(4.5, 8, 180, 4).S().Line(4, 16, 19.5, 16).Head(19.5, 16, 0, 4));
		Add(Out, FBuilder(TEXT("arrow.clockwise"), TEXT("Arrows"), TEXT("redo refresh reload")).Arc(12, 12, 8, -60, 230).Head(16, 5.07, 30, 4.5));
		Add(Out, FBuilder(TEXT("arrow.counterclockwise"), TEXT("Arrows"), TEXT("undo refresh")).Arc(12, 12, 8, 240, -50).Head(8, 5.07, 150, 4.5));
		Add(Out, FBuilder(TEXT("arrow.uturn.left"), TEXT("Arrows"), TEXT("undo back")).Svg(TEXT("M19 19 V11 A5 5 0 0 0 14 6 H5")).Head(5, 6, 180));
		Add(Out, FBuilder(TEXT("arrow.uturn.right"), TEXT("Arrows"), TEXT("redo")).Svg(TEXT("M5 19 V11 A5 5 0 0 1 10 6 H19")).Head(19, 6, 0));
		Add(Out, FBuilder(TEXT("arrow.down.to.line"), TEXT("Arrows"), TEXT("download save")).Line(12, 3.5, 12, 15.5).Head(12, 15.5, 90).S().Line(5, 20.5, 19, 20.5));
		Add(Out, FBuilder(TEXT("arrow.up.to.line"), TEXT("Arrows"), TEXT("upload")).Line(12, 20.5, 12, 8.5).Head(12, 8.5, -90).S().Line(5, 3.5, 19, 3.5));
		Add(Out, FBuilder(TEXT("arrow.2.circlepath"), TEXT("Arrows"), TEXT("sync refresh repeat")).Arc(12, 12, 7.5, 200, 340).Head(19.05, 9.43, 70, 4).S().Arc(12, 12, 7.5, 20, 160).Head(4.95, 14.57, 250, 4));
		Add(Out, FBuilder(TEXT("return"), TEXT("Arrows"), TEXT("enter")).Svg(TEXT("M19.5 5 V11 A3 3 0 0 1 16.5 14 H5")).Head(5, 14, 180, 4.5));
		Add(Out, FBuilder(TEXT("arrow.move"), TEXT("Arrows"), TEXT("move drag")).Line(12, 3.5, 12, 20.5).Line(3.5, 12, 20.5, 12).S().Head(12, 3.5, -90, 3.5).Head(12, 20.5, 90, 3.5).Head(3.5, 12, 180, 3.5).Head(20.5, 12, 0, 3.5));
		Add(Out, FBuilder(TEXT("arrow.expand"), TEXT("Arrows"), TEXT("fullscreen enlarge")).Line(5, 19, 19, 5).S().Head(19, 5, -45, 5).Head(5, 19, 135, 5));
		Add(Out, FBuilder(TEXT("arrow.compress"), TEXT("Arrows"), TEXT("shrink reduce")).Line(4.5, 19.5, 10.5, 13.5).Line(19.5, 4.5, 13.5, 10.5).S().Head(10.5, 13.5, -45, 4.5).Head(13.5, 10.5, 135, 4.5));

		// --- Chevrons -----------------------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("chevron.up"), TEXT("Arrows")).Poly({ 6, 15, 12, 9, 18, 15 }));
		Add(Out, FBuilder(TEXT("chevron.down"), TEXT("Arrows")).Poly({ 6, 9, 12, 15, 18, 9 }));
		Add(Out, FBuilder(TEXT("chevron.left"), TEXT("Arrows"), TEXT("back")).Poly({ 15, 6, 9, 12, 15, 18 }));
		Add(Out, FBuilder(TEXT("chevron.right"), TEXT("Arrows"), TEXT("next disclosure")).Poly({ 9, 6, 15, 12, 9, 18 }));
		Add(Out, FBuilder(TEXT("chevron.up.down"), TEXT("Arrows"), TEXT("picker select")).Poly({ 7, 9.5, 12, 4.5, 17, 9.5 }).S().Poly({ 7, 14.5, 12, 19.5, 17, 14.5 }));
		Add(Out, FBuilder(TEXT("chevron.left.right"), TEXT("Arrows"), TEXT("code")).Poly({ 9.5, 7, 4.5, 12, 9.5, 17 }).S().Poly({ 14.5, 7, 19.5, 12, 14.5, 17 }));

		// --- Marks and math -----------------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("plus"), TEXT("Math"), TEXT("add new create")).Line(12, 5, 12, 19).Line(5, 12, 19, 12));
		Add(Out, FBuilder(TEXT("minus"), TEXT("Math"), TEXT("remove subtract")).Line(5, 12, 19, 12));
		Add(Out, FBuilder(TEXT("xmark"), TEXT("Math"), TEXT("close cancel delete remove")).Line(6, 6, 18, 18).Line(18, 6, 6, 18));
		Add(Out, FBuilder(TEXT("checkmark"), TEXT("Math"), TEXT("done ok yes valid")).Poly({ 5, 12.5, 9.8, 17.3, 19, 7 }));
		Add(Out, FBuilder(TEXT("divide"), TEXT("Math")).Line(5, 12, 19, 12).S().Dot(12, 6.5).Dot(12, 17.5));
		Add(Out, FBuilder(TEXT("equal"), TEXT("Math")).Line(5, 9, 19, 9).Line(5, 15, 19, 15));
		Add(Out, FBuilder(TEXT("percent"), TEXT("Math")).Line(18, 5.5, 6, 18.5).S().Circle(7.5, 7.5, 2.3, false).Circle(16.5, 16.5, 2.3, false));
		Add(Out, FBuilder(TEXT("number"), TEXT("Math"), TEXT("hashtag")).Line(9.5, 4, 7.5, 20).Line(16.5, 4, 14.5, 20).Line(4.5, 9, 19.8, 9).Line(4.2, 15, 19.5, 15));
		Add(Out, FBuilder(TEXT("asterisk"), TEXT("Math"), TEXT("required")).Line(12, 4, 12, 20).Line(5.1, 8, 18.9, 16).Line(5.1, 16, 18.9, 8));
		Add(Out, FBuilder(TEXT("at"), TEXT("Communication"), TEXT("email mention")).Circle(12, 12, 3.6, false).Svg(TEXT("M15.6 12 V13.6 A2.6 2.6 0 0 0 20.8 13.6 V12 A8.8 8.8 0 1 0 17.3 19.1")));

		// --- Status -------------------------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("info"), TEXT("Status"), TEXT("information about help")).Line(12, 10.5, 12, 18).Dot(12, 6));
		Add(Out, FBuilder(TEXT("questionmark"), TEXT("Status"), TEXT("help question interrogation")).Svg(TEXT("M8.6 8.6 A3.5 3.5 0 1 1 13.6 11.7 C12.6 12.2 12 13 12 14.1 V14.6")).Dot(12, 18.6));
		Add(Out, FBuilder(TEXT("exclamationmark"), TEXT("Status"), TEXT("alert important")).Line(12, 5, 12, 14).Dot(12, 18.6));
		Add(Out, FBuilder(TEXT("exclamationmark.triangle"), TEXT("Status"), TEXT("warning caution alert")).Closed({ 12, 3.5, 21, 19.5, 3, 19.5 }).S().Line(12, 9.5, 12, 13.8).Dot(12, 16.8, 1.3f));
		Add(Out, FBuilder(TEXT("nosign"), TEXT("Status"), TEXT("forbidden block")).Circle(12, 12, 8.5).Line(6, 18, 18, 6));
		Add(Out, FBuilder(TEXT("checkmark.seal"), TEXT("Status"), TEXT("verified certified")).Star(12, 12, 12, 9.2, 7.6).S().Poly({ 8.2, 12.2, 10.9, 14.9, 15.8, 9.6 }));
		Add(Out, FBuilder(TEXT("bell"), TEXT("Status"), TEXT("notification alarm")).Svg(TEXT("M6 16.5 V11 A6 6 0 0 1 18 11 V16.5 L19.5 18 H4.5 Z"), true).S().Line(10.2, 20.6, 13.8, 20.6).Anchor(12, 4));
		Add(Out, FBuilder(TEXT("flag"), TEXT("Status"), TEXT("report mark")).Line(5.5, 21, 5.5, 3.5).S().Svg(TEXT("M5.5 4.5 H18 L15.5 8.5 L18 12.5 H5.5"), true));
		Add(Out, FBuilder(TEXT("loading"), TEXT("Status"), TEXT("spinner progress busy wait")).Line(12, 3.5, 12, 7).Line(18, 6, 15.5, 8.5).S().Line(20.5, 12, 17, 12).Line(18, 18, 15.5, 15.5).Line(12, 20.5, 12, 17).T().Line(6, 18, 8.5, 15.5).Line(3.5, 12, 7, 12).Line(6, 6, 8.5, 8.5));

		// --- Shapes -------------------------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("circle"), TEXT("Shapes"), TEXT("round dot")).Circle(12, 12, 9));
		Add(Out, FBuilder(TEXT("square"), TEXT("Shapes"), TEXT("box")).RRect(3.5, 3.5, 17, 17, 3.5));
		Add(Out, FBuilder(TEXT("triangle"), TEXT("Shapes"), TEXT("delta")).Closed({ 12, 4, 20.5, 19, 3.5, 19 }));
		Add(Out, FBuilder(TEXT("diamond"), TEXT("Shapes"), TEXT("rhombus")).Closed({ 12, 3, 21, 12, 12, 21, 3, 12 }));
		Add(Out, FBuilder(TEXT("pentagon"), TEXT("Shapes")).RegularPolygon(12, 12.6, 5, 9));
		Add(Out, FBuilder(TEXT("hexagon"), TEXT("Shapes")).RegularPolygon(12, 12, 6, 9, -90));
		Add(Out, FBuilder(TEXT("octagon"), TEXT("Shapes"), TEXT("stop")).RegularPolygon(12, 12, 8, 9, -67.5));
		Add(Out, FBuilder(TEXT("star"), TEXT("Shapes"), TEXT("favorite rate")).Star(12, 12.8, 5, 9.4, 4.3));
		Add(Out, FBuilder(TEXT("heart"), TEXT("Shapes"), TEXT("love like favorite")).Svg(TEXT("M12 20 C5.2 15.3 3 11.6 3 8.6 A4.6 4.6 0 0 1 12 6.6 A4.6 4.6 0 0 1 21 8.6 C21 11.6 18.8 15.3 12 20 Z"), true).Color(TEXT("Red")));
		Add(Out, FBuilder(TEXT("capsule"), TEXT("Shapes"), TEXT("pill")).RRect(3, 7.5, 18, 9, 4.5));
		Add(Out, FBuilder(TEXT("seal"), TEXT("Shapes"), TEXT("badge")).Star(12, 12, 12, 9.2, 7.6));
		Add(Out, FBuilder(TEXT("shield"), TEXT("Shapes"), TEXT("security protect")).Svg(TEXT("M12 3 L19.5 6 V11.5 C19.5 16 16.5 19.5 12 21 C7.5 19.5 4.5 16 4.5 11.5 V6 Z"), true));
		Add(Out, FBuilder(TEXT("bolt"), TEXT("Shapes"), TEXT("thunder lightning power flash")).Closed({ 13.5, 3, 5.5, 13.5, 11.5, 13.5, 10.5, 21, 18.5, 10.5, 12.5, 10.5 }).Color(TEXT("Yellow")));
		Add(Out, FBuilder(TEXT("drop"), TEXT("Shapes"), TEXT("water liquid")).Svg(TEXT("M12 3.5 C12 3.5 5.5 10.5 5.5 14.5 A6.5 6.5 0 0 0 18.5 14.5 C18.5 10.5 12 3.5 12 3.5 Z"), true).Color(TEXT("Blue")));
		Add(Out, FBuilder(TEXT("sparkle"), TEXT("Shapes"), TEXT("magic shine")).Svg(TEXT("M12 3 C12.8 8.5 15.5 11.2 21 12 C15.5 12.8 12.8 15.5 12 21 C11.2 15.5 8.5 12.8 3 12 C8.5 11.2 11.2 8.5 12 3 Z"), true).Color(TEXT("Yellow")));

		// --- Nature and weather -------------------------------------------------------------------------------
		{
			FBuilder Sun(TEXT("sun"), TEXT("Weather"), TEXT("day light bright brightness"));
			Sun.Circle(12, 12, 4).S().Color(TEXT("Yellow"));
			for (int32 Ray = 0; Ray < 8; ++Ray)
			{
				const double A = UE_TWO_PI * Ray / 8.0;
				Sun.Line(12 + 7.2 * FMath::Cos(A), 12 + 7.2 * FMath::Sin(A), 12 + 9.4 * FMath::Cos(A), 12 + 9.4 * FMath::Sin(A));
			}
			Sun.Def.Shapes[0].ColorToken = TEXT("Orange");
			Add(Out, Sun);
		}
		Add(Out, FBuilder(TEXT("moon"), TEXT("Weather"), TEXT("night dark sleep")).Svg(TEXT("M19.5 14.5 A8 8 0 1 1 9.5 4.5 A6.5 6.5 0 0 0 19.5 14.5 Z"), true).Color(TEXT("Yellow")));
		Add(Out, FBuilder(TEXT("moon.stars"), TEXT("Weather"), TEXT("night")).Svg(TEXT("M17 16.5 A7.5 7.5 0 1 1 8 6 A6 6 0 0 0 17 16.5 Z"), true).S().Star(17.5, 6.5, 4, 2.6, 0.9, 0).Thin().Star(20.5, 11, 4, 1.6, 0.6, 0));
		Add(Out, FBuilder(TEXT("cloud"), TEXT("Weather"), TEXT("weather sky")).Svg(Cloud, true));
		Add(Out, FBuilder(TEXT("cloud.rain"), TEXT("Weather"), TEXT("rain weather")).Svg(CloudHigh, true).S().Color(TEXT("Blue")).Line(8, 17.5, 7, 20.5).Line(12, 17.5, 11, 20.5).Line(16, 17.5, 15, 20.5));
		Add(Out, FBuilder(TEXT("cloud.bolt"), TEXT("Weather"), TEXT("storm thunder")).Svg(CloudHigh, true).S().Color(TEXT("Yellow")).Poly({ 12.5, 15.5, 10, 18.5, 13.5, 18.5, 11.5, 21.5 }));
		{
			FBuilder Snow(TEXT("snowflake"), TEXT("Weather"), TEXT("cold winter freeze"));
			for (int32 Arm = 0; Arm < 3; ++Arm)
			{
				const double A = UE_PI * Arm / 3.0 + UE_HALF_PI;
				Snow.P().Line(12 + 9 * FMath::Cos(A), 12 + 9 * FMath::Sin(A), 12 - 9 * FMath::Cos(A), 12 - 9 * FMath::Sin(A));
			}
			for (int32 Tip = 0; Tip < 6; ++Tip)
			{
				const double A = UE_PI * Tip / 3.0 + UE_HALF_PI;
				const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
				const FVector2D Side(-Dir.Y, Dir.X);
				const FVector2D Base = FVector2D(12, 12) + Dir * 5.5;
				const FVector2D L = Base + Dir * 2.6 + Side * 2.2;
				const FVector2D R = Base + Dir * 2.6 - Side * 2.2;
				Snow.S().Poly({ L.X, L.Y, Base.X, Base.Y, R.X, R.Y });
			}
			Add(Out, Snow);
		}
		Add(Out, FBuilder(TEXT("wind"), TEXT("Weather"), TEXT("air breeze")).Svg(TEXT("M3.5 9 H14.5 A2.5 2.5 0 1 0 12 6.5")).S().Svg(TEXT("M3.5 13 H18 A2.5 2.5 0 1 1 15.5 15.5")).T().Line(3.5, 17, 10, 17));
		Add(Out, FBuilder(TEXT("flame"), TEXT("Nature"), TEXT("fire hot trending")).Svg(TEXT("M12 21 A6.5 6.5 0 0 1 5.5 14.5 C5.5 10.5 9 8.5 9.5 3.5 C12.5 5.5 14 8 14 10 C15 9.5 15.8 8.5 16 7.5 C17.5 9.5 18.5 12 18.5 14.5 A6.5 6.5 0 0 1 12 21 Z"), true).Color(TEXT("Orange")));
		Add(Out, FBuilder(TEXT("leaf"), TEXT("Nature"), TEXT("eco plant green")).Svg(TEXT("M5 19 C5 10 10 5 20 4 C19 14 14 19 5 19 Z"), true).Color(TEXT("Green")).S().Line(5, 19, 13, 11));
		Add(Out, FBuilder(TEXT("thermometer"), TEXT("Weather"), TEXT("temperature heat")).Svg(TEXT("M10 14 V5.5 A2 2 0 0 1 14 5.5 V14 A4 4 0 1 1 10 14 Z"), true).S().Color(TEXT("Red")).Line(12, 10, 12, 17));
		Add(Out, FBuilder(TEXT("umbrella"), TEXT("Weather"), TEXT("rain protection")).Svg(TEXT("M3 12 A9 9 0 0 1 21 12 Z"), true).S().Svg(TEXT("M12 12 V18.5 A2 2 0 0 1 8 18.5")));
		Add(Out, FBuilder(TEXT("sparkles"), TEXT("Nature"), TEXT("magic new ai")).Svg(TEXT("M10 4 C10.6 8.6 12.4 10.4 17 11 C12.4 11.6 10.6 13.4 10 18 C9.4 13.4 7.6 11.6 3 11 C7.6 10.4 9.4 8.6 10 4 Z"), true).S().Color(TEXT("Yellow")).Star(18, 5.5, 4, 2.8, 0.9, 0).Star(18, 17.5, 4, 2.2, 0.8, 0));

		// --- People and communication -------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("person"), TEXT("People"), TEXT("user account profile")).Circle(12, 8, 3.8).Svg(TEXT("M4.5 20.5 A7.5 7.5 0 0 1 19.5 20.5")));
		Add(Out, FBuilder(TEXT("person.2"), TEXT("People"), TEXT("group users team")).Circle(9.5, 8.5, 3.3).Svg(TEXT("M3 20 A6.5 6.5 0 0 1 16 20")).S().Svg(TEXT("M15.5 5.6 A3 3 0 1 1 15.5 11.4")).Svg(TEXT("M18 13.8 A6.2 6.2 0 0 1 21.5 19.5")));
		Add(Out, FBuilder(TEXT("face.smiling"), TEXT("People"), TEXT("emoji happy")).Circle(12, 12, 9).S().Dot(9, 10).Dot(15, 10).Svg(TEXT("M8.3 14.2 A4.5 4.5 0 0 0 15.7 14.2")));
		Add(Out, FBuilder(TEXT("hand.thumbsup"), TEXT("People"), TEXT("like approve")).Svg(TEXT("M8 10.5 L11.5 3.5 A2 2 0 0 1 14 5.5 V9 H18.5 A2 2 0 0 1 20.4 11.5 L18.8 18.5 A2.5 2.5 0 0 1 16.4 20.5 H8 Z"), true).S().RRect(3.5, 10.5, 4.5, 10, 1.2, false));
		Add(Out, FBuilder(TEXT("bubble"), TEXT("Communication"), TEXT("message chat comment")).Svg(TEXT("M7 17 H17 A4 4 0 0 0 21 13 V8 A4 4 0 0 0 17 4 H7 A4 4 0 0 0 3 8 V13 A4 4 0 0 0 7 17 V20.5 Z"), true));
		Add(Out, FBuilder(TEXT("bubble.2"), TEXT("Communication"), TEXT("conversation chat")).Svg(TEXT("M5.5 13.5 V16.5 L8.5 13.5 H14 A3 3 0 0 0 17 10.5 V6.5 A3 3 0 0 0 14 3.5 H5.5 A3 3 0 0 0 2.5 6.5 V10.5 A3 3 0 0 0 5.5 13.5 Z"), true).S().Svg(TEXT("M19.5 8.5 A2.5 2.5 0 0 1 21.5 11 V14.5 A2.5 2.5 0 0 1 19 17 H18.5 V20 L15.5 17 H11 A2.5 2.5 0 0 1 9 16")));
		Add(Out, FBuilder(TEXT("envelope"), TEXT("Communication"), TEXT("mail email message")).RRect(3, 5.5, 18, 13, 2.2).S().Poly({ 3.6, 6.6, 12, 13, 20.4, 6.6 }));
		Add(Out, FBuilder(TEXT("phone"), TEXT("Communication"), TEXT("call telephone")).Svg(TEXT("M6.5 3.5 L9.5 3.5 L11 8 L8.7 9.6 A11 11 0 0 0 14.4 15.3 L16 13 L20.5 14.5 V17.5 A2.5 2.5 0 0 1 18 20 A15 15 0 0 1 4 6 A2.5 2.5 0 0 1 6.5 3.5 Z"), true));
		Add(Out, FBuilder(TEXT("paperplane"), TEXT("Communication"), TEXT("send share")).Closed({ 3, 11, 21, 3.5, 13.5, 21, 11, 13 }).S().Line(11, 13, 21, 3.5));
		Add(Out, FBuilder(TEXT("megaphone"), TEXT("Communication"), TEXT("announce speaker")).Svg(TEXT("M4 9.5 H8 L18 5 V19 L8 14.5 H4 Z"), true).S().Svg(TEXT("M8 14.5 L9.5 20 H12 L10.8 15.4")));
		Add(Out, FBuilder(TEXT("dot.radiowaves"), TEXT("Communication"), TEXT("broadcast signal live")).Dot(12, 12, 1.8f).S().Var(0).Arc(12, 12, 4.5, -45, 45).Arc(12, 12, 4.5, 135, 225).Var(1).Arc(12, 12, 8.5, -45, 45).Arc(12, 12, 8.5, 135, 225));

		// --- Interface ----------------------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("house"), TEXT("Interface"), TEXT("home main")).Poly({ 3.5, 11.5, 12, 3.8, 20.5, 11.5 }).Svg(TEXT("M5.8 9.6 V20 H18.2 V9.6")).S().Poly({ 10, 20, 10, 14.5, 14, 14.5, 14, 20 }));
		Add(Out, FBuilder(TEXT("gearshape"), TEXT("Interface"), TEXT("settings preferences options gear")).Gear(12, 12, 8, 9.2, 7).S().Circle(12, 12, 3));
		Add(Out, FBuilder(TEXT("slider.horizontal.3"), TEXT("Interface"), TEXT("settings filters adjust")).Thin().Line(4, 6, 20, 6).Line(4, 12, 20, 12).Line(4, 18, 20, 18).Normal().S().Dot(8, 6, 2.6f).Dot(15.5, 12, 2.6f).Dot(10, 18, 2.6f));
		Add(Out, FBuilder(TEXT("line.3.horizontal"), TEXT("Interface"), TEXT("menu hamburger list")).Line(4, 6.5, 20, 6.5).Line(4, 12, 20, 12).Line(4, 17.5, 20, 17.5));
		Add(Out, FBuilder(TEXT("line.3.horizontal.decrease"), TEXT("Interface"), TEXT("filter sort")).Line(3.5, 6.5, 20.5, 6.5).Line(6.5, 12, 17.5, 12).Line(9.5, 17.5, 14.5, 17.5));
		Add(Out, FBuilder(TEXT("ellipsis"), TEXT("Interface"), TEXT("more options")).Dot(5.5, 12, 1.6f).Dot(12, 12, 1.6f).Dot(18.5, 12, 1.6f));
		Add(Out, FBuilder(TEXT("magnifyingglass"), TEXT("Interface"), TEXT("search find zoom")).Circle(10.5, 10.5, 6.5, false).S().Line(15.4, 15.4, 20.5, 20.5));
		Add(Out, FBuilder(TEXT("trash"), TEXT("Interface"), TEXT("delete remove bin")).Line(4, 6.5, 20, 6.5).Svg(TEXT("M6 6.5 L7 20 H17 L18 6.5")).Poly({ 9.5, 6.5, 9.5, 4, 14.5, 4, 14.5, 6.5 }).S().Line(10, 10, 10, 16.5).Line(14, 10, 14, 16.5));
		Add(Out, FBuilder(TEXT("pencil"), TEXT("Interface"), TEXT("edit write compose")).Closed({ 4, 20, 5, 15.5, 16, 4.5, 19.5, 8, 8.5, 19 }).S().Line(13.6, 6.9, 17.1, 10.4));
		Add(Out, FBuilder(TEXT("eraser"), TEXT("Interface"), TEXT("erase clear rubber")).Closed({ 9.5, 20, 3.5, 14, 13, 4.5, 20.5, 12, 12.5, 20 }).S().Line(7.8, 9.8, 14.2, 16.2).T().Line(12.5, 20, 20.5, 20));
		Add(Out, FBuilder(TEXT("folder"), TEXT("Interface"), TEXT("directory files")).Svg(TEXT("M3 7 A2 2 0 0 1 5 5 H9.5 L11.5 7 H19 A2 2 0 0 1 21 9 V17.5 A2 2 0 0 1 19 19.5 H5 A2 2 0 0 1 3 17.5 Z"), true).S().Line(3, 10, 21, 10));
		Add(Out, FBuilder(TEXT("doc"), TEXT("Interface"), TEXT("document file page")).Closed({ 6, 3, 14, 3, 19, 8, 19, 21, 6, 21 }).S().Poly({ 14, 3, 14, 8, 19, 8 }));
		Add(Out, FBuilder(TEXT("doc.text"), TEXT("Interface"), TEXT("document text page")).Closed({ 6, 3, 14, 3, 19, 8, 19, 21, 6, 21 }).S().Poly({ 14, 3, 14, 8, 19, 8 }).T().Line(9, 12, 16, 12).Line(9, 15, 16, 15).Line(9, 18, 13.5, 18));
		Add(Out, FBuilder(TEXT("doc.on.doc"), TEXT("Interface"), TEXT("copy duplicate")).RRect(8, 8, 12.5, 13, 2).S().Svg(TEXT("M16 5.5 V5 A2 2 0 0 0 14 3 H6 A2 2 0 0 0 4 5 V14 A2 2 0 0 0 6 16 H6.5")));
		Add(Out, FBuilder(TEXT("clipboard"), TEXT("Interface"), TEXT("paste tasks")).Svg(TEXT("M9 4.5 H6.5 A2 2 0 0 0 4.5 6.5 V19 A2 2 0 0 0 6.5 21 H17.5 A2 2 0 0 0 19.5 19 V6.5 A2 2 0 0 0 17.5 4.5 H15"), false).S().RRect(9, 3, 6, 3, 1.2));
		Add(Out, FBuilder(TEXT("book"), TEXT("Interface"), TEXT("read library documentation")).Svg(TEXT("M12 7 C9.5 5 6.5 4.5 3 5 V18.5 C6.5 18 9.5 18.5 12 20.5 Z"), true).S().Svg(TEXT("M12 7 C14.5 5 17.5 4.5 21 5 V18.5 C17.5 18 14.5 18.5 12 20.5 Z"), true));
		Add(Out, FBuilder(TEXT("bookmark"), TEXT("Interface"), TEXT("save later")).Closed({ 6, 3.5, 18, 3.5, 18, 21, 12, 16, 6, 21 }));
		Add(Out, FBuilder(TEXT("tag"), TEXT("Interface"), TEXT("label price")).Svg(TEXT("M3.5 4.5 V11 L13 20.5 L20.5 13 L11 3.5 H4.5 A1 1 0 0 0 3.5 4.5 Z"), true).S().Dot(7.8, 7.8, 1.4f));
		Add(Out, FBuilder(TEXT("link"), TEXT("Interface"), TEXT("url chain hyperlink")).Svg(TEXT("M10.5 13.5 A3.5 3.5 0 0 0 15.5 13.5 L18.5 10.5 A3.5 3.5 0 0 0 13.5 5.5 L12 7")).S().Svg(TEXT("M13.5 10.5 A3.5 3.5 0 0 0 8.5 10.5 L5.5 13.5 A3.5 3.5 0 0 0 10.5 18.5 L12 17")));
		Add(Out, FBuilder(TEXT("paperclip"), TEXT("Interface"), TEXT("attachment attach")).Svg(TEXT("M19 11 L11.5 18.5 A4.6 4.6 0 0 1 5 12 L12.5 4.5 A3 3 0 0 1 16.8 8.8 L9.6 16 A1.5 1.5 0 0 1 7.5 13.9 L14 7.4")));
		Add(Out, FBuilder(TEXT("pin"), TEXT("Interface"), TEXT("pushpin attach")).Svg(TEXT("M8.5 3.5 H15.5 L14.5 9 L18 13 H6 L9.5 9 Z"), true).S().Line(12, 13, 12, 21));
		Add(Out, FBuilder(TEXT("lock"), TEXT("Interface"), TEXT("secure private password")).RRect(5, 10.5, 14, 10.5, 2.2).S().Svg(TEXT("M8 10.5 V8 A4 4 0 0 1 16 8 V10.5")).T().Line(12, 14.5, 12, 17));
		Add(Out, FBuilder(TEXT("lock.open"), TEXT("Interface"), TEXT("unlock unsecure")).RRect(5, 10.5, 14, 10.5, 2.2).S().Svg(TEXT("M8 10.5 V8 A4 4 0 0 1 15.9 7.2")).T().Line(12, 14.5, 12, 17));
		Add(Out, FBuilder(TEXT("key"), TEXT("Interface"), TEXT("password access")).Circle(7.5, 12, 3.8).S().Line(11.3, 12, 20.5, 12).Line(17.5, 12, 17.5, 15).Line(20.5, 12, 20.5, 14.5));
		Add(Out, FBuilder(TEXT("eye"), TEXT("Interface"), TEXT("view show visible preview")).Svg(TEXT("M2.5 12 C5 7 8.5 5 12 5 C15.5 5 19 7 21.5 12 C19 17 15.5 19 12 19 C8.5 19 5 17 2.5 12 Z"), true).S().Circle(12, 12, 3.2));
		Add(Out, FBuilder(TEXT("calendar"), TEXT("Interface"), TEXT("date event schedule")).RRect(3.5, 5, 17, 15.5, 2.5).Line(3.5, 9.5, 20.5, 9.5).S().Line(8, 3, 8, 6.5).Line(16, 3, 16, 6.5).T().Dot(8, 13.5, 1.2f).Dot(12, 13.5, 1.2f).Dot(16, 13.5, 1.2f).Dot(8, 17, 1.2f).Dot(12, 17, 1.2f));
		Add(Out, FBuilder(TEXT("clock"), TEXT("Interface"), TEXT("time hour watch")).Circle(12, 12, 9).S().Poly({ 12, 7, 12, 12, 15.5, 15.5 }));
		Add(Out, FBuilder(TEXT("alarm"), TEXT("Interface"), TEXT("alarm wake clock")).Circle(12, 13, 7.5).S().Poly({ 12, 9.5, 12, 13, 14.5, 15 }).T().Line(4, 6, 6.5, 3.8).Line(20, 6, 17.5, 3.8).Line(7, 20, 5.5, 21.5).Line(17, 20, 18.5, 21.5));
		Add(Out, FBuilder(TEXT("timer"), TEXT("Interface"), TEXT("countdown stopwatch")).Circle(12, 13, 8).S().Line(12, 13, 14.8, 10.2).T().Line(10, 3, 14, 3).Line(12, 3, 12, 5));
		Add(Out, FBuilder(TEXT("hourglass"), TEXT("Interface"), TEXT("wait time loading")).Line(6, 3.5, 18, 3.5).Line(6, 20.5, 18, 20.5).S().Svg(TEXT("M7.5 3.5 V6 L12 12 L16.5 6 V3.5")).Svg(TEXT("M7.5 20.5 V18 L12 12 L16.5 18 V20.5")));
		Add(Out, FBuilder(TEXT("globe"), TEXT("Interface"), TEXT("world web internet language")).Circle(12, 12, 9).S().Svg(TEXT("M12 3 C8.5 6 8.5 18 12 21 C15.5 18 15.5 6 12 3 Z")).Line(3, 12, 21, 12).T().Svg(TEXT("M4.5 7.5 C9 9 15 9 19.5 7.5")).Svg(TEXT("M4.5 16.5 C9 15 15 15 19.5 16.5")));
		Add(Out, FBuilder(TEXT("map"), TEXT("Places"), TEXT("navigation directions")).Closed({ 3, 6, 9, 3.5, 15, 6, 21, 3.5, 21, 18, 15, 20.5, 9, 18, 3, 20.5 }).S().Line(9, 3.5, 9, 18).Line(15, 6, 15, 20.5));
		Add(Out, FBuilder(TEXT("mappin"), TEXT("Places"), TEXT("location place marker")).Svg(TEXT("M12 21 C12 21 5 14.5 5 9.5 A7 7 0 0 1 19 9.5 C19 14.5 12 21 12 21 Z"), true).S().Circle(12, 9.5, 2.5, false));
		Add(Out, FBuilder(TEXT("location"), TEXT("Places"), TEXT("navigation gps current")).Closed({ 20, 4, 13, 20, 11.5, 12.5, 4, 11 }));
		Add(Out, FBuilder(TEXT("camera"), TEXT("Media"), TEXT("photo picture capture")).Svg(TEXT("M3 9 A2 2 0 0 1 5 7 H7.5 L9 4.5 H15 L16.5 7 H19 A2 2 0 0 1 21 9 V18 A2 2 0 0 1 19 20 H5 A2 2 0 0 1 3 18 Z"), true).S().Circle(12, 13.2, 3.6));
		Add(Out, FBuilder(TEXT("photo"), TEXT("Media"), TEXT("image picture gallery")).RRect(3, 4.5, 18, 15, 2.5).S().Poly({ 3.5, 17.5, 9, 11.5, 13, 15.5, 15.5, 13, 20.5, 18 }).T().Dot(15.5, 8.8, 1.6f));
		Add(Out, FBuilder(TEXT("video"), TEXT("Media"), TEXT("camera movie record")).RRect(3, 6.5, 13, 11, 2.5).S().Closed({ 16, 10.5, 21, 7.5, 21, 16.5, 16, 13.5 }));
		Add(Out, FBuilder(TEXT("mic"), TEXT("Media"), TEXT("microphone voice record audio")).RRect(9, 3, 6, 11, 3).S().Svg(TEXT("M5.5 11 A6.5 6.5 0 0 0 18.5 11")).Line(12, 17.5, 12, 21));
		Add(Out, FBuilder(TEXT("speaker"), TEXT("Media"), TEXT("sound audio volume mute")).Closed({ 4, 9, 8, 9, 13, 5, 13, 19, 8, 15, 4, 15 }));
		Add(Out, FBuilder(TEXT("speaker.wave.3"), TEXT("Media"), TEXT("sound volume audio loud")).Closed({ 3, 9, 7, 9, 12, 5, 12, 19, 7, 15, 3, 15 }).S().Var(0).Arc(12, 12, 3.5, -45, 45).Var(1).Arc(12, 12, 6.5, -48, 48).Var(2).Arc(12, 12, 9.5, -50, 50));
		Add(Out, FBuilder(TEXT("music.note"), TEXT("Media"), TEXT("song audio")).Svg(TEXT("M9 18 V5.5 L18 3.5 V15.5")).S().Circle(6.8, 18, 2.4).Circle(15.8, 15.5, 2.4));
		Add(Out, FBuilder(TEXT("headphones"), TEXT("Media"), TEXT("audio listen music")).Svg(TEXT("M3.5 16 V12 A8.5 8.5 0 0 1 20.5 12 V16")).S().RRect(3.5, 13.5, 4.5, 7, 2).RRect(16, 13.5, 4.5, 7, 2));
		Add(Out, FBuilder(TEXT("play"), TEXT("Media"), TEXT("start resume")).Closed({ 7, 4.5, 19, 12, 7, 19.5 }));
		Add(Out, FBuilder(TEXT("pause"), TEXT("Media"), TEXT("hold")).Line(8.5, 5, 8.5, 19).Line(15.5, 5, 15.5, 19));
		Add(Out, FBuilder(TEXT("stop"), TEXT("Media"), TEXT("end")).RRect(6, 6, 12, 12, 2));
		Add(Out, FBuilder(TEXT("record"), TEXT("Media"), TEXT("rec")).Circle(12, 12, 6.5).Color(TEXT("Red")));
		Add(Out, FBuilder(TEXT("backward"), TEXT("Media"), TEXT("rewind")).Closed({ 11.5, 6, 3.5, 12, 11.5, 18 }).S().Closed({ 20.5, 6, 12.5, 12, 20.5, 18 }));
		Add(Out, FBuilder(TEXT("forward"), TEXT("Media"), TEXT("fast forward")).Closed({ 3.5, 6, 11.5, 12, 3.5, 18 }).S().Closed({ 12.5, 6, 20.5, 12, 12.5, 18 }));
		Add(Out, FBuilder(TEXT("backward.end"), TEXT("Media"), TEXT("previous track")).Closed({ 18.5, 5.5, 8.5, 12, 18.5, 18.5 }).S().Line(6, 5.5, 6, 18.5));
		Add(Out, FBuilder(TEXT("forward.end"), TEXT("Media"), TEXT("next track skip")).Closed({ 5.5, 5.5, 15.5, 12, 5.5, 18.5 }).S().Line(18, 5.5, 18, 18.5));
		Add(Out, FBuilder(TEXT("shuffle"), TEXT("Media"), TEXT("random mix")).Svg(TEXT("M3.5 7 H7 C11 7 13 17 17 17 H20")).Head(20, 17, 0, 3.5).S().Svg(TEXT("M3.5 17 H7 C9 17 10.5 14.5 11.5 12.5 M13 10 C14 8.3 15.3 7 17 7 H20")).Head(20, 7, 0, 3.5));
		Add(Out, FBuilder(TEXT("repeat"), TEXT("Media"), TEXT("loop again")).Svg(TEXT("M4 11 V9.5 A3 3 0 0 1 7 6.5 H19.5")).Head(19.5, 6.5, 0, 3.5).S().Svg(TEXT("M20 13 V14.5 A3 3 0 0 1 17 17.5 H4.5")).Head(4.5, 17.5, 180, 3.5));
		Add(Out, FBuilder(TEXT("film"), TEXT("Media"), TEXT("movie cinema video")).RRect(4, 3, 16, 18, 2).S().Line(8, 3, 8, 21).Line(16, 3, 16, 21).T().Line(4, 9, 8, 9).Line(4, 15, 8, 15).Line(16, 9, 20, 9).Line(16, 15, 20, 15));
		Add(Out, FBuilder(TEXT("waveform"), TEXT("Media"), TEXT("audio sound voice")).Var(0).Line(4, 10.5, 4, 13.5).Var(1).Line(8, 7, 8, 17).Var(2).Line(12, 4, 12, 20).Var(3).Line(16, 8, 16, 16).Var(4).Line(20, 10.5, 20, 13.5));

		// --- Connectivity and devices ------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("wifi"), TEXT("Connectivity"), TEXT("wireless network internet signal")).Dot(12, 18.5, 1.6f).S().Var(0).Arc(12, 18.5, 4.5, -135, -45).Var(1).Arc(12, 18.5, 8.5, -135, -45).Var(2).Arc(12, 18.5, 12.5, -135, -45));
		Add(Out, FBuilder(TEXT("cellularbars"), TEXT("Connectivity"), TEXT("signal network mobile")).Var(0).Line(4.5, 19, 4.5, 16).Var(1).Line(9.5, 19, 9.5, 12.5).Var(2).Line(14.5, 19, 14.5, 9).Var(3).Line(19.5, 19, 19.5, 5.5));
		{
			FBuilder Battery(TEXT("battery"), TEXT("Connectivity"), TEXT("power charge energy level"));
			Battery.RRect(2.5, 7, 17, 10, 2.5).Line(21.5, 10.5, 21.5, 13.5).S().Color(TEXT("Green"));
			for (int32 Bar = 0; Bar < 4; ++Bar)
			{
				Battery.Var(Bar).Line(5.5 + Bar * 3.5, 9.8, 5.5 + Bar * 3.5, 14.2);
			}
			Add(Out, Battery);
		}
		Add(Out, FBuilder(TEXT("power"), TEXT("Connectivity"), TEXT("on off shutdown")).Svg(TEXT("M7.2 6.6 A7.5 7.5 0 1 0 16.8 6.6")).S().Line(12, 3, 12, 11));
		Add(Out, FBuilder(TEXT("bluetooth"), TEXT("Connectivity"), TEXT("wireless")).Poly({ 7, 7.5, 17, 16.5, 12, 20.5, 12, 3.5, 17, 7.5, 7, 16.5 }));
		Add(Out, FBuilder(TEXT("airplane"), TEXT("Places"), TEXT("flight travel offline mode")).Svg(TEXT("M12 2.8 C13 2.8 13.5 4 13.5 5 V9.5 L21 14 V16 L13.5 13.5 V18 L16 19.8 V21.2 L12 20 L8 21.2 V19.8 L10.5 18 V13.5 L3 16 V14 L10.5 9.5 V5 C10.5 4 11 2.8 12 2.8 Z"), true));
		Add(Out, FBuilder(TEXT("car"), TEXT("Places"), TEXT("drive vehicle travel")).Svg(TEXT("M4 16 V12 L6 7 A2 2 0 0 1 7.8 5.7 H16.2 A2 2 0 0 1 18 7 L20 12 V16 Z"), true).S().Circle(7.5, 16.5, 2, false).Circle(16.5, 16.5, 2, false).T().Line(4.5, 12, 19.5, 12));
		Add(Out, FBuilder(TEXT("bicycle"), TEXT("Places"), TEXT("bike ride cycle")).Circle(6, 15.5, 4, false).Circle(18, 15.5, 4, false).S().Poly({ 6, 15.5, 9.5, 8.5, 15.5, 8.5, 18, 15.5 }).Poly({ 9.5, 8.5, 12, 15.5, 15.5, 8.5 }).Line(8.5, 6, 11, 6));
		Add(Out, FBuilder(TEXT("display"), TEXT("Devices"), TEXT("screen monitor computer desktop")).RRect(3, 4, 18, 12.5, 2).S().Line(12, 16.5, 12, 20).Line(8, 20, 16, 20));
		Add(Out, FBuilder(TEXT("laptop"), TEXT("Devices"), TEXT("computer notebook")).RRect(5, 5, 14, 10.5, 1.5).S().Line(2.5, 19, 21.5, 19));
		Add(Out, FBuilder(TEXT("keyboard"), TEXT("Devices"), TEXT("type input")).RRect(2.5, 6, 19, 12, 2).T().Dot(6.5, 10, 1.1f).Dot(10, 10, 1.1f).Dot(14, 10, 1.1f).Dot(17.5, 10, 1.1f).S().Line(8, 14.3, 16, 14.3));
		Add(Out, FBuilder(TEXT("printer"), TEXT("Devices"), TEXT("print paper")).Svg(TEXT("M7 16.5 H5 A2 2 0 0 1 3 14.5 V10 A2 2 0 0 1 5 8 H19 A2 2 0 0 1 21 10 V14.5 A2 2 0 0 1 19 16.5 H17"), false).S().RRect(7, 13, 10, 8, 1, false).Svg(TEXT("M7 8 V3.5 H17 V8")));
		Add(Out, FBuilder(TEXT("gamecontroller"), TEXT("Devices"), TEXT("game play joystick pad")).Svg(TEXT("M7.5 7 H16.5 A4.5 4.5 0 0 1 21 11.5 V15 A2.8 2.8 0 0 1 16.2 17 L14.5 15 H9.5 L7.8 17 A2.8 2.8 0 0 1 3 15 V11.5 A4.5 4.5 0 0 1 7.5 7 Z"), true).S().Line(6, 11.2, 9.5, 11.2).Line(7.75, 9.5, 7.75, 13).T().Dot(15.5, 10.3, 1.2f).Dot(17.5, 12.3, 1.2f));
		Add(Out, FBuilder(TEXT("cpu"), TEXT("Devices"), TEXT("chip processor")).RRect(6, 6, 12, 12, 2).S().RRect(9.5, 9.5, 5, 5, 1, false).T().Line(9.5, 3, 9.5, 6).Line(14.5, 3, 14.5, 6).Line(9.5, 18, 9.5, 21).Line(14.5, 18, 14.5, 21).Line(3, 9.5, 6, 9.5).Line(3, 14.5, 6, 14.5).Line(18, 9.5, 21, 9.5).Line(18, 14.5, 21, 14.5));

		// --- Commerce -----------------------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("cart"), TEXT("Commerce"), TEXT("shop buy basket")).Poly({ 3, 4, 5.8, 4, 8.3, 15.5, 18.5, 15.5, 20.5, 7.5, 6.8, 7.5 }).S().Dot(9, 19.5, 1.4f).Dot(17.5, 19.5, 1.4f));
		Add(Out, FBuilder(TEXT("bag"), TEXT("Commerce"), TEXT("shopping buy")).Svg(TEXT("M5 8 H19 L18.2 19.2 A2 2 0 0 1 16.2 21 H7.8 A2 2 0 0 1 5.8 19.2 Z"), true).S().Svg(TEXT("M9 10.5 V7 A3 3 0 0 1 15 7 V10.5")));
		Add(Out, FBuilder(TEXT("creditcard"), TEXT("Commerce"), TEXT("payment card buy")).RRect(2.5, 5.5, 19, 13, 2.5).S().Line(2.5, 10, 21.5, 10).T().Line(6, 15, 10, 15));
		Add(Out, FBuilder(TEXT("banknote"), TEXT("Commerce"), TEXT("money cash bill")).RRect(2.5, 6, 19, 12, 2).S().Circle(12, 12, 2.8).T().Dot(5.8, 12, 1.1f).Dot(18.2, 12, 1.1f));
		Add(Out, FBuilder(TEXT("dollarsign"), TEXT("Commerce"), TEXT("money price currency")).Svg(TEXT("M16.5 7.5 C15.5 6 14 5.5 12 5.5 C9.5 5.5 7.8 6.8 7.8 8.8 C7.8 13 16.5 11 16.5 15.2 C16.5 17.2 14.6 18.5 12 18.5 C10 18.5 8.3 17.8 7.5 16.3")).S().Line(12, 3, 12, 21));
		Add(Out, FBuilder(TEXT("gift"), TEXT("Commerce"), TEXT("present reward")).RRect(3.5, 8, 17, 4.5, 1).S().Svg(TEXT("M5 12.5 V20 H19 V12.5")).Line(12, 8, 12, 20).T().Svg(TEXT("M12 8 C9 8 7 7 7 5.5 A1.5 1.5 0 0 1 9 4.3 C10.5 5 12 8 12 8 C12 8 13.5 5 15 4.3 A1.5 1.5 0 0 1 17 5.5 C17 7 15 8 12 8")));
		Add(Out, FBuilder(TEXT("ticket"), TEXT("Commerce"), TEXT("pass event admission")).Svg(TEXT("M3 7 A1.5 1.5 0 0 1 4.5 5.5 H19.5 A1.5 1.5 0 0 1 21 7 V9.5 A2.5 2.5 0 0 0 21 14.5 V17 A1.5 1.5 0 0 1 19.5 18.5 H4.5 A1.5 1.5 0 0 1 3 17 V14.5 A2.5 2.5 0 0 0 3 9.5 Z"), true).T().Thin(0.7f).Line(14.5, 6.5, 14.5, 17.5));
		Add(Out, FBuilder(TEXT("chart.bar"), TEXT("Commerce"), TEXT("statistics graph analytics")).Var(0).Line(6, 19, 6, 13).Var(1).Line(12, 19, 12, 8).Var(2).Line(18, 19, 18, 4.5).NoVar().T().Line(3, 21, 21, 21));
		Add(Out, FBuilder(TEXT("chart.line"), TEXT("Commerce"), TEXT("trend graph statistics")).Poly({ 3.5, 17, 9, 11, 13, 14, 20.5, 6 }).T().Poly({ 3.5, 3.5, 3.5, 20.5, 20.5, 20.5 }));
		Add(Out, FBuilder(TEXT("chart.pie"), TEXT("Commerce"), TEXT("statistics share")).Svg(TEXT("M10.5 4.6 A8 8 0 1 0 19.4 13.5 H10.5 Z"), true).S().Svg(TEXT("M13.5 3 A8 8 0 0 1 21 10.5 H13.5 Z"), true));
		Add(Out, FBuilder(TEXT("gauge"), TEXT("Commerce"), TEXT("speed meter performance")).Arc(12, 14, 8.5, 160, 380).S().Line(12, 14, 16.5, 9).T().Dot(12, 14, 1.4f));
		Add(Out, FBuilder(TEXT("briefcase"), TEXT("Commerce"), TEXT("work job business")).RRect(3, 7.5, 18, 12.5, 2).S().Svg(TEXT("M9 7.5 V5.5 A1.5 1.5 0 0 1 10.5 4 H13.5 A1.5 1.5 0 0 1 15 5.5 V7.5")).T().Line(3, 13, 21, 13));

		// --- Objects and tools --------------------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("square.grid.2x2"), TEXT("Interface"), TEXT("grid apps layout")).RRect(3.5, 3.5, 7, 7, 1.8).RRect(13.5, 3.5, 7, 7, 1.8).S().RRect(3.5, 13.5, 7, 7, 1.8).RRect(13.5, 13.5, 7, 7, 1.8));
		Add(Out, FBuilder(TEXT("list.bullet"), TEXT("Interface"), TEXT("list items")).S().Dot(5, 6.5, 1.4f).Dot(5, 12, 1.4f).Dot(5, 17.5, 1.4f).P().Line(9, 6.5, 20, 6.5).Line(9, 12, 20, 12).Line(9, 17.5, 20, 17.5));
		Add(Out, FBuilder(TEXT("checklist"), TEXT("Interface"), TEXT("tasks todo")).S().Poly({ 3.5, 6.5, 5.2, 8.2, 8, 5 }).Poly({ 3.5, 15, 5.2, 16.7, 8, 13.5 }).P().Line(11, 6.8, 20.5, 6.8).Line(11, 15.3, 20.5, 15.3));
		Add(Out, FBuilder(TEXT("text.alignleft"), TEXT("Text"), TEXT("align left paragraph")).Line(4, 6, 20, 6).Line(4, 10, 14, 10).Line(4, 14, 20, 14).Line(4, 18, 12, 18));
		Add(Out, FBuilder(TEXT("text.aligncenter"), TEXT("Text"), TEXT("align center")).Line(4, 6, 20, 6).Line(7, 10, 17, 10).Line(4, 14, 20, 14).Line(8, 18, 16, 18));
		Add(Out, FBuilder(TEXT("text.alignright"), TEXT("Text"), TEXT("align right")).Line(4, 6, 20, 6).Line(10, 10, 20, 10).Line(4, 14, 20, 14).Line(12, 18, 20, 18));
		Add(Out, FBuilder(TEXT("textformat.size"), TEXT("Text"), TEXT("font size typography")).Poly({ 3, 19, 8, 6, 13, 19 }).Line(4.8, 14.5, 11.2, 14.5).S().Poly({ 13.5, 19, 17, 10.5, 20.5, 19 }).Line(14.8, 16, 19.2, 16));
		Add(Out, FBuilder(TEXT("quote.opening"), TEXT("Text"), TEXT("quotation citation")).Svg(TEXT("M10 6 C6.5 7.5 5 10 5 13.5 V17.5 H10 V12.5 H7")).S().Svg(TEXT("M19 6 C15.5 7.5 14 10 14 13.5 V17.5 H19 V12.5 H16")));
		Add(Out, FBuilder(TEXT("rectangle.stack"), TEXT("Interface"), TEXT("layers cards stack")).RRect(3.5, 9, 17, 11.5, 2).S().Line(5.5, 6, 18.5, 6).T().Line(7.5, 3.3, 16.5, 3.3));
		Add(Out, FBuilder(TEXT("sidebar.left"), TEXT("Interface"), TEXT("panel layout")).RRect(3, 4.5, 18, 15, 2.5).S().Line(9.5, 4.5, 9.5, 19.5).T().Line(5.2, 8, 7.4, 8).Line(5.2, 11, 7.4, 11));
		Add(Out, FBuilder(TEXT("window"), TEXT("Interface"), TEXT("app browser")).RRect(3, 4.5, 18, 15, 2.5).S().Line(3, 8.5, 21, 8.5).T().Dot(5.8, 6.5, 1.0f).Dot(8.3, 6.5, 1.0f).Dot(10.8, 6.5, 1.0f));
		Add(Out, FBuilder(TEXT("square.and.arrow.up"), TEXT("Interface"), TEXT("share export send")).Svg(TEXT("M8.5 9.5 H6.5 A1.5 1.5 0 0 0 5 11 V19.5 A1.5 1.5 0 0 0 6.5 21 H17.5 A1.5 1.5 0 0 0 19 19.5 V11 A1.5 1.5 0 0 0 17.5 9.5 H15.5")).S().Line(12, 15, 12, 3.5).Head(12, 3.5, -90, 4));
		Add(Out, FBuilder(TEXT("square.and.arrow.down"), TEXT("Interface"), TEXT("import save receive")).Svg(TEXT("M8.5 9.5 H6.5 A1.5 1.5 0 0 0 5 11 V19.5 A1.5 1.5 0 0 0 6.5 21 H17.5 A1.5 1.5 0 0 0 19 19.5 V11 A1.5 1.5 0 0 0 17.5 9.5 H15.5")).S().Line(12, 3, 12, 15).Head(12, 15, 90, 4));
		Add(Out, FBuilder(TEXT("tray"), TEXT("Interface"), TEXT("inbox")).Svg(TEXT("M3 13 L5.5 5.5 H18.5 L21 13 V18 A2 2 0 0 1 19 20 H5 A2 2 0 0 1 3 18 Z"), true).S().Poly({ 3, 13, 8, 13, 9.5, 15.5, 14.5, 15.5, 16, 13, 21, 13 }));
		Add(Out, FBuilder(TEXT("archivebox"), TEXT("Interface"), TEXT("archive storage box")).RRect(3, 4, 18, 4.5, 1.2).S().Svg(TEXT("M4.5 8.5 V18.5 A1.5 1.5 0 0 0 6 20 H18 A1.5 1.5 0 0 0 19.5 18.5 V8.5")).T().Line(10, 12, 14, 12));
		Add(Out, FBuilder(TEXT("cube"), TEXT("Objects"), TEXT("3d box model package")).Closed({ 12, 3, 20, 7.5, 20, 16.5, 12, 21, 4, 16.5, 4, 7.5 }).S().Poly({ 4, 7.5, 12, 12, 20, 7.5 }).Line(12, 12, 12, 21));
		Add(Out, FBuilder(TEXT("lightbulb"), TEXT("Objects"), TEXT("idea tip hint")).Svg(TEXT("M9 16.5 C9 14.5 6 13 6 9.5 A6 6 0 0 1 18 9.5 C18 13 15 14.5 15 16.5 Z"), true).Color(TEXT("Yellow")).S().Color(nullptr).Line(9.5, 19, 14.5, 19).Line(10.5, 21.3, 13.5, 21.3));
		Add(Out, FBuilder(TEXT("wrench"), TEXT("Tools"), TEXT("tool fix repair settings")).Svg(TEXT("M14.5 3.5 A5 5 0 0 0 10 10.5 L3.8 16.7 A2 2 0 0 0 7.3 20.2 L13.5 14 A5 5 0 0 0 20.5 9.5 L17.5 12.5 L14.5 12 L12 9.5 L11.5 6.5 Z"), true));
		Add(Out, FBuilder(TEXT("hammer"), TEXT("Tools"), TEXT("build tool")).Svg(TEXT("M5 8.5 L11 3.5 L16 4.5 L14 6.5 L17.5 10 L14.5 13 L11 9.5 L9 11.5 Z"), true).S().Line(13.5, 11.5, 20.5, 18.5));
		Add(Out, FBuilder(TEXT("paintbrush"), TEXT("Tools"), TEXT("paint draw art")).Line(20.5, 3.5, 11, 13).S().Svg(TEXT("M11 13 A3 3 0 0 0 7 13.5 C6 15 6.5 17 4 19.5 C7.5 20.5 10.8 19.8 11.8 17.5 A3 3 0 0 0 11 13 Z"), true));
		Add(Out, FBuilder(TEXT("paintpalette"), TEXT("Tools"), TEXT("color art design")).Svg(TEXT("M12 3 A9 9 0 0 0 12 21 C13.5 21 14 20 14 19 C14 17.5 13 17.2 13 16 C13 14.8 14 14 15.5 14 H17.5 A3.5 3.5 0 0 0 21 10.5 C21 6.3 17 3 12 3 Z"), true).S().Color(TEXT("Red")).Dot(7.5, 11, 1.5f).Color(TEXT("Yellow")).Dot(9.5, 7, 1.5f).Color(TEXT("Green")).Dot(14, 6.5, 1.5f).Color(TEXT("Blue")).Dot(17, 9.5, 1.5f));
		Add(Out, FBuilder(TEXT("eyedropper"), TEXT("Tools"), TEXT("color picker sample")).Svg(TEXT("M14.5 6.5 L17.5 9.5 L8.5 18.5 L5 19 L5.5 15.5 Z"), true).S().Svg(TEXT("M13 5 L15.6 2.4 A2 2 0 0 1 18.4 2.4 L21.6 5.6 A2 2 0 0 1 21.6 8.4 L19 11")));
		Add(Out, FBuilder(TEXT("scissors"), TEXT("Tools"), TEXT("cut")).Circle(6.5, 6.5, 3, false).Circle(6.5, 17.5, 3, false).S().Line(8.8, 8.5, 20.5, 18).Line(8.8, 15.5, 20.5, 6));
		Add(Out, FBuilder(TEXT("crop"), TEXT("Tools"), TEXT("trim frame")).Poly({ 6, 3, 6, 18, 21, 18 }).S().Poly({ 3, 6, 18, 6, 18, 21 }));
		Add(Out, FBuilder(TEXT("wand.and.stars"), TEXT("Tools"), TEXT("magic auto enhance")).Line(4, 20, 15, 9).S().Color(TEXT("Yellow")).Star(17.5, 6.5, 4, 3.2, 1.1, 0).Star(10, 4.5, 4, 1.8, 0.6, 0).Star(20, 13, 4, 1.8, 0.6, 0));
		Add(Out, FBuilder(TEXT("crown"), TEXT("Objects"), TEXT("king premium vip")).Closed({ 3.5, 7.5, 8, 11.5, 12, 5, 16, 11.5, 20.5, 7.5, 18.5, 17.5, 5.5, 17.5 }).Color(TEXT("Yellow")).S().Color(nullptr).Line(5.5, 20.5, 18.5, 20.5));
		Add(Out, FBuilder(TEXT("trophy"), TEXT("Objects"), TEXT("award win prize")).Svg(TEXT("M7 4 H17 V9.5 A5 5 0 0 1 7 9.5 Z"), true).Color(TEXT("Yellow")).S().Color(nullptr).Svg(TEXT("M7 6 H4.5 A2.8 2.8 0 0 0 7.2 11.5")).Svg(TEXT("M17 6 H19.5 A2.8 2.8 0 0 1 16.8 11.5")).T().Line(12, 14.5, 12, 18).Line(8, 20.5, 16, 20.5));
		Add(Out, FBuilder(TEXT("medal"), TEXT("Objects"), TEXT("award achievement")).Circle(12, 15, 5.5).Color(TEXT("Yellow")).S().Color(nullptr).Poly({ 8, 10.5, 5.5, 3.5 }).Poly({ 16, 10.5, 18.5, 3.5 }).T().Line(5.5, 3.5, 18.5, 3.5));
		Add(Out, FBuilder(TEXT("target"), TEXT("Objects"), TEXT("goal aim focus")).Circle(12, 12, 9).S().Circle(12, 12, 5.2).T().Dot(12, 12, 1.5f));
		Add(Out, FBuilder(TEXT("dice"), TEXT("Objects"), TEXT("random game chance")).RRect(3.5, 3.5, 17, 17, 3.5).S().Dot(8, 8, 1.4f).Dot(16, 8, 1.4f).Dot(12, 12, 1.4f).Dot(8, 16, 1.4f).Dot(16, 16, 1.4f));
		Add(Out, FBuilder(TEXT("cross"), TEXT("Objects"), TEXT("medical health hospital")).Closed({ 9, 3.5, 15, 3.5, 15, 9, 20.5, 9, 20.5, 15, 15, 15, 15, 20.5, 9, 20.5, 9, 15, 3.5, 15, 3.5, 9, 9, 9 }).Color(TEXT("Red")));
		Add(Out, FBuilder(TEXT("cup"), TEXT("Objects"), TEXT("coffee mug drink tea")).Svg(TEXT("M4.5 8 H16.5 V15.5 A4 4 0 0 1 12.5 19.5 H8.5 A4 4 0 0 1 4.5 15.5 Z"), true).S().Svg(TEXT("M16.5 10 H18 A2.5 2.5 0 0 1 18 15 H16.5")).T().Line(8, 3.5, 8, 5.5).Line(12, 3.5, 12, 5.5));
		Add(Out, FBuilder(TEXT("fork.knife"), TEXT("Objects"), TEXT("food restaurant eat")).Line(7, 3, 7, 21).S().Svg(TEXT("M4 3 V8 A3 3 0 0 0 10 8 V3")).T().Svg(TEXT("M17 21 V3 C14.5 4.5 14 7 14 10 V13 H17")));
		Add(Out, FBuilder(TEXT("graduationcap"), TEXT("Objects"), TEXT("school education learn")).Closed({ 12, 4.5, 21.5, 9, 12, 13.5, 2.5, 9 }).S().Svg(TEXT("M6.5 11 V15.5 C9 17.8 15 17.8 17.5 15.5 V11")).T().Line(21.5, 9, 21.5, 14));
		Add(Out, FBuilder(TEXT("building"), TEXT("Places"), TEXT("office company city")).RRect(5, 3, 14, 18, 1.5).S().Dot(9, 7.5, 1.2f).Dot(15, 7.5, 1.2f).Dot(9, 11.5, 1.2f).Dot(15, 11.5, 1.2f).T().Poly({ 10, 21, 10, 16, 14, 16, 14, 21 }));
		Add(Out, FBuilder(TEXT("barcode"), TEXT("Commerce"), TEXT("scan product")).Line(4, 5, 4, 19).Thin(0.6f).Line(6.5, 5, 6.5, 19).Normal().Line(9.5, 5, 9.5, 19).Thin(0.6f).Line(12, 5, 12, 19).Line(14, 5, 14, 19).Normal().Line(17, 5, 17, 19).Thin(0.6f).Line(20, 5, 20, 19));

		// --- Alpha 13 glyphs (redrawn) -----------------------------------------------------------------------
		Add(Out, FBuilder(TEXT("placeholder"), TEXT("Veka"), TEXT("empty missing image")).T().RRect(3.5, 3.5, 17, 17, 3).Thin(0.7f).Line(5.5, 5.5, 18.5, 18.5).Line(18.5, 5.5, 5.5, 18.5));
		Add(Out, FBuilder(TEXT("keylinegrid"), TEXT("Veka"), TEXT("template design grid keyline")).T().Thin(0.5f).RRect(2.5, 2.5, 19, 19, 0, false).Line(2.5, 2.5, 21.5, 21.5).Line(21.5, 2.5, 2.5, 21.5).Line(12, 2.5, 12, 21.5).Line(2.5, 12, 21.5, 12).S().Circle(12, 12, 9, false).Circle(12, 12, 4.5, false).RRect(5, 5, 14, 14, 0, false));
		Add(Out, FBuilder(TEXT("testtube"), TEXT("Veka"), TEXT("test experiment lab science")).Svg(TEXT("M9 3.5 V14.5 A3 3 0 0 0 15 14.5 V3.5")).S().Color(TEXT("Mint")).Svg(TEXT("M9 11 H15 V14.5 A3 3 0 0 1 9 14.5 Z"), true).T().Color(nullptr).Line(7.5, 3.5, 16.5, 3.5));
		Add(Out, FBuilder(TEXT("veka.logo"), TEXT("Veka"), TEXT("veka logo brand")).Closed({ 3.5, 4.5, 8.5, 4.5, 12, 13.5, 15.5, 4.5, 20.5, 4.5, 13.6, 20, 10.4, 20 }).S().Thin(0.8f).Line(6.6, 6.8, 10.6, 16.5).T().Line(17.4, 6.8, 13.4, 16.5));
	}
}
