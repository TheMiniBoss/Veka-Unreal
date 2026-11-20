// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Core/VekaElement.h"
#include "Core/VekaView.h"
#include "Core/VekaPaint.h"
#include "Layout/VekaGrid.h"
#include "Vector/VekaPath.h"
#include "Vector/VekaShape.h"
#include "Vector/VekaStroke.h"

class UTexture;

/**
 * Declarative C++ API.
 *
 *   using namespace Veka;
 *   VStack(12)(
 *       Text(INVTEXT("Hello")).Font(Fonts::Title()),
 *       HStack()(Circle().Fill(FLinearColor::Red).Frame(24, 24), Spacer(), Text(INVTEXT("Ready")))
 *   ).Padding(16).Background(FLinearColor(0.1f, 0.1f, 0.12f), 12);
 */
namespace Veka
{
	// --- Primitives ------------------------------------------------------------------------------

	VEKAUI_API FVekaElement Text(const FText& InText);
	VEKAUI_API FVekaElement Text(const FString& InString);
	VEKAUI_API FVekaElement Image(UObject* Source, bool bResizable = false);
	VEKAUI_API FVekaElement Shape(const FVekaShape& InShape);
	VEKAUI_API FVekaElement Rectangle();
	VEKAUI_API FVekaElement RoundedRectangle(float Radius, bool bContinuous = false);
	VEKAUI_API FVekaElement RoundedRectangle(const FVekaCornerRadii& Radii, bool bContinuous = false);
	VEKAUI_API FVekaElement Capsule();
	VEKAUI_API FVekaElement Circle();
	VEKAUI_API FVekaElement Ellipse();
	/** Path shape. bUnitSpace = path drawn in 0..1 and stretched to the frame. */
	VEKAUI_API FVekaElement Path(const FVekaPath& InPath, bool bUnitSpace = true);
	/** Path shape from SVG path data ("M0 0 L10 10 Z"). Empty element if the data is invalid. */
	VEKAUI_API FVekaElement SvgPath(const FString& PathData, bool bUnitSpace = false);
	/** Flexible area filled with a paint. */
	VEKAUI_API FVekaElement Color(const FVekaPaint& Paint);
	VEKAUI_API FVekaElement Spacer(float MinLength = 8.f);
	VEKAUI_API FVekaElement Empty();

	/** Any primitive struct (extension API). */
	VEKAUI_API FVekaElement Make(FInstancedStruct&& Primitive, TArray<FVekaElement> Children = TArray<FVekaElement>());

	// --- Containers ----------------------------------------------------------------------------

	/** Returned by container functions; call it with the children: Stack(...)(A, B, C). */
	class VEKAUI_API FContainerBuilder
	{
	public:
		explicit FContainerBuilder(FInstancedStruct&& InPrimitive) : Primitive(MoveTemp(InPrimitive)) {}

		template <typename... TChildren>
		FVekaElement operator()(TChildren&&... InChildren) const
		{
			TArray<FVekaElement> ChildArray;
			ChildArray.Reserve(sizeof...(TChildren));
			(ChildArray.Add(FVekaElement(Forward<TChildren>(InChildren))), ...);
			return Children(MoveTemp(ChildArray));
		}

		FVekaElement Children(TArray<FVekaElement> InChildren) const;

	private:
		FInstancedStruct Primitive;
	};

	/** Unified stack. Spacing between children; Alignment on the cross axis (both axes for Depth). */
	VEKAUI_API FContainerBuilder Stack(EVekaAxis Axis, float Spacing = 8.f, FVekaAlignment Alignment = FVekaAlignment());
	VEKAUI_API FContainerBuilder VStack(float Spacing = 8.f, EVekaHAlign Alignment = EVekaHAlign::Center);
	VEKAUI_API FContainerBuilder HStack(float Spacing = 8.f, EVekaVAlign Alignment = EVekaVAlign::Center);
	VEKAUI_API FContainerBuilder ZStack(FVekaAlignment Alignment = FVekaAlignment());
	VEKAUI_API FContainerBuilder Grid(TArray<FVekaGridItem> Columns, float HorizontalSpacing = 8.f, float VerticalSpacing = 8.f, FVekaAlignment Alignment = FVekaAlignment());
	VEKAUI_API FContainerBuilder ScrollView(EVekaAxis Axis = EVekaAxis::Vertical, bool bShowsIndicators = true);

	// --- Control flow --------------------------------------------------------------------------

	inline FVekaElement If(bool bCondition, FVekaElement Then)
	{
		return bCondition ? MoveTemp(Then) : FVekaElement();
	}

	inline FVekaElement If(bool bCondition, FVekaElement Then, FVekaElement Else)
	{
		return bCondition ? MoveTemp(Then).Id(TEXT("then")) : MoveTemp(Else).Id(TEXT("else"));
	}

	namespace Private
	{
		inline FName ToKey(FName Key) { return Key; }
		inline FName ToKey(const FString& Key) { return FName(*Key); }
		inline FName ToKey(const TCHAR* Key) { return FName(Key); }
		inline FName ToKey(int32 Key) { return FName(*FString::Printf(TEXT("%d"), Key)); }
		inline FName ToKey(int64 Key) { return FName(*FString::Printf(TEXT("%lld"), Key)); }
		inline FName ToKey(const FGuid& Key) { return FName(*Key.ToString()); }
	}

	/** One element per item; KeyFn gives a stable identity (nodes and their state follow the item when the list changes). */
	template <typename TItem, typename TKeyFn, typename TBuildFn>
	FVekaElement ForEach(const TArray<TItem>& Items, TKeyFn&& KeyFn, TBuildFn&& Build)
	{
		TArray<FVekaElement> Elements;
		Elements.Reserve(Items.Num());
		for (const TItem& Item : Items)
		{
			Elements.Add(FVekaElement(Build(Item)).Id(Private::ToKey(KeyFn(Item))));
		}
		return FVekaElement::MakeFragment(MoveTemp(Elements));
	}

	/** Count elements keyed by index. */
	VEKAUI_API FVekaElement ForEach(int32 Count, TFunctionRef<FVekaElement(int32)> Build);

	/** Composite view (FVekaView subclass with VEKA_VIEW). */
	template <typename TView, typename... TArgs>
	FVekaElement View(TArgs&&... Args)
	{
		static_assert(TIsDerivedFrom<TView, FVekaView>::Value, "TView must derive from FVekaView");
		return FVekaElement::MakeView(MakeShared<TView>(Forward<TArgs>(Args)...));
	}

	// --- Helpers -------------------------------------------------------------------------------

	namespace Fonts
	{
		VEKAUI_API FSlateFontInfo Regular(int32 Size = 14);
		VEKAUI_API FSlateFontInfo Bold(int32 Size = 14);
		VEKAUI_API FSlateFontInfo LargeTitle();
		VEKAUI_API FSlateFontInfo Title();
		VEKAUI_API FSlateFontInfo Headline();
		VEKAUI_API FSlateFontInfo Body();
		VEKAUI_API FSlateFontInfo Caption();
	}

	/** sRGB hex ("#RRGGBB" or "#RRGGBBAA") to linear color. */
	VEKAUI_API FLinearColor Hex(const FString& HexColor);
}
