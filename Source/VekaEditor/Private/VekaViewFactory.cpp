// Copyright TheMiniBoss. All Rights Reserved.

#include "VekaViewFactory.h"
#include "View/VekaViewAsset.h"
#include "Samples/VekaSampleView.h"
#include "AssetTypeCategories.h"
#include "VekaLanguage.h"
#include "Misc/FeedbackContext.h"

#define LOCTEXT_NAMESPACE "VekaEditor"

UVekaViewFactory::UVekaViewFactory()
{
	SupportedClass = UVekaView::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UVekaViewFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UVekaView* View = NewObject<UVekaView>(InParent, InClass, InName, Flags);
	View->Document = Veka::Samples::MakeStarterDocument();
	return View;
}

FText UVekaViewFactory::GetDisplayName() const
{
	return LOCTEXT("VekaViewFactoryName", "Veka View");
}

uint32 UVekaViewFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::UI;
}

UVekaViewImportFactory::UVekaViewImportFactory()
{
	SupportedClass = UVekaView::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	bText = true;
	Formats.Add(TEXT("vk;Veka view"));
}

UObject* UVekaViewImportFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type,
	const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	const FString Source(UE_PTRDIFF_TO_INT32(BufferEnd - Buffer), Buffer);
	FVekaParseOptions Options;
	Options.ResolveComponent = Veka::Lang::MakeAssetResolver();
	const FVekaParseResult Result = Veka::Lang::Parse(Source, Options);

	TArray<FVekaDiagnostic> Diagnostics = Result.Diagnostics;
	if (!Result.HasErrors())
	{
		Diagnostics.Append(Veka::Lang::Analyze(Result.Document));
	}
	for (const FVekaDiagnostic& Diagnostic : Diagnostics)
	{
		const FString Line = FString::Printf(TEXT("%s:%s"), *GetCurrentFilename(), *Diagnostic.ToString());
		if (Diagnostic.Severity == EVekaDiagnosticSeverity::Error)
		{
			Warn->Log(ELogVerbosity::Error, Line);
		}
		else
		{
			Warn->Log(ELogVerbosity::Warning, Line);
		}
	}
	if (Result.HasErrors())
	{
		// Never replace a working asset with a broken document.
		return nullptr;
	}
	if (!Result.ViewName.IsNone() && Result.ViewName != InName)
	{
		Warn->Log(ELogVerbosity::Warning, FText::Format(LOCTEXT("ViewNameMismatch", "{0}: the file declares view {1}, imported as {2}"),
			FText::FromString(GetCurrentFilename()), FText::FromName(Result.ViewName), FText::FromName(InName)).ToString());
	}

	UVekaView* View = NewObject<UVekaView>(InParent, InClass, InName, Flags);
	View->Document = Result.Document;
	Buffer = BufferEnd;
	return View;
}

FText UVekaViewImportFactory::GetDisplayName() const
{
	return LOCTEXT("VekaViewImportFactoryName", "Veka source (.vk)");
}

#undef LOCTEXT_NAMESPACE
