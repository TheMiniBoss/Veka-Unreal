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
		});
	}
}
