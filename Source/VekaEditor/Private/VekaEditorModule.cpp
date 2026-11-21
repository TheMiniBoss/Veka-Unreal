// Copyright TheMiniBoss. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Slate/SVekaHost.h"
#include "Samples/VekaPlayground.h"
#include "Samples/VekaSampleView.h"
#include "View/VekaViewAsset.h"
#include "UMG/VekaHostWidget.h"
#include "HAL/IConsoleManager.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "VekaEditor"

/**
 * Phase 1: a "VekaUI Playground" nomad tab (Window > Tools) that hosts the C++ playground view,
 * to check rendering and input inside the editor. Phase 3 adds the UVekaView asset editor and the Hub.
 */
class FVekaEditorModule : public IModuleInterface
{
public:
	static const FName PlaygroundTabName;

	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PlaygroundTabName, FOnSpawnTab::CreateRaw(this, &FVekaEditorModule::SpawnPlayground))
			.SetDisplayName(LOCTEXT("PlaygroundTabTitle", "VekaUI Playground"))
			.SetTooltipText(LOCTEXT("PlaygroundTabTooltip", "Phase 1 test view for the VekaUI runtime."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

		SampleCommand = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Veka.CreateSampleAssets"),
			TEXT("Creates /Game/VekaSamples/VV_Shop (UVekaView) and WBP_VekaShop (a Widget Blueprint with a Veka Host showing it)."),
			FConsoleCommandDelegate::CreateStatic(&FVekaEditorModule::CreateSampleAssets));
	}

	virtual void ShutdownModule() override
	{
		if (SampleCommand)
		{
			IConsoleManager::Get().UnregisterConsoleObject(SampleCommand);
			SampleCommand = nullptr;
		}
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PlaygroundTabName);
		}
	}

	/** Sample asset view and a Widget Blueprint that displays it (with the sample view model). */
	static void CreateSampleAssets()
	{
		const FString Folder = TEXT("/Game/VekaSamples");
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		TArray<UPackage*> ToSave;

		UVekaView* View = LoadObject<UVekaView>(nullptr, *(Folder + TEXT("/VV_Shop.VV_Shop")), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!View)
		{
			UPackage* Package = CreatePackage(*(Folder + TEXT("/VV_Shop")));
			View = NewObject<UVekaView>(Package, TEXT("VV_Shop"), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(View);
		}
		View->Document = Veka::Samples::MakeShopDocument();
		View->MarkPackageDirty();
		ToSave.Add(View->GetPackage());

		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *(Folder + TEXT("/WBP_VekaShop.WBP_VekaShop")), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Blueprint)
		{
			UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
			Factory->ParentClass = UUserWidget::StaticClass();
			Blueprint = Cast<UWidgetBlueprint>(AssetTools.CreateAsset(TEXT("WBP_VekaShop"), Folder, UWidgetBlueprint::StaticClass(), Factory));
		}
		if (!Blueprint)
		{
			UE_LOG(LogTemp, Error, TEXT("Veka.CreateSampleAssets: could not create the Widget Blueprint"));
			return;
		}
		Blueprint->Modify();
		UVekaHostWidget* Host = Cast<UVekaHostWidget>(Blueprint->WidgetTree->FindWidget(TEXT("VekaHost")));
		if (!Host)
		{
			Host = Blueprint->WidgetTree->ConstructWidget<UVekaHostWidget>(UVekaHostWidget::StaticClass(), TEXT("VekaHost"));
			Blueprint->WidgetTree->RootWidget = Host;
		}
		Host->bIsVariable = true;
		Host->View = View;
		Host->ViewModel = NewObject<UVekaSampleViewModel>(Host, NAME_None, RF_Transactional);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Blueprint->MarkPackageDirty();
		ToSave.Add(Blueprint->GetPackage());

		UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
		UE_LOG(LogTemp, Display, TEXT("Veka.CreateSampleAssets: %s and %s saved"), *View->GetPathName(), *Blueprint->GetPathName());
	}

private:
	IConsoleObject* SampleCommand = nullptr;

	TSharedRef<SDockTab> SpawnPlayground(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SVekaHost)
				.Root(Veka::Samples::Playground())
			];
	}
};

const FName FVekaEditorModule::PlaygroundTabName(TEXT("VekaUIPlayground"));

IMPLEMENT_MODULE(FVekaEditorModule, VekaEditor)

#undef LOCTEXT_NAMESPACE
