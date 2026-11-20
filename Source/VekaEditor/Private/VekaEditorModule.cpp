// Copyright TheMiniBoss. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Slate/SVekaHost.h"
#include "Samples/VekaPlayground.h"

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
	}

	virtual void ShutdownModule() override
	{
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PlaygroundTabName);
		}
	}

private:
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
