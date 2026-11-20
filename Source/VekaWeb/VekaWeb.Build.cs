// Copyright TheMiniBoss. All Rights Reserved.

using UnrealBuildTool;

public class VekaWeb : ModuleRules
{
	public VekaWeb(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Phase 1: empty. WebBrowser dependencies are added with the WebView component (Phase 2).
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "VekaUI" });
	}
}
