// Copyright TheMiniBoss. All Rights Reserved.

using UnrealBuildTool;

public class VekaImport : ModuleRules
{
	public VekaImport(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "VekaUI" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Projects" });

		// Glyph outlines for vector text and symbols (spike 2c).
		AddEngineThirdPartyPrivateStaticDependencies(Target, "FreeType2", "UElibPNG", "zlib");
	}
}
