// Copyright TheMiniBoss. All Rights Reserved.

using UnrealBuildTool;

public class VekaUIRender : ModuleRules
{
	public VekaUIRender(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Projects", "RenderCore" });
	}
}
