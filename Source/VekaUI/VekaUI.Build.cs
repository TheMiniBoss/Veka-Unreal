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
			// MediaView (2e).
			"MediaAssets",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			// Phase 4 spike: backdrop (B: our render-graph element; A: the engine's Slate post buffers).
			"VekaUIRender",
			"SlateRHIRenderer",
		});

		// Default font family (LT Flode Neue), loaded by Slate from these files.
		RuntimeDependencies.Add("$(PluginDir)/Resources/Fonts/LTFlodeNeue/*.otf", StagedFileType.UFS);
	}
}
