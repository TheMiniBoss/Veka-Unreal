// Copyright TheMiniBoss. All Rights Reserved.

#include "View/VekaComponentLibrary.h"
#include "View/VekaViewAsset.h"
#include "UObject/Package.h"

namespace
{
	TArray<TWeakObjectPtr<UVekaComponentLibrary>>& Registered()
	{
		static TArray<TWeakObjectPtr<UVekaComponentLibrary>> Libraries;
		return Libraries;
	}

	uint32& Revision()
	{
		static uint32 Value = 1;
		return Value;
	}

	bool IsRegistrable(const UVekaComponentLibrary* Library)
	{
		return Library && !Library->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && Library->GetOuter() && Library->GetOuter() != GetTransientPackage();
	}
}

FText UVekaComponentLibrary::GetLibraryName() const
{
	return DisplayName.IsEmpty() ? FText::FromName(GetFName()) : DisplayName;
}

const FVekaSharedStyle* UVekaComponentLibrary::FindStyle(FName Name) const
{
	return Styles.FindByPredicate([Name](const FVekaSharedStyle& Style) { return Style.Name == Name; });
}

void UVekaComponentLibrary::PostInitProperties()
{
	Super::PostInitProperties();
	if (IsRegistrable(this))
	{
		Veka::Components::RegisterLibrary(this);
	}
}

void UVekaComponentLibrary::PostLoad()
{
	Super::PostLoad();
	Veka::Components::RegisterLibrary(this);
}

void UVekaComponentLibrary::BeginDestroy()
{
	Veka::Components::UnregisterLibrary(this);
	Super::BeginDestroy();
}

#if WITH_EDITOR
void UVekaComponentLibrary::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	Veka::Components::CompileStyles(*this);
	Veka::Components::NotifyChanged();
}

void UVekaComponentLibrary::PostEditUndo()
{
	Super::PostEditUndo();
	Veka::Components::NotifyChanged();
}
#endif

namespace Veka::Components
{
	void RegisterLibrary(UVekaComponentLibrary* Library)
	{
		if (Library && !Library->HasAnyFlags(RF_ClassDefaultObject))
		{
			Registered().AddUnique(Library);
			NotifyChanged();
		}
	}

	void UnregisterLibrary(UVekaComponentLibrary* Library)
	{
		Registered().RemoveAll([Library](const TWeakObjectPtr<UVekaComponentLibrary>& Item) { return !Item.IsValid() || Item.Get() == Library; });
		NotifyChanged();
	}

	TArray<UVekaComponentLibrary*> GetLibraries()
	{
		TArray<UVekaComponentLibrary*> Out;
		for (const TWeakObjectPtr<UVekaComponentLibrary>& Item : Registered())
		{
			if (UVekaComponentLibrary* Library = Item.Get())
			{
				Out.Add(Library);
			}
		}
		Out.Sort([](const UVekaComponentLibrary& A, const UVekaComponentLibrary& B) { return A.GetLibraryName().CompareTo(B.GetLibraryName()) < 0; });
		return Out;
	}

	const FVekaSharedStyle* FindStyle(FName Name)
	{
		if (Name.IsNone())
		{
			return nullptr;
		}
		for (const UVekaComponentLibrary* Library : GetLibraries())
		{
			if (const FVekaSharedStyle* Style = Library->FindStyle(Name))
			{
				return Style;
			}
		}
		return nullptr;
	}

	TArray<FName> GetAllStyleNames()
	{
		TArray<FName> Names;
		for (const UVekaComponentLibrary* Library : GetLibraries())
		{
			for (const FVekaSharedStyle& Style : Library->Styles)
			{
				if (!Style.Name.IsNone())
				{
					Names.AddUnique(Style.Name);
				}
			}
		}
		Names.Sort(FNameLexicalLess());
		return Names;
	}

	const UVekaComponentLibrary* FindLibraryOf(const UVekaView* View)
	{
		if (!View)
		{
			return nullptr;
		}
		for (const UVekaComponentLibrary* Library : GetLibraries())
		{
			if (Library->Components.Contains(View))
			{
				return Library;
			}
		}
		return nullptr;
	}

	TFunction<void(FVekaSharedStyle&)>& StyleCompiler()
	{
		static TFunction<void(FVekaSharedStyle&)> Compiler;
		return Compiler;
	}

	void CompileStyles(UVekaComponentLibrary& Library)
	{
		if (const TFunction<void(FVekaSharedStyle&)>& Compiler = StyleCompiler())
		{
			for (FVekaSharedStyle& Style : Library.Styles)
			{
				Compiler(Style);
			}
		}
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
