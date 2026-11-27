// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UEnum;

/** What an argument expects (checked by the Veka analyzer, shown by completion). */
enum class EVekaArgumentKind : uint8
{
	Any,
	Number,
	Bool,
	/** Displayed text (localizable). */
	Text,
	/** Plain name: symbol name... */
	Name,
	/** Color, hex string or Colora token. */
	Paint,
	/** Value of Enum (by name: `.headline`). */
	Enum,
	/** Must be a state (`isOn: wifi`): the control writes it back. */
	Binding,
	/** Name of a shared style (UVekaComponentLibrary): `.style(.Card)`. */
	Style
};

struct VEKAUI_API FVekaCatalogArgument
{
	FName Name;
	EVekaArgumentKind Kind = EVekaArgumentKind::Any;
	const UEnum* Enum = nullptr;
};

/** A component or modifier the interpreter knows by name (others come from the schema, by reflection). */
struct VEKAUI_API FVekaCatalogEntry
{
	FName Name;
	TArray<FVekaCatalogArgument> Arguments;
	/** Argument written without its name: `Text("Hi")`, `.padding(16)`. */
	FName PrimaryArgument;
	/** Has children (`VStack { ... }`). */
	bool bContainer = false;
	/** Takes actions (`Button("Buy") { emit buy }`, `.onTap { ... }`). */
	bool bActions = false;

	const FVekaCatalogArgument* FindArgument(FName Argument) const;
};

/** A built-in function of the Veka language (`sin(x)`, `random(seed)`...): numbers in, a number out. */
struct FVekaCatalogFunction
{
	FName Name;
	int32 MinArguments = 1;
	int32 MaxArguments = 1;
	/** How it is called and what it gives (documentation, completion). */
	const TCHAR* Signature = TEXT("");
	const TCHAR* Description = TEXT("");
	/** Gives a color (hsv, rgba) instead of a number: see Veka::Catalog::CallColorFunction. */
	bool bColor = false;
};

/** Plain words for one argument (inspector label, tooltip, unit shown in the field). */
struct FVekaArgumentDoc
{
	FName Name;
	FText Label;
	FText Help;
	/** "px", "pt", "°", "×" or empty. */
	FText Unit;
};

/** Plain words and an icon for a component or a modifier (palette, outliner, inspector, documentation). */
struct FVekaCatalogDoc
{
	FName Name;
	/** "Vertical stack", "Padding"... */
	FText Title;
	/** A symbol of the built-in library ("line.3.horizontal"). */
	FName Icon;
	/** Components: Layout, Content, Controls, Shapes. Modifiers: Layout, Style, Text, Controls, Effects, Animation, Interaction. */
	FText Category;
	/** One sentence: what it does. */
	FText Description;
	TArray<FVekaArgumentDoc> Arguments;

	const FVekaArgumentDoc* FindArgument(FName Argument) const
	{
		return Arguments.FindByPredicate([Argument](const FVekaArgumentDoc& Doc) { return Doc.Name == Argument; });
	}
};

namespace Veka::Catalog
{
	/** Words and icon of a catalog component or modifier (nullptr for an unknown name). */
	VEKAUI_API const FVekaCatalogDoc* FindDoc(FName Name, bool bModifier);
	/** Label of an argument: its documented label, else the name with spaces ("isOn" -> "Is on"). */
	VEKAUI_API FText ArgumentLabel(FName Owner, bool bModifier, FName Argument);

	VEKAUI_API const TArray<FVekaCatalogEntry>& Components();
	VEKAUI_API const TArray<FVekaCatalogEntry>& Modifiers();
	VEKAUI_API const FVekaCatalogEntry* FindComponent(FName Name);
	VEKAUI_API const FVekaCatalogEntry* FindModifier(FName Name);

	/** Built-in functions (math, random). Also: the constant `pi` and `time` (seconds since the view appeared). */
	VEKAUI_API const TArray<FVekaCatalogFunction>& Functions();
	VEKAUI_API const FVekaCatalogFunction* FindFunction(FName Name);
	/** Calls a built-in function (Name from Functions()); 0 for an unknown name. */
	VEKAUI_API double CallFunction(FName Name, TConstArrayView<double> Arguments);
	/** Calls a color function (bColor): hsv(hue in degrees, saturation, value [, alpha]), rgba(r, g, b, a) in 0..1. */
	VEKAUI_API FLinearColor CallColorFunction(FName Name, TConstArrayView<double> Arguments);
}
