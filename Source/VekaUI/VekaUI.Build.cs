// Copyright TheMiniBoss. All Rights Reserved.

using UnrealBuildTool;

public class VekaUI : ModuleRules
{
	public VekaUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UMG",
		});
	}
}
