// Copyright TheMiniBoss. All Rights Reserved.

using UnrealBuildTool;

public class VekaUIRender : ModuleRules
{
	public VekaUIRender(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "SlateCore" });
		// Phase 4: backdrop capture and blur (global shaders, render graph, screen passes).
		PrivateDependencyModuleNames.AddRange(new string[] { "Projects", "RenderCore", "RHI", "Renderer" });
	}
}
