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
	Binding
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

namespace Veka::Catalog
{
	VEKAUI_API const TArray<FVekaCatalogEntry>& Components();
	VEKAUI_API const TArray<FVekaCatalogEntry>& Modifiers();
	VEKAUI_API const FVekaCatalogEntry* FindComponent(FName Name);
	VEKAUI_API const FVekaCatalogEntry* FindModifier(FName Name);
}
