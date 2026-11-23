// Copyright TheMiniBoss. All Rights Reserved.

#include "Core/VekaSchema.h"
#include "Core/VekaPrimitive.h"
#include "Core/VekaModifier.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

FVekaSchema& FVekaSchema::Get()
{
	static FVekaSchema Instance;
	if (!Instance.bInitialized)
	{
		Instance.Refresh();
	}
	return Instance;
}

void FVekaSchema::Refresh()
{
	bInitialized = true;
	Entries.Reset();

	const UScriptStruct* PrimitiveBase = FVekaPrimitive::StaticStruct();
	const UScriptStruct* ModifierBase = FVekaModifier::StaticStruct();

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		const UScriptStruct* Struct = *It;
		if (Struct == ModifierBase)
		{
			continue;
		}

		EVekaSchemaKind Kind;
		if (Struct->IsChildOf(PrimitiveBase))
		{
			Kind = EVekaSchemaKind::Primitive;
		}
		else if (Struct->IsChildOf(ModifierBase))
		{
			Kind = EVekaSchemaKind::Modifier;
		}
		else
		{
			continue;
		}

		FVekaSchemaEntry Entry;
		Entry.Kind = Kind;
		Entry.Struct = Struct;
		Entry.Name = MakeName(Struct, Kind);
#if WITH_EDITORONLY_DATA
		Entry.Category = Struct->GetMetaData(TEXT("VekaCategory"));
#endif
		Entries.Add(MoveTemp(Entry));
	}

	Entries.Sort([](const FVekaSchemaEntry& A, const FVekaSchemaEntry& B)
	{
		if (A.Kind != B.Kind)
		{
			return A.Kind < B.Kind;
		}
		return A.Name.LexicalLess(B.Name);
	});
}

const TArray<FVekaSchemaEntry>& FVekaSchema::GetEntries() const
{
	return Entries;
}

const FVekaSchemaEntry* FVekaSchema::Find(FName Name, EVekaSchemaKind Kind) const
{
	return Entries.FindByPredicate([Name, Kind](const FVekaSchemaEntry& Entry) { return Entry.Kind == Kind && Entry.Name == Name; });
}

const FVekaSchemaEntry* FVekaSchema::FindByStruct(const UScriptStruct* Struct) const
{
	return Entries.FindByPredicate([Struct](const FVekaSchemaEntry& Entry) { return Entry.Struct == Struct; });
}

FName FVekaSchema::MakeName(const UScriptStruct* Struct, EVekaSchemaKind Kind)
{
	if (!Struct)
	{
		return NAME_None;
	}

#if WITH_EDITORONLY_DATA
	const FString& MetaName = Struct->GetMetaData(TEXT("VekaName"));
	if (!MetaName.IsEmpty())
	{
		return FName(*MetaName);
	}
#endif

	// Fallback that also works in cooked builds (no metadata there): VekaPaddingModifier -> padding.
	FString Name = Struct->GetName();
	Name.RemoveFromStart(TEXT("Veka"));
	if (Kind == EVekaSchemaKind::Modifier)
	{
		Name.RemoveFromEnd(TEXT("Modifier"));
		if (Name.Len() > 0)
		{
			Name[0] = FChar::ToLower(Name[0]);
		}
	}
	return FName(*Name);
}
