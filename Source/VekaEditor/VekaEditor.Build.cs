// Copyright TheMiniBoss. All Rights Reserved.

using UnrealBuildTool;

public class VekaEditor : ModuleRules
{
	public VekaEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"WorkspaceMenuStructure",
			"VekaUI",
			"VekaLang",
			// 2g: UVekaView factory and the sample Widget Blueprint.
			"UMG",
			"UMGEditor",
			"AssetTools",
			"AssetRegistry",
			"BlueprintGraph",
			"Kismet",
			// 3a: .vk import (tests read the plugin samples).
			"Projects",
			// 3b: asset editor, viewport, thumbnails.
			"AssetDefinition",
			"PropertyEditor",
			"ToolMenus",
			"InputCore",
			"RenderCore",
			// 3d: color picker in the inspector.
			"AppFramework",
		});
	}
}
