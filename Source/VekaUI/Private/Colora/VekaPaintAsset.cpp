// Copyright TheMiniBoss. All Rights Reserved.

#include "Colora/VekaPaintAsset.h"
#include "UObject/Package.h"

namespace
{
	TArray<TWeakObjectPtr<UVekaPaintAsset>>& Registered()
	{
		static TArray<TWeakObjectPtr<UVekaPaintAsset>> Assets;
		return Assets;
	}

	uint32& Revision()
	{
		static uint32 Value = 1;
		return Value;
	}

	bool IsRegistrable(const UVekaPaintAsset* Asset)
	{
		return Asset && !Asset->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && Asset->GetOuter() && Asset->GetOuter() != GetTransientPackage();
	}
}

void UVekaPaintAsset::PostInitProperties()
{
	Super::PostInitProperties();
	if (IsRegistrable(this))
	{
		Veka::Paints::Register(this);
	}
}

void UVekaPaintAsset::PostLoad()
{
	Super::PostLoad();
	Veka::Paints::Register(this);
}

void UVekaPaintAsset::BeginDestroy()
{
	Veka::Paints::Unregister(this);
	Super::BeginDestroy();
}

#if WITH_EDITOR
void UVekaPaintAsset::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	Veka::Paints::NotifyChanged();
}

void UVekaPaintAsset::PostEditUndo()
{
	Super::PostEditUndo();
	Veka::Paints::NotifyChanged();
}
#endif

namespace Veka::Paints
{
	void Register(UVekaPaintAsset* Asset)
	{
		if (Asset && !Asset->HasAnyFlags(RF_ClassDefaultObject))
		{
			Registered().AddUnique(Asset);
			NotifyChanged();
		}
	}

	void Unregister(UVekaPaintAsset* Asset)
	{
		Registered().RemoveAll([Asset](const TWeakObjectPtr<UVekaPaintAsset>& Item) { return !Item.IsValid() || Item.Get() == Asset; });
	}

	const FVekaPaint* Find(FName Name)
	{
		if (Name.IsNone())
		{
			return nullptr;
		}
		for (const TWeakObjectPtr<UVekaPaintAsset>& Item : Registered())
		{
			if (const UVekaPaintAsset* Asset = Item.Get())
			{
				if (Asset->GetFName() == Name)
				{
					return &Asset->Paint;
				}
			}
		}
		return nullptr;
	}

	TArray<FName> GetAllNames()
	{
		TArray<FName> Names;
		for (const TWeakObjectPtr<UVekaPaintAsset>& Item : Registered())
		{
			if (const UVekaPaintAsset* Asset = Item.Get())
			{
				Names.AddUnique(Asset->GetFName());
			}
		}
		Names.Sort(FNameLexicalLess());
		return Names;
	}

	uint32 GetRevision()
	{
		return Revision();
	}

	void NotifyChanged()
	{
		++Revision();
	}
}
