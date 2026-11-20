// Copyright TheMiniBoss. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

/**
 * Loaded at PostConfigInit so the plugin shader directory is mapped before shaders compile.
 * Phase 4 adds the backdrop capture, blur pyramid and glass passes here.
 */
class FVekaUIRenderModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VekaUI")))
		{
			const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
			if (FPaths::DirectoryExists(ShaderDir))
			{
				AddShaderSourceDirectoryMapping(TEXT("/Plugin/VekaUI"), ShaderDir);
			}
		}
	}
};

IMPLEMENT_MODULE(FVekaUIRenderModule, VekaUIRender)
