// Copyright TheMiniBoss. All Rights Reserved.

using UnrealBuildTool;

public class VekaLang : ModuleRules
{
	public VekaLang(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "VekaUI" });
		PrivateDependencyModuleNames.AddRange(new string[] { "AssetRegistry", "Projects" });
	}
}
