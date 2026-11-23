// Copyright TheMiniBoss. All Rights Reserved.

#include "VekaBuilders.h"
#include "Layout/VekaStack.h"
#include "Layout/VekaGrid.h"
#include "Layout/VekaScrollView.h"
#include "Primitives/VekaText.h"
#include "Primitives/VekaImage.h"
#include "Primitives/VekaShapeView.h"
#include "Symbols/VekaSymbolView.h"
#include "Styling/CoreStyle.h"
#include "VekaUILog.h"

namespace Veka
{
	FVekaElement Text(const FText& InText)
	{
		FVekaText Primitive;
		Primitive.Text = InText;
		return FVekaElement::Make(Primitive);
	}

	FVekaElement Text(const FString& InString)
	{
		return Text(FText::FromString(InString));
	}

	FVekaElement Image(UObject* Source, bool bResizable)
	{
		FVekaImage Primitive;
		Primitive.Source = Source;
		Primitive.bResizable = bResizable;
		return FVekaElement::Make(Primitive);
	}

	FVekaElement Symbol(FName Name, float VariableValue)
	{
		FVekaSymbolView Primitive;
		Primitive.Name = Name;
		Primitive.VariableValue = FMath::Clamp(VariableValue, -1.f, 1.f);
		return FVekaElement::Make(Primitive);
	}

	// Label: Controls/VekaControls.cpp (follows the label style).

	FVekaElement Shape(const FVekaShape& InShape)
	{
		FVekaShapeView Primitive;
		Primitive.Shape = InShape;
		// Like SwiftUI: an unfilled shape is drawn with the foreground color.
		Primitive.bFill = true;
		Primitive.bFillWithForeground = true;
		return FVekaElement::Make(Primitive);
	}

	FVekaElement Rectangle() { return Shape(FVekaShape::Rectangle()); }
	FVekaElement RoundedRectangle(float Radius, bool bContinuous) { return Shape(FVekaShape::RoundedRectangle(FVekaCornerRadii(Radius), bContinuous)); }
	FVekaElement RoundedRectangle(const FVekaCornerRadii& Radii, bool bContinuous) { return Shape(FVekaShape::RoundedRectangle(Radii, bContinuous)); }
	FVekaElement Capsule() { return Shape(FVekaShape::Capsule()); }
	FVekaElement Circle() { return Shape(FVekaShape::Circle()); }
	FVekaElement Ellipse() { return Shape(FVekaShape::Ellipse()); }
	FVekaElement Path(const FVekaPath& InPath, bool bUnitSpace) { return Shape(FVekaShape::FromPath(InPath, bUnitSpace)); }

	FVekaElement SvgPath(const FString& PathData, bool bUnitSpace)
	{
		FVekaPath Parsed;
		FString Error;
		if (!FVekaPath::FromSvg(PathData, Parsed, &Error))
		{
			UE_LOG(LogVekaUI, Warning, TEXT("Veka::SvgPath: %s"), *Error);
			return FVekaElement();
		}
		return Path(Parsed, bUnitSpace);
	}

	FVekaElement Color(const FVekaPaint& Paint)
	{
		FVekaShapeView Primitive;
		Primitive.Shape = FVekaShape::Rectangle();
		Primitive.bFill = true;
		Primitive.Fill = Paint;
		return FVekaElement::Make(Primitive);
	}

	FVekaElement Spacer(float MinLength)
	{
		FVekaSpacer Primitive;
		Primitive.MinLength = MinLength;
		return FVekaElement::Make(Primitive);
	}

	FVekaElement Empty()
	{
		return FVekaElement();
	}

	FVekaElement Make(FInstancedStruct&& Primitive, TArray<FVekaElement> Children)
	{
		return FVekaElement::MakePrimitive(MoveTemp(Primitive), MoveTemp(Children));
	}

	FVekaElement FContainerBuilder::Children(TArray<FVekaElement> InChildren) const
	{
		FInstancedStruct Copy = Primitive;
		return FVekaElement::MakePrimitive(MoveTemp(Copy), MoveTemp(InChildren));
	}

	FContainerBuilder Stack(EVekaAxis Axis, float Spacing, FVekaAlignment Alignment)
	{
		FVekaStack Primitive;
		Primitive.Axis = Axis;
		Primitive.Spacing = Spacing;
		Primitive.Alignment = Alignment;
		return FContainerBuilder(FInstancedStruct::Make(Primitive));
	}

	FContainerBuilder VStack(float Spacing, EVekaHAlign Alignment)
	{
		return Stack(EVekaAxis::Vertical, Spacing, FVekaAlignment(Alignment, EVekaVAlign::Center));
	}

	FContainerBuilder HStack(float Spacing, EVekaVAlign Alignment)
	{
		return Stack(EVekaAxis::Horizontal, Spacing, FVekaAlignment(EVekaHAlign::Center, Alignment));
	}

	FContainerBuilder ZStack(FVekaAlignment Alignment)
	{
		return Stack(EVekaAxis::Depth, 0.f, Alignment);
	}

	FContainerBuilder Grid(TArray<FVekaGridItem> Columns, float HorizontalSpacing, float VerticalSpacing, FVekaAlignment Alignment)
	{
		FVekaGrid Primitive;
		Primitive.Columns = MoveTemp(Columns);
		Primitive.HorizontalSpacing = HorizontalSpacing;
		Primitive.VerticalSpacing = VerticalSpacing;
		Primitive.Alignment = Alignment;
		return FContainerBuilder(FInstancedStruct::Make(Primitive));
	}

	FContainerBuilder ScrollView(EVekaAxis Axis, bool bShowsIndicators)
	{
		FVekaScrollView Primitive;
		Primitive.Axis = Axis;
		Primitive.bShowsIndicators = bShowsIndicators;
		return FContainerBuilder(FInstancedStruct::Make(Primitive));
	}

	FVekaElement ForEach(int32 Count, TFunctionRef<FVekaElement(int32)> Build)
	{
		TArray<FVekaElement> Elements;
		Elements.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Elements.Add(Build(Index).Id(Private::ToKey(Index)));
		}
		return FVekaElement::MakeFragment(MoveTemp(Elements));
	}

	namespace Fonts
	{
		// LT Flode Neue (default Veka family).
		FSlateFontInfo Regular(int32 Size) { return Veka::Typography::MakeFont(nullptr, Size, EVekaFontWeight::Regular, false, Veka::Typography::OpticalTracking(Size)); }
		FSlateFontInfo Bold(int32 Size) { return Veka::Typography::MakeFont(nullptr, Size, EVekaFontWeight::Bold, false, Veka::Typography::OpticalTracking(Size)); }
		FSlateFontInfo LargeTitle() { return Veka::Typography::StyleFont(EVekaTextStyle::LargeTitle); }
		FSlateFontInfo Title() { return Veka::Typography::StyleFont(EVekaTextStyle::Title1); }
		FSlateFontInfo Headline() { return Veka::Typography::StyleFont(EVekaTextStyle::Headline); }
		FSlateFontInfo Body() { return Veka::Typography::StyleFont(EVekaTextStyle::Body); }
		FSlateFontInfo Caption() { return Veka::Typography::StyleFont(EVekaTextStyle::Caption1); }
	}

	FLinearColor Hex(const FString& HexColor)
	{
		return FLinearColor(FColor::FromHex(HexColor));
	}
}
