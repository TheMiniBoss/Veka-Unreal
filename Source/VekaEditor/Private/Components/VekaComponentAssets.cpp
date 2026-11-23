// Copyright TheMiniBoss. All Rights Reserved.

#include "Components/VekaComponentAssets.h"
#include "View/VekaComponentLibrary.h"
#include "View/VekaViewAsset.h"
#include "VekaLanguage.h"
#include "AssetTypeCategories.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#define LOCTEXT_NAMESPACE "VekaComponentAssets"

namespace Veka::Editor::Components
{
	void CompileStyle(FVekaSharedStyle& Style)
	{
		// The modifiers are parsed on a placeholder view: lines 1 and 2 are the wrapper.
		constexpr int32 WrapperLines = 2;
		TArray<FString> Lines;
		Style.Modifiers.ParseIntoArrayLines(Lines, false);
		FString Source = TEXT("view Style {\n    ZStack {\n    }\n");
		for (const FString& Line : Lines)
		{
			Source += TEXT("    ") + Line.TrimStartAndEnd() + TEXT("\n");
		}
		Source += TEXT("}\n");

		FVekaParseOptions Options;
		Options.TextNamespace = TEXT("Veka.Style.") + Style.Name.ToString();
		FVekaParseResult Result = Veka::Lang::Parse(Source, Options);
		TArray<FVekaDiagnostic> Diagnostics = Result.Diagnostics;
		if (!Result.HasErrors())
		{
			Diagnostics.Append(Veka::Lang::Analyze(Result.Document));
		}
		const FVekaAstNode* Root = Result.Document.Nodes.IsValidIndex(Result.Document.Root) ? &Result.Document.Nodes[Result.Document.Root] : nullptr;
		if (Root && (!Root->Children.IsEmpty() || !Root->Actions.IsEmpty()))
		{
			FVekaDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
			Diagnostic.Message = LOCTEXT("OnlyModifiers", "A style holds modifiers only (lines starting with '.')");
		}

		TArray<FString> Errors;
		bool bError = false;
		for (FVekaDiagnostic& Diagnostic : Diagnostics)
		{
			Diagnostic.Range.Line = FMath::Max(0, Diagnostic.Range.Line - WrapperLines - 1);
			bError |= Diagnostic.Severity == EVekaDiagnosticSeverity::Error;
			Errors.Add(Diagnostic.ToString());
		}
		Style.CompileErrors = FString::Join(Errors, TEXT("\n"));
		Style.Compiled = bError || !Root ? FVekaDocument() : MoveTemp(Result.Document);
	}

	TArray<FComponentEntry> GatherComponents(const UVekaView* Except)
	{
		TArray<FComponentEntry> Out;
		TSet<const UVekaView*> Listed;
		for (const UVekaComponentLibrary* Library : Veka::Components::GetLibraries())
		{
			for (UVekaView* View : Library->Components)
			{
				if (View && View != Except && !Listed.Contains(View))
				{
					Listed.Add(View);
					Out.Add({ View, Library->GetLibraryName() });
				}
			}
		}
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(UVekaView::StaticClass()->GetClassPathName(), Assets, true);
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		for (const FAssetData& Asset : Assets)
		{
			if (Except && Asset.AssetName == Except->GetFName() && Asset.PackageName == Except->GetPackage()->GetFName())
			{
				continue;
			}
			UVekaView* View = Cast<UVekaView>(Asset.GetAsset());
			if (View && View != Except && !Listed.Contains(View))
			{
				Listed.Add(View);
				Out.Add({ View, LOCTEXT("ProjectGroup", "Views of the project") });
			}
		}
		return Out;
	}

	FString DescribeComponent(const UVekaView& View)
	{
		TArray<FString> Params;
		for (const FVekaParamDeclaration& Param : View.Document.Params)
		{
			FString Item = Param.Name.ToString() + TEXT(": ") + Veka::Lang::TypeName(Param.Type);
			if (!Param.Default.IsNone())
			{
				Item += TEXT(" = ") + Veka::Lang::PrintValue(Param.Default);
			}
			Params.Add(Item);
		}
		TArray<FString> Slots;
		for (const FVekaSlotDeclaration& Slot : View.Document.Slots)
		{
			Slots.Add(Slot.Name.IsNone() ? FString(TEXT("default")) : Slot.Name.ToString());
		}
		FString Out = Params.IsEmpty() ? FString(TEXT("no parameter")) : FString::Join(Params, TEXT(", "));
		if (!Slots.IsEmpty())
		{
			Out += TEXT(" · slots: ") + FString::Join(Slots, TEXT(", "));
		}
		return Out;
	}
}

UVekaComponentLibraryFactory::UVekaComponentLibraryFactory()
{
	SupportedClass = UVekaComponentLibrary::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UVekaComponentLibraryFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UVekaComponentLibrary* Library = NewObject<UVekaComponentLibrary>(InParent, InClass, InName, Flags | RF_Transactional);
	FVekaSharedStyle& Card = Library->Styles.AddDefaulted_GetRef();
	Card.Name = TEXT("Card");
	Card.Description = LOCTEXT("CardStyle", "Padded content on the card background.");
	Card.Modifiers = TEXT(".padding(16)\n.background(.WindowBackground, radius: 16)");
	Veka::Editor::Components::CompileStyle(Card);
	return Library;
}

FText UVekaComponentLibraryFactory::GetDisplayName() const
{
	return LOCTEXT("LibraryFactoryName", "Veka Component Library");
}

uint32 UVekaComponentLibraryFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::UI;
}

FText UAssetDefinition_VekaComponentLibrary::GetAssetDisplayName() const
{
	return LOCTEXT("LibraryAsset", "Veka Component Library");
}

FLinearColor UAssetDefinition_VekaComponentLibrary::GetAssetColor() const
{
	return FLinearColor(0.95f, 0.65f, 0.25f);
}

TSoftClassPtr<UObject> UAssetDefinition_VekaComponentLibrary::GetAssetClass() const
{
	return UVekaComponentLibrary::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_VekaComponentLibrary::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { EAssetCategoryPaths::UI };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
