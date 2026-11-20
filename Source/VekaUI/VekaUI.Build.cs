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

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
		});

		// Default font family (LT Flode Neue), loaded by Slate from these files.
		RuntimeDependencies.Add("$(PluginDir)/Resources/Fonts/LTFlodeNeue/*.otf", StagedFileType.UFS);
	}
}
