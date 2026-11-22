// Copyright TheMiniBoss. All Rights Reserved.

#include "View/VekaCatalog.h"
#include "Core/VekaTypes.h"
#include "Typography/VekaTypography.h"
#include "Controls/VekaControlStyles.h"
#include "Symbols/VekaSymbolView.h"

const FVekaCatalogArgument* FVekaCatalogEntry::FindArgument(FName Argument) const
{
	return Arguments.FindByPredicate([Argument](const FVekaCatalogArgument& Item) { return Item.Name == Argument; });
}

namespace Veka::Catalog
{
	namespace
	{
		using EKind = EVekaArgumentKind;

		FVekaCatalogArgument A(const TCHAR* Name, EKind Kind = EKind::Any, const UEnum* Enum = nullptr)
		{
			FVekaCatalogArgument Out;
			Out.Name = Name;
			Out.Kind = Kind;
			Out.Enum = Enum;
			return Out;
		}

		FVekaCatalogEntry E(const TCHAR* Name, TArray<FVekaCatalogArgument> Arguments, const TCHAR* Primary = nullptr, bool bContainer = false, bool bActions = false)
		{
			FVekaCatalogEntry Out;
			Out.Name = Name;
			Out.Arguments = MoveTemp(Arguments);
			Out.PrimaryArgument = Primary ? FName(Primary) : NAME_None;
			Out.bContainer = bContainer;
			Out.bActions = bActions;
			return Out;
		}
	}

	const TArray<FVekaCatalogEntry>& Components()
	{
		// Must match the components the interpreter builds by name (VekaInterpreter.cpp, MakeElement).
		static const TArray<FVekaCatalogEntry> Entries = {
			E(TEXT("VStack"), { A(TEXT("spacing"), EKind::Number), A(TEXT("alignment"), EKind::Enum, StaticEnum<EVekaHAlign>()) }, nullptr, true),
			E(TEXT("HStack"), { A(TEXT("spacing"), EKind::Number), A(TEXT("alignment"), EKind::Enum, StaticEnum<EVekaVAlign>()) }, nullptr, true),
			E(TEXT("ZStack"), {}, nullptr, true),
			E(TEXT("Text"), { A(TEXT("text"), EKind::Text) }, TEXT("text")),
			E(TEXT("Symbol"), { A(TEXT("name"), EKind::Name), A(TEXT("value"), EKind::Number) }, TEXT("name")),
			E(TEXT("Label"), { A(TEXT("title"), EKind::Text), A(TEXT("symbol"), EKind::Name) }, TEXT("title")),
			E(TEXT("Button"), { A(TEXT("title"), EKind::Text), A(TEXT("symbol"), EKind::Name), A(TEXT("role"), EKind::Enum, StaticEnum<EVekaButtonRole>()) }, TEXT("title"), true, true),
			E(TEXT("Toggle"), { A(TEXT("title"), EKind::Text), A(TEXT("isOn"), EKind::Binding) }, TEXT("title")),
			E(TEXT("Slider"), { A(TEXT("value"), EKind::Binding), A(TEXT("min"), EKind::Number), A(TEXT("max"), EKind::Number), A(TEXT("step"), EKind::Number) }, TEXT("value")),
			E(TEXT("Stepper"), { A(TEXT("title"), EKind::Text), A(TEXT("value"), EKind::Binding), A(TEXT("min"), EKind::Number), A(TEXT("max"), EKind::Number),
				A(TEXT("step"), EKind::Number), A(TEXT("decimals"), EKind::Number) }, TEXT("title")),
			E(TEXT("TextField"), { A(TEXT("placeholder"), EKind::Text), A(TEXT("text"), EKind::Binding) }, TEXT("placeholder")),
			E(TEXT("ProgressView"), { A(TEXT("value"), EKind::Number), A(TEXT("label"), EKind::Text) }, TEXT("value")),
			E(TEXT("Gauge"), { A(TEXT("value"), EKind::Number), A(TEXT("min"), EKind::Number), A(TEXT("max"), EKind::Number), A(TEXT("label"), EKind::Text),
				A(TEXT("current"), EKind::Text) }, TEXT("value")),
			E(TEXT("Divider"), {}),
			E(TEXT("Spacer"), { A(TEXT("min"), EKind::Number) }, TEXT("min")),
			E(TEXT("Rectangle"), { A(TEXT("fill"), EKind::Paint) }),
			E(TEXT("RoundedRectangle"), { A(TEXT("radius"), EKind::Number), A(TEXT("fill"), EKind::Paint) }, TEXT("radius")),
			E(TEXT("Capsule"), { A(TEXT("fill"), EKind::Paint) }),
			E(TEXT("Circle"), { A(TEXT("fill"), EKind::Paint) }),
			E(TEXT("Ellipse"), { A(TEXT("fill"), EKind::Paint) }),
		};
		return Entries;
	}

	const TArray<FVekaCatalogEntry>& Modifiers()
	{
		// Must match VekaInterpreter.cpp, ApplyModifier.
		static const TArray<FVekaCatalogEntry> Entries = {
			E(TEXT("onTap"), {}, nullptr, false, true),
			E(TEXT("on"), { A(TEXT("event"), EKind::Name) }, TEXT("event"), false, true),
			E(TEXT("padding"), { A(TEXT("value"), EKind::Number), A(TEXT("horizontal"), EKind::Number), A(TEXT("vertical"), EKind::Number) }, TEXT("value")),
			E(TEXT("frame"), { A(TEXT("width"), EKind::Number), A(TEXT("height"), EKind::Number), A(TEXT("fillWidth"), EKind::Bool),
				A(TEXT("alignment"), EKind::Enum, StaticEnum<EVekaHAlign>()) }),
			E(TEXT("background"), { A(TEXT("paint"), EKind::Paint), A(TEXT("radius"), EKind::Number) }, TEXT("paint")),
			E(TEXT("foreground"), { A(TEXT("paint"), EKind::Paint) }, TEXT("paint")),
			E(TEXT("border"), { A(TEXT("paint"), EKind::Paint), A(TEXT("width"), EKind::Number), A(TEXT("radius"), EKind::Number) }, TEXT("paint")),
			E(TEXT("opacity"), { A(TEXT("value"), EKind::Number) }, TEXT("value")),
			E(TEXT("clip"), {}),
			E(TEXT("offset"), { A(TEXT("x"), EKind::Number), A(TEXT("y"), EKind::Number) }),
			E(TEXT("scale"), { A(TEXT("value"), EKind::Number) }, TEXT("value")),
			E(TEXT("rotation"), { A(TEXT("degrees"), EKind::Number) }, TEXT("degrees")),
			E(TEXT("font"), { A(TEXT("style"), EKind::Enum, StaticEnum<EVekaTextStyle>()) }, TEXT("style")),
			E(TEXT("fontWeight"), { A(TEXT("weight"), EKind::Enum, StaticEnum<EVekaFontWeight>()) }, TEXT("weight")),
			E(TEXT("fontSize"), { A(TEXT("size"), EKind::Number) }, TEXT("size")),
			E(TEXT("disabled"), { A(TEXT("value"), EKind::Bool) }, TEXT("value")),
			E(TEXT("buttonStyle"), { A(TEXT("style"), EKind::Enum, StaticEnum<EVekaButtonStyleKind>()) }, TEXT("style")),
			E(TEXT("toggleStyle"), { A(TEXT("style"), EKind::Enum, StaticEnum<EVekaToggleStyleKind>()) }, TEXT("style")),
			E(TEXT("controlSize"), { A(TEXT("size"), EKind::Enum, StaticEnum<EVekaControlSize>()) }, TEXT("size")),
			E(TEXT("symbolRenderingMode"), { A(TEXT("mode"), EKind::Enum, StaticEnum<EVekaSymbolRenderingMode>()) }, TEXT("mode")),
			E(TEXT("animation"), { A(TEXT("value")) }, TEXT("value")),
			E(TEXT("transition"), { A(TEXT("kind"), EKind::Name) }, TEXT("kind")),
			E(TEXT("style"), { A(TEXT("name"), EKind::Style) }, TEXT("name")),
			E(TEXT("backdropBlur"), { A(TEXT("radius"), EKind::Number), A(TEXT("cornerRadius"), EKind::Number), A(TEXT("tint"), EKind::Paint),
				A(TEXT("tintAmount"), EKind::Number), A(TEXT("saturation"), EKind::Number) }, TEXT("radius")),
			E(TEXT("frostedGlass"), { A(TEXT("blur"), EKind::Number), A(TEXT("cornerRadius"), EKind::Number), A(TEXT("tint"), EKind::Paint), A(TEXT("noise"), EKind::Number) }),
			E(TEXT("glass"), { A(TEXT("cornerRadius"), EKind::Number), A(TEXT("blur"), EKind::Number), A(TEXT("refraction"), EKind::Number), A(TEXT("bezel"), EKind::Number),
				A(TEXT("aberration"), EKind::Number), A(TEXT("highlight"), EKind::Number), A(TEXT("tint"), EKind::Paint), A(TEXT("tintAmount"), EKind::Number) }, TEXT("cornerRadius")),
			E(TEXT("glassGroup"), { A(TEXT("smoothness"), EKind::Number), A(TEXT("blur"), EKind::Number), A(TEXT("refraction"), EKind::Number), A(TEXT("bezel"), EKind::Number),
				A(TEXT("aberration"), EKind::Number), A(TEXT("highlight"), EKind::Number), A(TEXT("tint"), EKind::Paint), A(TEXT("tintAmount"), EKind::Number) }, TEXT("smoothness")),
		};
		return Entries;
	}

	const FVekaCatalogEntry* FindComponent(FName Name)
	{
		return Components().FindByPredicate([Name](const FVekaCatalogEntry& Entry) { return Entry.Name == Name; });
	}

	const FVekaCatalogEntry* FindModifier(FName Name)
	{
		return Modifiers().FindByPredicate([Name](const FVekaCatalogEntry& Entry) { return Entry.Name == Name; });
	}
}
