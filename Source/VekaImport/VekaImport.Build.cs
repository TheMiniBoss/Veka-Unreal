// Copyright TheMiniBoss. All Rights Reserved.

using UnrealBuildTool;

public class VekaImport : ModuleRules
{
	public VekaImport(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "VekaUI" });
	}
}
