// Copyright TheMiniBoss. All Rights Reserved.

#include "Symbols/VekaSvgImport.h"
#include "XmlFile.h"
#include "XmlNode.h"

namespace
{
	/** Scale + translation (rotation and skew are not kept). */
	struct FAffine
	{
		FVector2D Scale = FVector2D(1.0, 1.0);
		FVector2D Translation = FVector2D::ZeroVector;

		/** This, then Parent. */
		FAffine Then(const FAffine& Parent) const
		{
			FAffine Out;
			Out.Scale = Parent.Scale * Scale;
			Out.Translation = Parent.Scale * Translation + Parent.Translation;
			return Out;
		}
	};

	/** Style inherited down the tree. */
	struct FStyle
	{
		FString Fill = TEXT("black");
		FString Stroke = TEXT("none");
		double StrokeWidth = 1.0;
		double Opacity = 1.0;
		double FillOpacity = 1.0;
		FString FillRule = TEXT("nonzero");
		FString Role;
		int32 Variable = INDEX_NONE;
	};

	TArray<double> Numbers(const FString& Text)
	{
		TArray<double> Out;
		FString Current;
		auto Flush = [&]()
		{
			if (!Current.IsEmpty())
			{
				Out.Add(FCString::Atod(*Current));
				Current.Reset();
			}
		};
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR C = Text[Index];
			if (FChar::IsDigit(C) || C == TEXT('.') || C == TEXT('e') || C == TEXT('E') || ((C == TEXT('-') || C == TEXT('+')) && (Current.IsEmpty() || Current.EndsWith(TEXT("e")) || Current.EndsWith(TEXT("E")))))
			{
				if ((C == TEXT('-') || C == TEXT('+')) && !Current.IsEmpty() && !Current.EndsWith(TEXT("e")) && !Current.EndsWith(TEXT("E")))
				{
					Flush();
				}
				Current.AppendChar(C);
			}
			else if (C == TEXT('-'))
			{
				Flush();
				Current.AppendChar(C);
			}
			else
			{
				Flush();
			}
		}
		Flush();
		return Out;
	}

	double Number(const FXmlNode* Node, const TCHAR* Name, double Default = 0.0)
	{
		const FString Value = Node->GetAttribute(Name);
		const TArray<double> Parsed = Numbers(Value);
		return Parsed.IsEmpty() ? Default : Parsed[0];
	}

	bool ParseTransform(const FString& Text, FAffine& Out, TArray<FString>& Warnings)
	{
		Out = FAffine();
		int32 Index = 0;
		while (Index < Text.Len())
		{
			const int32 Open = Text.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
			const int32 Close = Open == INDEX_NONE ? INDEX_NONE : Text.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
			if (Close == INDEX_NONE)
			{
				break;
			}
			const FString Name = Text.Mid(Index, Open - Index).TrimStartAndEnd().Replace(TEXT(","), TEXT(""));
			const TArray<double> Args = Numbers(Text.Mid(Open + 1, Close - Open - 1));
			FAffine Step;
			if (Name == TEXT("translate") && Args.Num() >= 1)
			{
				Step.Translation = FVector2D(Args[0], Args.Num() > 1 ? Args[1] : 0.0);
			}
			else if (Name == TEXT("scale") && Args.Num() >= 1)
			{
				Step.Scale = FVector2D(Args[0], Args.Num() > 1 ? Args[1] : Args[0]);
			}
			else if (Name == TEXT("matrix") && Args.Num() == 6)
			{
				if (!FMath::IsNearlyZero(Args[1]) || !FMath::IsNearlyZero(Args[2]))
				{
					Warnings.AddUnique(TEXT("A rotated or skewed transform was imported without its rotation / skew."));
				}
				Step.Scale = FVector2D(Args[0], Args[3]);
				Step.Translation = FVector2D(Args[4], Args[5]);
			}
			else
			{
				Warnings.AddUnique(FString::Printf(TEXT("Transform '%s' is not supported (ignored)."), *Name));
			}
			// SVG applies the list right to left: the later step is applied to the point first.
			Out = Step.Then(Out);
			Index = Close + 1;
		}
		return true;
	}

	/** Reads presentation attributes and the style attribute into Style. */
	void ApplyStyle(const FXmlNode* Node, FStyle& Style)
	{
		TMap<FString, FString> Values;
		for (const FXmlAttribute& Attribute : Node->GetAttributes())
		{
			Values.Add(Attribute.GetTag().ToLower(), Attribute.GetValue());
		}
		TArray<FString> Declarations;
		Node->GetAttribute(TEXT("style")).ParseIntoArray(Declarations, TEXT(";"));
		for (const FString& Declaration : Declarations)
		{
			FString Key;
			FString Value;
			if (Declaration.Split(TEXT(":"), &Key, &Value))
			{
				Values.Add(Key.TrimStartAndEnd().ToLower(), Value.TrimStartAndEnd());
			}
		}
		if (const FString* Value = Values.Find(TEXT("fill")))
		{
			Style.Fill = Value->TrimStartAndEnd();
		}
		if (const FString* Value = Values.Find(TEXT("stroke")))
		{
			Style.Stroke = Value->TrimStartAndEnd();
		}
		if (const FString* Value = Values.Find(TEXT("stroke-width")))
		{
			const TArray<double> Parsed = Numbers(*Value);
			if (!Parsed.IsEmpty())
			{
				Style.StrokeWidth = Parsed[0];
			}
		}
		if (const FString* Value = Values.Find(TEXT("opacity")))
		{
			Style.Opacity *= FCString::Atod(**Value);
		}
		if (const FString* Value = Values.Find(TEXT("fill-opacity")))
		{
			Style.FillOpacity = FCString::Atod(**Value);
		}
		if (const FString* Value = Values.Find(TEXT("fill-rule")))
		{
			Style.FillRule = *Value;
		}
		if (const FString* Value = Values.Find(TEXT("data-role")))
		{
			Style.Role = Value->ToLower();
		}
		if (const FString* Value = Values.Find(TEXT("class")))
		{
			for (const TCHAR* Role : { TEXT("primary"), TEXT("secondary"), TEXT("tertiary") })
			{
				if (Value->ToLower().Contains(Role))
				{
					Style.Role = Role;
				}
			}
		}
		if (const FString* Value = Values.Find(TEXT("data-variable")))
		{
			Style.Variable = FCString::Atoi(**Value);
		}
	}

	bool IsNone(const FString& Paint)
	{
		return Paint.IsEmpty() || Paint.Equals(TEXT("none"), ESearchCase::IgnoreCase) || Paint.Equals(TEXT("transparent"), ESearchCase::IgnoreCase);
	}

	/** An explicit color worth keeping for multicolor (not black / white / currentColor). */
	TOptional<FLinearColor> ExplicitColor(const FString& Paint)
	{
		FString Text = Paint.TrimStartAndEnd();
		if (Text.StartsWith(TEXT("#")))
		{
			Text = Text.Mid(1);
			if (Text.Len() == 3)
			{
				Text = FString::Printf(TEXT("%c%c%c%c%c%c"), Text[0], Text[0], Text[1], Text[1], Text[2], Text[2]);
			}
			if (Text.Len() == 6)
			{
				const FColor Color = FColor::FromHex(Text);
				if (!(Color.R == Color.G && Color.G == Color.B && (Color.R == 0 || Color.R == 255)))
				{
					return FLinearColor::FromSRGBColor(Color);
				}
			}
			return {};
		}
		if (Text.StartsWith(TEXT("rgb(")))
		{
			const TArray<double> Parts = Numbers(Text);
			if (Parts.Num() >= 3)
			{
				return FLinearColor::FromSRGBColor(FColor(static_cast<uint8>(Parts[0]), static_cast<uint8>(Parts[1]), static_cast<uint8>(Parts[2])));
			}
		}
		return {};
	}

	struct FImporter
	{
		FVekaSymbolDefinition& Definition;
		TArray<FString>& Warnings;
		double GridScale = 1.0;

		void Visit(const FXmlNode* Node, FStyle Style, const FAffine& Transform)
		{
			const FString Tag = Node->GetTag().ToLower();
			if (Tag == TEXT("defs") || Tag == TEXT("clippath") || Tag == TEXT("mask") || Tag == TEXT("title") || Tag == TEXT("desc") || Tag == TEXT("metadata")
				|| Tag == TEXT("style") || Tag == TEXT("lineargradient") || Tag == TEXT("radialgradient") || Tag == TEXT("symbol"))
			{
				return;
			}
			if (Tag == TEXT("text") || Tag == TEXT("image") || Tag == TEXT("use"))
			{
				Warnings.AddUnique(FString::Printf(TEXT("<%s> is not supported (skipped)."), *Tag));
				return;
			}
			ApplyStyle(Node, Style);
			FAffine Local;
			ParseTransform(Node->GetAttribute(TEXT("transform")), Local, Warnings);
			const FAffine Total = Local.Then(Transform);

			FVekaPath Path;
			bool bShape = true;
			if (Tag == TEXT("path"))
			{
				FString Error;
				if (!FVekaPath::FromSvg(Node->GetAttribute(TEXT("d")), Path, &Error))
				{
					Warnings.Add(FString::Printf(TEXT("A path could not be read (%s)."), *Error));
					return;
				}
			}
			else if (Tag == TEXT("rect"))
			{
				const FVekaRect Rect(FVector2D(Number(Node, TEXT("x")), Number(Node, TEXT("y"))), FVector2D(Number(Node, TEXT("width")), Number(Node, TEXT("height"))));
				const double Radius = FMath::Max(Number(Node, TEXT("rx")), Number(Node, TEXT("ry")));
				Radius > 0.0 ? Path.AddRoundedRect(Rect, FVekaCornerRadii(static_cast<float>(Radius))) : Path.AddRect(Rect);
			}
			else if (Tag == TEXT("circle"))
			{
				Path.AddCircle(FVector2D(Number(Node, TEXT("cx")), Number(Node, TEXT("cy"))), Number(Node, TEXT("r")));
			}
			else if (Tag == TEXT("ellipse"))
			{
				const FVector2D Center(Number(Node, TEXT("cx")), Number(Node, TEXT("cy")));
				const FVector2D Radius(Number(Node, TEXT("rx")), Number(Node, TEXT("ry")));
				Path.AddEllipse(FVekaRect(Center - Radius, Radius * 2.0));
			}
			else if (Tag == TEXT("line"))
			{
				Path.MoveTo(FVector2D(Number(Node, TEXT("x1")), Number(Node, TEXT("y1"))));
				Path.LineTo(FVector2D(Number(Node, TEXT("x2")), Number(Node, TEXT("y2"))));
			}
			else if (Tag == TEXT("polyline") || Tag == TEXT("polygon"))
			{
				const TArray<double> Values = Numbers(Node->GetAttribute(TEXT("points")));
				TArray<FVector2D> Points;
				for (int32 Index = 0; Index + 1 < Values.Num(); Index += 2)
				{
					Points.Add(FVector2D(Values[Index], Values[Index + 1]));
				}
				Path.AddPolygon(Points, Tag == TEXT("polygon"));
			}
			else
			{
				bShape = false;
			}

			if (bShape && !Path.IsEmpty())
			{
				AddLayer(Path.Transformed(Total.Scale, Total.Translation), Style, Tag == TEXT("line") || Tag == TEXT("polyline"), Total);
			}
			for (const FXmlNode* Child : Node->GetChildrenNodes())
			{
				Visit(Child, Style, Total);
			}
		}

		void AddLayer(const FVekaPath& Path, const FStyle& Style, bool bOpen, const FAffine& Transform)
		{
			const bool bFill = !bOpen && !IsNone(Style.Fill);
			const bool bStroke = !IsNone(Style.Stroke);
			if (!bFill && !bStroke)
			{
				return;
			}
			FVekaSymbolShape& Shape = Definition.Shapes.AddDefaulted_GetRef();
			Shape.Path = Path;
			Shape.Path.FillRule = Style.FillRule.Equals(TEXT("evenodd"), ESearchCase::IgnoreCase) ? EVekaFillRule::EvenOdd : EVekaFillRule::NonZero;
			Shape.Kind = bFill ? EVekaSymbolShapeKind::Fill : EVekaSymbolShapeKind::Stroke;
			if (!bFill)
			{
				// Width in grid units, relative to the regular symbol weight: the layer follows the weight.
				const double Width = Style.StrokeWidth * 0.5 * (FMath::Abs(Transform.Scale.X) + FMath::Abs(Transform.Scale.Y));
				const float Regular = Veka::Symbols::StrokeWidth(Veka::Symbols::WeightFromFont(EVekaFontWeight::Regular));
				Shape.StrokeScale = Regular > KINDA_SMALL_NUMBER ? static_cast<float>(Width / Regular) : 1.f;
			}
			const double Opacity = Style.Opacity * (bFill ? Style.FillOpacity : 1.0);
			Shape.Role = Style.Role == TEXT("secondary") ? EVekaSymbolRole::Secondary : Style.Role == TEXT("tertiary") ? EVekaSymbolRole::Tertiary
				: Style.Role == TEXT("primary") ? EVekaSymbolRole::Primary : Opacity <= 0.45 ? EVekaSymbolRole::Tertiary : Opacity <= 0.75 ? EVekaSymbolRole::Secondary
				: EVekaSymbolRole::Primary;
			Shape.VariableIndex = Style.Variable;
			if (Style.Variable != INDEX_NONE)
			{
				Definition.VariableSteps = FMath::Max(Definition.VariableSteps, Style.Variable + 1);
			}
			if (const TOptional<FLinearColor> Color = ExplicitColor(bFill ? Style.Fill : Style.Stroke))
			{
				Shape.Color = Color.GetValue();
			}
		}
	};
}

namespace Veka::Editor::Svg
{
	bool ImportSymbol(const FString& SvgText, FVekaSymbolDefinition& OutDefinition, TArray<FString>& OutWarnings, FString* OutError)
	{
		// The engine's XML reader does not take comments nor a DOCTYPE: drop them first.
		FString Text = SvgText;
		for (int32 Start = Text.Find(TEXT("<!--")); Start != INDEX_NONE; Start = Text.Find(TEXT("<!--")))
		{
			const int32 End = Text.Find(TEXT("-->"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
			Text.RemoveAt(Start, End == INDEX_NONE ? Text.Len() - Start : End + 3 - Start);
		}
		for (int32 Start = Text.Find(TEXT("<!DOCTYPE")); Start != INDEX_NONE; Start = Text.Find(TEXT("<!DOCTYPE")))
		{
			const int32 End = Text.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
			Text.RemoveAt(Start, End == INDEX_NONE ? Text.Len() - Start : End + 1 - Start);
		}
		FXmlFile File(Text, EConstructMethod::ConstructFromBuffer);
		const FXmlNode* Root = File.IsValid() ? File.GetRootNode() : nullptr;
		if (!Root || Root->GetTag().ToLower() != TEXT("svg"))
		{
			if (OutError)
			{
				*OutError = File.IsValid() ? TEXT("The root element is not <svg>") : File.GetLastError();
			}
			return false;
		}
		// The drawing area: viewBox, else width / height, else 24 x 24.
		FVector4 Box(0.0, 0.0, 24.0, 24.0);
		const TArray<double> ViewBox = Numbers(Root->GetAttribute(TEXT("viewBox")));
		if (ViewBox.Num() == 4 && ViewBox[2] > 0.0 && ViewBox[3] > 0.0)
		{
			Box = FVector4(ViewBox[0], ViewBox[1], ViewBox[2], ViewBox[3]);
		}
		else if (Number(Root, TEXT("width")) > 0.0 && Number(Root, TEXT("height")) > 0.0)
		{
			Box = FVector4(0.0, 0.0, Number(Root, TEXT("width")), Number(Root, TEXT("height")));
		}
		const double Scale = 24.0 / FMath::Max(Box.Z, Box.W);
		FAffine Grid;
		Grid.Scale = FVector2D(Scale, Scale);
		Grid.Translation = FVector2D(-Box.X * Scale + (24.0 - Box.Z * Scale) * 0.5, -Box.Y * Scale + (24.0 - Box.W * Scale) * 0.5);

		OutDefinition.Shapes.Reset();
		OutDefinition.VariableSteps = 0;
		FImporter Importer{ OutDefinition, OutWarnings, Scale };
		FStyle Style;
		ApplyStyle(Root, Style);
		for (const FXmlNode* Child : Root->GetChildrenNodes())
		{
			Importer.Visit(Child, Style, Grid);
		}
		if (OutDefinition.Shapes.IsEmpty())
		{
			if (OutError)
			{
				*OutError = TEXT("No drawable shape in the SVG");
			}
			return false;
		}
		return true;
	}
}
