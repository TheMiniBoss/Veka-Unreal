// Copyright TheMiniBoss. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UScriptStruct;

enum class EVekaSchemaKind : uint8
{
	Primitive,
	Modifier
};

struct VEKAUI_API FVekaSchemaEntry
{
	/** Name used by the DSL and the inspector ("Text", "Stack", "padding"...). */
	FName Name;
	EVekaSchemaKind Kind = EVekaSchemaKind::Primitive;
	const UScriptStruct* Struct = nullptr;
	FString Category;
};

/**
 * Registry of every component and modifier, built from reflection: any USTRUCT deriving from
 * FVekaPrimitive / FVekaModifier is found automatically (Veka's and the user's).
 * It feeds the DSL parser, auto-completion, the Simple-mode inspector, docs and DesignLab (Phase 3),
 * which is how Simple and Advanced modes stay at parity.
 */
class VEKAUI_API FVekaSchema
{
public:
	static FVekaSchema& Get();

	/** Rescans loaded structs (call after loading a module that adds components). */
	void Refresh();

	const TArray<FVekaSchemaEntry>& GetEntries() const;
	const FVekaSchemaEntry* Find(FName Name, EVekaSchemaKind Kind) const;
	const FVekaSchemaEntry* FindByStruct(const UScriptStruct* Struct) const;

	/** Name for a struct: VekaName metadata (editor), else the struct name without the F/Veka prefix and Modifier suffix. */
	static FName MakeName(const UScriptStruct* Struct, EVekaSchemaKind Kind);

private:
	TArray<FVekaSchemaEntry> Entries;
	bool bInitialized = false;
};
