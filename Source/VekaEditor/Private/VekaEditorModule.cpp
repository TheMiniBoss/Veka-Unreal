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
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_ComponentBoundEvent.h"
#include "EdGraphSchema_K2.h"
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

		auto FindOrCreateView = [&Folder](const TCHAR* Name)
		{
			UVekaView* Found = LoadObject<UVekaView>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Found)
			{
				UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *Folder, Name));
				Found = NewObject<UVekaView>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
				FAssetRegistryModule::AssetCreated(Found);
			}
			return Found;
		};
		UVekaView* Item = FindOrCreateView(TEXT("VV_ShopItem"));
		Item->Document = Veka::Samples::MakeShopItemDocument();
		Item->MarkPackageDirty();
		ToSave.Add(Item->GetPackage());
		UVekaView* View = FindOrCreateView(TEXT("VV_Shop"));
		View->Document = Veka::Samples::MakeShopDocument(Item);
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
		WireShopEvent(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Blueprint->MarkPackageDirty();
		ToSave.Add(Blueprint->GetPackage());

		UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
		UE_LOG(LogTemp, Display, TEXT("Veka.CreateSampleAssets: %s and %s saved"), *View->GetPathName(), *Blueprint->GetPathName());
	}

	/** Event graph: VekaHost.OnVekaEvent -> UVekaSampleLibrary::HandleShopEvent(VekaHost, EventName, Payload). */
	static void WireShopEvent(UWidgetBlueprint* Blueprint)
	{
		const FName HostName(TEXT("VekaHost"));
		const FName EventName(TEXT("OnVekaEvent"));
		if (FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, EventName, HostName))
		{
			return;
		}
		FObjectProperty* HostProperty = FindFProperty<FObjectProperty>(Blueprint->SkeletonGeneratedClass, HostName);
		UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
		if (!HostProperty || !Graph)
		{
			UE_LOG(LogTemp, Error, TEXT("Veka.CreateSampleAssets: no VekaHost variable or event graph"));
			return;
		}
		FKismetEditorUtilities::CreateNewBoundEventForClass(UVekaHostWidget::StaticClass(), EventName, Blueprint, HostProperty);
		UK2Node_ComponentBoundEvent* Event = const_cast<UK2Node_ComponentBoundEvent*>(FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, EventName, HostName));
		if (!Event)
		{
			return;
		}
		Event->NodePosX = 0;
		Event->NodePosY = 0;

		UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph);
		Call->SetFromFunction(UVekaSampleLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UVekaSampleLibrary, HandleShopEvent)));
		Call->NodePosX = 420;
		Call->NodePosY = 0;
		Graph->AddNode(Call, false, false);
		Call->CreateNewGuid();
		Call->PostPlacedNewNode();
		Call->AllocateDefaultPins();

		UK2Node_VariableGet* Get = NewObject<UK2Node_VariableGet>(Graph);
		Get->VariableReference.SetSelfMember(HostName);
		Get->NodePosX = 180;
		Get->NodePosY = 160;
		Graph->AddNode(Get, false, false);
		Get->CreateNewGuid();
		Get->PostPlacedNewNode();
		Get->AllocateDefaultPins();

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		auto Connect = [Schema](UEdGraphPin* A, UEdGraphPin* B)
		{
			if (!A || !B || !Schema->TryCreateConnection(A, B))
			{
				UE_LOG(LogTemp, Error, TEXT("Veka.CreateSampleAssets: could not connect %s to %s"), A ? *A->PinName.ToString() : TEXT("?"), B ? *B->PinName.ToString() : TEXT("?"));
			}
		};
		Connect(Event->FindPin(UEdGraphSchema_K2::PN_Then), Call->GetExecPin());
		Connect(Get->FindPin(HostName), Call->FindPin(TEXT("Host")));
		Connect(Event->FindPin(TEXT("EventName")), Call->FindPin(TEXT("EventName")));
		Connect(Event->FindPin(TEXT("Payload")), Call->FindPin(TEXT("Payload")));
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
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
