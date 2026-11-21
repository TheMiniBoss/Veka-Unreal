// Copyright TheMiniBoss. All Rights Reserved.

// Jalon 3a: importing a .vk file gives a Veka View asset.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "VekaViewFactory.h"
#include "VekaLanguage.h"
#include "View/VekaViewAsset.h"
#include "Samples/VekaSampleView.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVekaEditorImportTest, "VekaUI.Lang.Import", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVekaEditorImportTest::RunTest(const FString& Parameters)
{
	const FString Path = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("VekaUI"))->GetBaseDir(), TEXT("Samples"), TEXT("Veka"), TEXT("VV_Starter.vk"));
	FString Source;
	if (!TestTrue(TEXT("sample file loads"), FFileHelper::LoadFileToString(Source, *Path)))
	{
		return false;
	}

	UPackage* Package = CreatePackage(TEXT("/Temp/VekaImportTests"));
	UVekaViewImportFactory* Factory = NewObject<UVekaViewImportFactory>();
	TestTrue(TEXT("the factory takes .vk"), Factory->FactoryCanImport(TEXT("Starter.vk")));

	const TCHAR* Buffer = *Source;
	UObject* Imported = Factory->FactoryCreateText(UVekaView::StaticClass(), Package, TEXT("VV_Starter"), RF_Transient, nullptr, TEXT("vk"), Buffer, Buffer + Source.Len(), GWarn);
	UVekaView* View = Cast<UVekaView>(Imported);
	if (!TestNotNull(TEXT("imported view"), View))
	{
		return false;
	}
	FString Difference;
	TestTrue(FString::Printf(TEXT("same document as the sample (%s)"), *Difference), Veka::Lang::StructurallyEqual(View->Document, Veka::Samples::MakeStarterDocument(), &Difference));

	// A file with errors creates nothing (an existing asset is never replaced by a broken one).
	AddExpectedError(TEXT("Unknown name"), EAutomationExpectedErrorFlags::Contains, 1);
	const FString Broken = TEXT("view Broken {\n    Text(nope)\n}\n");
	const TCHAR* BrokenBuffer = *Broken;
	TestNull(TEXT("broken file not imported"), Factory->FactoryCreateText(UVekaView::StaticClass(), Package, TEXT("VV_Broken"), RF_Transient, nullptr, TEXT("vk"), BrokenBuffer,
		BrokenBuffer + Broken.Len(), GWarn));
	return true;
}

#endif
