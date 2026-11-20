// Copyright TheMiniBoss. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "VekaUILog.h"
#include "Core/VekaSchema.h"

DEFINE_LOG_CATEGORY(LogVekaUI);

class FVekaUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// The schema is built lazily on first use (all compiled-in structs are registered by then).
	}
};

IMPLEMENT_MODULE(FVekaUIModule, VekaUI)
