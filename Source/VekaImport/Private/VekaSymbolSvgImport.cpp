// Copyright TheMiniBoss. All Rights Reserved.

#include "VekaSymbolSvgImport.h"

namespace Veka::Import
{
	namespace
	{
		FString Attribute(const FString& Tag, const TCHAR* Name)
		{
			const FString Key = FString(TEXT(" ")) + Name + TEXT("=\"");
			const int32 Start = Tag.Find(Key, ESearchCase::IgnoreCase);
			if (Start == INDEX_NONE)
			{
				return FString();
			}
			const int32 ValueStart = Start + Key.Len();
			const int32 End = Tag.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			return End == INDEX_NONE ? FString() : Tag.Mid(ValueStart, End - ValueStart);
		}

		double Number(const FString& Tag, const TCHAR* Name)
		{
			return FCString::Atod(*Attribute(Tag, Name));
		}

		TArray<FVector2D> Points(const FString& Text)
		{
			FString Clean = Text.Replace(TEXT(","), TEXT(" "));
			TArray<FString> Parts;
			Clean.ParseIntoArrayWS(Parts);
			TArray<FVector2D> Out;
			for (int32 Index = 0; Index + 1 < Parts.Num(); Index += 2)
			{
				Out.Add(FVector2D(FCString::Atod(*Parts[Index]), FCString::Atod(*Parts[Index + 1])));
			}
			return Out;
		}

		EVekaSymbolRole Role(const FString& Tag)
		{
			const FString Text = (Attribute(Tag, TEXT("class")) + TEXT(" ") + Attribute(Tag, TEXT("id")) + TEXT(" ") + Attribute(Tag, TEXT("data-role"))).ToLower();
			if (Text.Contains(TEXT("tertiary")))
			{
				return EVekaSymbolRole::Tertiary;
			}
			return Text.Contains(TEXT("secondary")) ? EVekaSymbolRole::Secondary : EVekaSymbolRole::Primary;
		}
	}

	bool ImportSymbolSvg(const FString& SvgText, FName Name, FVekaSymbolDefinition& OutDefinition, FString* OutError)
	{
		OutDefinition = FVekaSymbolDefinition();
		OutDefinition.Name = Name;
		OutDefinition.Category = TEXT("Imported");

		// viewBox -> 24 grid.
		FVector2D Origin = FVector2D::ZeroVector;
		double Scale = 1.0;
		const int32 SvgStart = SvgText.Find(TEXT("<svg"), ESearchCase::IgnoreCase);
		if (SvgStart == INDEX_NONE)
		{
			if (OutError) { *OutError = TEXT("No <svg> element"); }
			return false;
		}
		const int32 SvgEnd = SvgText.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SvgStart);
		const FString SvgTag = SvgText.Mid(SvgStart, SvgEnd - SvgStart);
		const TArray<FVector2D> ViewBox = Points(Attribute(SvgTag, TEXT("viewBox")));
		if (ViewBox.Num() == 2 && ViewBox[1].X > 0.0 && ViewBox[1].Y > 0.0)
		{
			Origin = ViewBox[0];
			Scale = 24.0 / FMath::Max(ViewBox[1].X, ViewBox[1].Y);
		}

		const TCHAR* Kinds[] = { TEXT("path"), TEXT("circle"), TEXT("rect"), TEXT("line"), TEXT("polyline"), TEXT("polygon") };
		int32 Cursor = SvgEnd;
		while (true)
		{
			const int32 Open = SvgText.Find(TEXT("<"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor + 1);
			if (Open == INDEX_NONE)
			{
				break;
			}
			const int32 Close = SvgText.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
			if (Close == INDEX_NONE)
			{
				break;
			}
			Cursor = Close;
			const FString Tag = SvgText.Mid(Open, Close - Open);
			FString Element = Tag.Mid(1);
			int32 Space = INDEX_NONE;
			if (Element.FindChar(TEXT(' '), Space))
			{
				Element.LeftInline(Space);
			}

			FVekaPath Path;
			bool bKnown = true;
			if (Element == Kinds[0])
			{
				if (!FVekaPath::FromSvg(Attribute(Tag, TEXT("d")), Path, OutError))
				{
					return false;
				}
			}
			else if (Element == Kinds[1])
			{
				Path.AddCircle(FVector2D(Number(Tag, TEXT("cx")), Number(Tag, TEXT("cy"))), Number(Tag, TEXT("r")));
			}
			else if (Element == Kinds[2])
			{
				const FVekaRect Rect(FVector2D(Number(Tag, TEXT("x")), Number(Tag, TEXT("y"))), FVector2D(Number(Tag, TEXT("width")), Number(Tag, TEXT("height"))));
				Path.AddRoundedRect(Rect, FVekaCornerRadii(static_cast<float>(Number(Tag, TEXT("rx")))));
			}
			else if (Element == Kinds[3])
			{
				Path.MoveTo(FVector2D(Number(Tag, TEXT("x1")), Number(Tag, TEXT("y1")))).LineTo(FVector2D(Number(Tag, TEXT("x2")), Number(Tag, TEXT("y2"))));
			}
			else if (Element == Kinds[4] || Element == Kinds[5])
			{
				Path.AddPolygon(Points(Attribute(Tag, TEXT("points"))), Element == Kinds[5]);
			}
			else
			{
				bKnown = false;
			}
			if (!bKnown || Path.IsEmpty())
			{
				continue;
			}

			// Flatten in source units, then map to the grid (keeps arcs exact under any viewBox).
			TArray<FVekaPolyline> Polylines;
			Path.Flatten(Polylines, 0.04 / Scale);
			FVekaPath Mapped;
			for (const FVekaPolyline& Polyline : Polylines)
			{
				TArray<FVector2D> Grid;
				for (const FVector2D& Point : Polyline.Points)
				{
					Grid.Add((Point - Origin) * Scale);
				}
				Mapped.AddPolygon(Grid, Polyline.bClosed);
			}

			const FString Fill = Attribute(Tag, TEXT("fill")).ToLower();
			FVekaSymbolShape& Shape = OutDefinition.Shapes.AddDefaulted_GetRef();
			Shape.Path = MoveTemp(Mapped);
			Shape.Role = Role(Tag);
			Shape.Kind = (Fill.IsEmpty() || Fill == TEXT("none")) ? EVekaSymbolShapeKind::Stroke : EVekaSymbolShapeKind::Fill;
		}

		if (OutDefinition.Shapes.IsEmpty())
		{
			if (OutError) { *OutError = TEXT("No drawable element"); }
			return false;
		}
		return true;
	}
}
